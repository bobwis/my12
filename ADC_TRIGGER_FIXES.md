# ADC trigger path fixes - fw 0.32, build 10050 (2026-10-06)

Bug fixes and ADC ISR optimizations found during the trigger-detection bench
experiments (branch `experiment/trigger-detection`, log in
`TRIGGER_EXPERIMENTS.md` on that branch). Detection behaviour is otherwise
unchanged: same edge detector, same thresholds, same AGC rules. The STA/LTA
detector from those experiments is NOT in this build.

Bench rig: detector 15, Junctek PSG9000 replaying a recorded stroke once per
second, ADC measured at 2.70 MSps (728 samples = 270 us = 58240 CPU cycles per
buffer at 216 MHz).

## 1. AGC lockout (detector deaf indefinitely)

The near-miss (pre-trigger) test reduced algebraically to a fixed "per-sample
rise > 2", independent of `trigthresh`. Raising the threshold never reduced
the near-miss count the 100 ms AGC regulates, so on a noisy input the AGC
pinned `trigthresh` at its 100 clamp, the clamp stepped PGA gain down to 0,
and the detector stayed deaf. Reproduced on the bench (no triggers for >10 min
with a 1 Hz stimulus); also seen on live detectors.

Fix: a near miss is now `abs(meanwindiff) > lastmeanwindiff + pretrigthresh +
trigcomp`, i.e. a 2-unit band just below the trigger. Bench: the threshold
settles at 2-6 instead of 100.

## 2. Wrong or torn buffers sent

`startudp()` copied whichever DMA buffer was current when the task got to run.
With the ISR using most of the CPU (item 3) the task was routinely more than
one buffer late, so ~40% of sent "events" lacked the waveform that triggered
them, and copies could be overwritten by DMA mid-copy.

Fix: `enqueue_sample_isr()` copies the triggering buffer into a send-queue slot
from inside `ADC_Conv_complete()`, while DMA fills the other half. A DMA
completion counter (`adcbufseq`) is re-checked after the copy; if DMA has
moved on the sample is dropped and counted (`trigbuflate`, console `lt=`)
rather than sent torn. Slot reservation and packet numbering are split so a
dropped sample leaves no gap in `udppknum`. Sample packets are only queued
once the send path is armed (`samplesarmed`, set at "Arming") and under the
same GPS-lock / jabber / freeze conditions as before.

Knock-on fix found in the 10050 soak: the UDP stall watchdog runs in `startudp()`,
which the ISR now wakes *after* queuing the sample, so the watchdog sees a
just-queued item before the (lower priority) sender has run. Its "queue not
draining" test used time since the last send, so the first trigger after
30 s of quiet (once past the 600 s arming time) rebooted the detector -
twice in the soak, at ~460 packets each time. The test now uses
`sq_alive_sec`, refreshed every time the sender's queue wait returns
(item or 1 s timeout), so it still catches a blocked or starved sender.

FreeRTOS `FromISR` calls are legal here: the TIM5 proxy IRQ runs at priority 5,
below `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (3); the tasks touch
`sq_head` / `udppknum` under `taskENTER_CRITICAL`, which masks it.

## 3. ADC ISR load 83% -> ~40% of each buffer period

Measured with the DWT cycle counter (`isrcyc_*`; the M7 DWT needs `LAR`
unlocked before `CTRL` writes take effect). D-cache is off, so every SRAM1
access in the 728-sample loop is slow.

| Change | ISR avg / peak |
|---|---|
| master (0.31) | 83% / 89% |
| DMA double buffer static in DTCM (was heap, drifted into SRAM1) | 75% / 81% |
| scan loop state in locals, thresholds read once per buffer, globals written back once | 38% / 43-47% |

`startadc()` prints a warning if the DMA buffer ever lands outside DTCM
(>= 0x20020000).

## 4. ENDSEQ late and with the wrong batch id

`sendendstatus` was set in the first quiet buffer after a batch, but the ISR
only woke `startudp()` on a trigger, so the end-of-sequence status waited for
the NEXT batch's first trigger (seconds to minutes) and carried that batch's
id. The ISR now also notifies on the buffer where a batch ends.

On the wire (79 batches): ENDSEQ arrives 0-9 ms after the batch's last sample
and `adcpktssent` matched in 77/79. Its batch id is the event's own id. Sample
packet headers still carry id - 1 (header written before `adcbatchid`
increments - unchanged, pre-existing). lightsrv5 uses the ENDSEQ batch id only
for the status JSON `batchid` field and logs, not for pairing with samples.

## 5. Console

- USART2 receive is re-armed after UART errors (`HAL_UART_ErrorCallback`) and
  from the LP loop whenever it is found idle - console keys (Ctrl-D, Ctrl-C
  AGC toggle, digit = gain) had stopped working.
- Compact status line every `CONSOLE_STATUS_SECS` (version.h, 60 s in this
  build, 0 = off, independent of `TESTING`):
  `S <t1sec> tr= th= pt= g= nz= nm= ov= lt= jb= ss= isr=<avg>/<peak>%`
  - triggers, threshold, pre-trigger threshold, gain, noise, near misses since
  the last line, adc->udp overruns, late/torn ISR copies, jabber count,
  suppression countdown, ADC ISR load since the last line.

## Known residuals

- `lt` (late/torn copies dropped) still increments occasionally on the bench:
  ~2 per 90 s, once 9 in ~5 min.
- With the lockout fixed the AGC equilibrates on near misses, not trigger
  rate, so a noisy site can see ~0.5 noise triggers/s. A rate-based AGC is a
  candidate for the experiment branch.
