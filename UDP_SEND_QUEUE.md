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

**Timed status logic is unchanged on purpose.** It's still only evaluated
from the notify-timeout branch (i.e. only once the ADC side has been idle).
This was checked with the user and confirmed as intentional, not a bug:
during a trigger burst the end-of-sequence status packet already carries
the same fields, so a timed status mid-burst would be a redundant
duplicate.

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
