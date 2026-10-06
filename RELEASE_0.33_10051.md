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

## Known open items
lwIP raw API calls without the core lock (`sendudp()`, `www.c` client) are still to be fixed, and so is the heap-check
latency (see `TRIGGER_EXPERIMENTS.md`).
