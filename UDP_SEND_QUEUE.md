# UDP send queue redesign (2026-09-23)

Branch: `feature/decoupled-udp-send-queue`. Builds on the UDP checksum fix
(see `git log` around "Fix UDP checksum corruption on fragmented sample
packets") - that fix made outgoing sample packets reach the network
correctly for the first time in a long while, which is what surfaced the
problems documented here: the send path underneath it hadn't been
exercised at full burst rate before.

## The problem

The original send queue (a 64-slot ring, one task) opportunistically called
its drain step inline, right after every `enqueue_sample()`, from inside
`startudp()`'s ADC-notify branch. That reintroduced the exact
variable-latency-in-the-hot-path problem the queue was built to avoid: the
`sendudp()` call chain (UDP checksum generation, IP fragmentation, HAL
Ethernet TX) is not constant-time, and running it synchronously in the same
task that services ADC trigger notifications meant a slow send could delay
the task's return to `ulTaskNotifyTake()` - observed on real hardware as
ADC overruns during a fast trigger burst.

Two smaller issues rode along with the same root cause:
- End-of-sequence and timed status packets were sent directly, out of band
  from the sample queue, so they could arrive out of order relative to the
  samples around them.
- With the opportunistic drain removed, the *only* remaining drain point
  was the notify-timeout branch (once per up-to-1000ms idle period), which
  drained exactly one packet per call - a full 64-deep queue could take
  close to a minute to flush after a burst ended.

## The fix: two tasks, one ring

**Producer** (`startudp()`, unchanged priority - `osPriorityNormal`,
`defaultTask`): on an ADC notification, does a fixed-size `memcpy` into the
next free ring slot and pushes a small descriptor. No lwIP call, no
network-timing dependency, ever. This is the only thing that runs on the
ADC-critical path.

**Sender** (`netsendtask()`, new, `osPriorityBelowNormal` - one level below
the producer): owns every `sendudp()` call in the firmware. Blocks on the
descriptor queue (zero CPU while idle) and drains as fast as the network
stack/hardware allow, not gated to one packet per producer-loop-timeout.
Because it is strictly lower priority than the producer, the scheduler
guarantees it can never delay the ADC path - a new trigger notification
preempts it unconditionally, even mid-send.

**Everything shares one ring.** Sample packets, end-of-sequence status, and
timed status (`enqueue_sample()` / `enqueue_status()`) all go through the
same 96-slot ring in production order, so wire ordering is automatic and
there is exactly one task calling into lwIP's raw API - see "Deferred"
below for why that matters.

**Slot reuse safety** uses a counting semaphore (`freeslots`), not the
descriptor queue's own occupancy. A descriptor is popped off the queue the
moment `netsendtask()` wakes, but the physical slot isn't safe for the
producer to overwrite until `sendudp()` has actually been called for it -
`freeslots` is given back at that point, matching the timing the original
single-task design's `sq_count` accounting relied on. Freeing the slot
merely on dequeue (the more obvious approach) would have widened the
window where the producer could overwrite a slot lwIP/hardware might still
be reading out of.

**Timed status is still only ever evaluated while the ADC side is idle** -
never mid-burst, since the end-of-sequence status packet already carries
the same fields then and a timed status too would be a redundant
duplicate. *Where* that idle-time check lives, and exactly what "idle
enough" means, changed in a follow-up - see "Timed status trigger
relocation" below.

## RAM budget

Queue depth: 64 -> 96 slots (`SEND_QUEUE_DEPTH` in `udpstream.c`), chosen
as a comfortable multiple of the ~50-packet bursts this system produces
while leaving meaningful headroom for other future work - deliberately not
maxed out against available RAM. At 1472 bytes/slot that's ~141KB;
confirmed via a real build (`bss=465048`, `data=2540`) that ~57KB of RAM
still remains free out of the 512KB total.

Increasing the queue also raised a second, easy-to-miss cost: each slot
needs a `struct pbuf` header from lwIP's separate `MEMP_NUM_PBUF` pool, not
just the byte-array RAM. This pool's budget wasn't re-checked when the
queue grew, and hit its limit on first hardware boot after the change
(`startudp: sendqueue pbuf 94 alloc failed!`, followed by a reboot loop) -
`MEMP_NUM_PBUF` sized 96 was exactly enough for the *old* 64-slot design
(`p1 + p2 + ps + 64 = 67`, with margin) but one short of the new design's
minimum (`p1 + p2 + 96 = 98`, `ps` having been retired). Fixed by raising
it to 128. Confirmed via a real build that ~53.5KB of RAM remains free.

`my12.ioc`'s own `LWIP.MEMP_NUM_PBUF` and `LWIP.CHECKSUM_GEN_UDP` values
were updated to match what's actually in `lwipopts.h`, so CubeMX doesn't
show stale values if the project is ever reopened there. The new
`netsendtask()` thread and `configUSE_COUNTING_SEMAPHORES`
(`FreeRTOSConfig.h`) were deliberately *not* added to the `.ioc` - see the
commit message for "Sync .ioc LwIP parameters..." for why.

## Timed status trigger relocation (follow-up)

Branch: `feature/timed-status-in-sender`, on top of the above.

The original timed-status check fired on a fixed `t1sec % STAT_TIME`
grid, evaluated only from `startudp()`'s notify-timeout branch. That left
a real, if low-probability (~1-in-120 per burst end, at production's
`STAT_TIME`=120s), gap: if a burst happened to end just before a grid
boundary, the first idle check afterward could land almost exactly on
that boundary, firing a timed status only ~1 second after the
end-of-sequence status that had just closed the same burst - two status
packets carrying near-identical fields, back to back.

Fixed by changing what "due" means, and where it's decided:

- **What**: instead of an absolute `t1sec % STAT_TIME` grid, the trigger
  is now "has `STAT_TIME` seconds passed since the last status packet of
  *any* kind (end-of-sequence or timed)". `netsendtask()` tracks
  `laststatussecond`, updated right after it dispatches any status-type
  packet - not just timed ones. This makes the near-simultaneous-pair
  scenario structurally impossible: a timed status can no longer follow
  an end-of-sequence one by less than a full `STAT_TIME`. Deliberately
  gives up landing on clean `STAT_TIME` multiples to get this - confirmed
  with the user that alignment to a clean grid was never actually needed.
- **Where**: the whole check moved off the ADC-facing producer and into
  `netsendtask()` itself. Unlike end-of-sequence status - which stays
  exactly where it was, fired synchronously off the real ADC batch-end
  notification, since moving it would add latency for no CPU benefit -
  timed status was never tied to any ADC event, only to elapsed time, so
  there was nothing to lose by relocating it off the ADC-critical task.
  `netsendtask()`'s queue wait changed from `portMAX_DELAY` to a 1-second
  bound so it can notice a due timed status with zero ADC activity -
  still effectively idle (zero CPU) the rest of the time it's waiting.

**This gave `enqueue_status()` a second caller** (`netsendtask()` itself,
for timed status, alongside `startudp()`'s existing end-of-sequence call),
meaning `sq_head` and `statuspkt.udppknum` - previously updated by exactly
one task by construction - now have two possible writers. Fixed with
`reserve_queue_slot()`: grabs a slot index and a packet sequence number
together under one `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()` section,
deliberately scoped to just those two integer read-modify-writes and
*not* the payload memcpy that follows using the reserved values - once a
slot is reserved, no other caller can pick it again until its descriptor
is sent and `freeslots` is given back, so the memcpy needs no protection
of its own. The critical section's cost is fixed (a handful of
instructions), never proportional to packet size, regardless of whether
a sample or a status packet triggered it - the whole point being to never
put a payload-sized interrupts-disabled window in front of the ADC ISR,
the same concern that shaped the rest of this design.

`startudp()`'s notify-timeout branch is now empty (timed status no longer
lives there, and nothing else ever did). Left as a 1000ms bounded wait
rather than switched to `portMAX_DELAY`, since making the producer purely
event-driven is a separate decision that hasn't been made.

