# fw 0.33 / build 10051 (2026-10-07)

Builds on 0.32 / 10050 (see `ADC_TRIGGER_FIXES.md`). Bench work and measurements: `TRIGGER_EXPERIMENTS.md`
and the instrumentation repo (`docs/2026-10-07_detector13_spike_interference.md`).

## Impulse filter, on by default (mode 3, "jump-hold")
Detector 13 showed a steady train of single-sample negative spikes (~30 kHz, switching-converter edges, most
likely the LCD backlight driver), which kept it triggering ~2/s even with a server trigger offset of 75. Other
units (e.g. 15) have the same spikes at a much lower rate. In the trigger scan only (the samples sent to the
server stay raw), a jump of more than `despike_k` (60 counts) from the last accepted value is held for
`despike_n` (3) samples and then the settled level is accepted: a spike and its ringing vanish, a real edge is
seen ~1 us later. Bench (detector 15, calibrated detector-13 replay, AGC on): spike false triggers cut 85-100%,
no measured loss of clean-stroke sensitivity, ADC ISR +~9% (about 46% avg / 61% peak). Console Ctrl-F cycles
0 off / 1 median / 2 median+blank / 3 jump-hold; status field `f=`.

## LCD
- TRIGGER overlay and tone only for local strikes: the ISR measures each triggered buffer's peak deviation from the
  baseline (triggered buffers only) and alerts at `alert_mv` (default 100 mV at the PGA input). Every trigger
  is still counted and sent.
- Backlight: the brightness slider now sets the idle (dim) level, rescaled from its ~15-100 travel to 1-100 and
  stored in the LCD (`dims`, read back at boot; the factory 100 counts as unset -> 24). Alerts, touches, page
  changes, LCD restart and boot go bright (idle + 66, max 100) for 10 s, then back to idle.

## Remote settings
One table of server-settable variables (key, type, range, log-on-change) with exact `key:value` matching. That
fixes partial key matches and the poll-interval range check. Existing keys are unchanged; new keys work with
today's server (absent = firmware default):
`al` alert level mV (1-10000), `dsp` impulse filter mode (0-3), `dsk` filter threshold (1-4095),
`dsn` filter hold (0-20).

## Also from the experiment branch
- STA/LTA detector, off by default (console Ctrl-T; status `d=`, `r=`).
- The console status line is every 60 s (`CONSOLE_STATUS_SECS`; 1 s on the bench).

## Deployment status (2026-10-07)
- Images A (CRC 0x0e4eebe1) and I (0x05fa46f2) published on b2 as my12-{A,I}{11,22}-10051.bin; detectorsrv9
  advertises 10051. Detector 15 updated over the air and boots 10051 from bank A.
- Quick soak inconclusive: it ran straight after the OTA reboot, likely before GPS lock. Rerun before merging to master.

## 10052 (same day): LCD reload loop fix
10051 read the LCD's dims straight after its sys0 (LCD build) at boot. Both reads accepted any LCD packet as the
answer, and the dims read cleared the receive buffer, so a slow LCD's sys0 reply was lost. lcd_sys0 then stayed
-1, the unit reflashed the LCD and rebooted it again at every 15-minute poll (detector 18: "LCD server build
10036, lcd has -1"). Now each get waits up to 500 ms for its own 0x71 reply, an unrequested reply is ignored,
and lcdupneeded() asks for sys0 again (and skips the update) when it's still unknown.
Images: A CRC 0xb0d7ebce, I CRC 0xd05dd52b (305056 bytes each).
Stopgap on the server for units still on 10051: lbl:0 turns off the LCD build check.

## 10053 (same day, deployed): alert check out of the ADC ISR
The local-strike min/max pass now runs in the UDP send task on the queued sample copy, so alerts need GPS lock.
Images: A CRC 0x8546c64f, I CRC 0xb98c4c0c (305136 bytes). Includes the 10052 LCD fix. Bench: detection and
filter unchanged, alert verified. Open: occasional mode-3 ISR peak of 75% and late dropped trigger buffers (`lt`).

## Known open items
lwIP raw API calls without the core lock (`sendudp()`, `www.c` client) are still to be fixed, and so is the heap-check
latency (see `TRIGGER_EXPERIMENTS.md`).