Verified on real hardware: confirmed working.

## Deferred, not forgotten

- **`LOCK_TCPIP_CORE()`/`UNLOCK_TCPIP_CORE()`**: with
  `LWIP_TCPIP_CORE_LOCKING=1`, raw API calls like `udp_sendto()` made off
  the `tcpip_thread` are technically supposed to be wrapped in these. This
  has always worked without them because there's only ever been one task
  calling into lwIP's raw API - a real but implicit safety property. This
  redesign is careful to preserve that property (only `netsendtask()` calls
  `sendudp()` now), so explicit locking was deliberately left out for this
  pass. Revisit if that single-caller assumption ever stops holding - e.g.
  if the currently-commented-out `myudp_recv()` UDP-receive callback in
  `udpstream.c` is ever re-enabled, since lwIP invokes that from
  `tcpip_thread`'s own context, not the sender task's.
- **Event-driven pbuf-ready wait**: `netsendtask()` currently retries with
  a bounded `vTaskDelay(1)` when a slot's pbuf hasn't been released by
  hardware yet, rather than blocking on something `HAL_ETH_TxFreeCallback`
  signals directly. Fine for now (this task is never on the ADC-critical
  path, so the retry costs nothing there) - worth revisiting only if it
  ever shows up as an actual bottleneck.

## Status

Verified on real hardware: builds clean, boots, sample packets confirmed
received in order by the PC-side monitor (Captofile), `adcudpover` stays
at 0 across trigger bursts. Not yet stress-tested against the largest
bursts this system can produce - worth keeping an eye on `adcudpover` and
queue drain time as it sees more real-world lightning activity.
