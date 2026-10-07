# Detector console commands (my12, from build 10055)

The console is USART2 at 115200 8N1. On a bench unit that's the ST-Link virtual COM port (COM7 on Bob's PC).
Commands are single control keys, with no Enter. Each key prints a one-line confirmation. Any other key is echoed back.

## Keys

| Key | Action | Confirmation |
|---|---|---|
| Ctrl-C | AGC on/off toggle. Off holds the current gain until a digit sets a new one. | `AGC is ON` / `AGC is OFF` |
| 0-9 | With AGC off: set the PGA gain step (0 lowest, 9 highest). With AGC on they're only echoed. | `Manually setting PGA gain to N` |
| Ctrl-D | Status dump: an `ID:` line, 12 trigger-threshold lines, and the firmware/build/PCB line. | |
| Ctrl-E | Status line (`S ...`, below) off → every 60 s → every 1 s → off. **Off at boot.** | `Status line every N s` / `Status line off` |
| Ctrl-F | Impulse (spike) filter mode: 0 off → 1 median → 2 median+blank → 3 jump-hold → 0. Boots in **3**. **Also clears any UDP freeze**, including Ctrl-G's. | `UDP freeze is off`, then `Despike filter mode N (...)` |
| Ctrl-G | Fake GPS lock + UDP freeze on/off (bench use). | `Fake GPS lock and UDP freeze is ON/OFF` |
| Ctrl-R | Reboot. | `Reboot` |
| Ctrl-T | Trigger detector: edge (default) ↔ STA/LTA (experimental). | `Detector is EDGE` / `Detector is STA/LTA` |

Avoid Ctrl-S/Ctrl-Q in terminal programs that use XON/XOFF; the firmware doesn't use them.

Settings changed from the console (AGC, gain, filter mode, detector, status line) are not stored: a reboot restores the defaults.
They can be overridden from the server (below). The `ID:` line still prints every 30 s regardless of Ctrl-E.

## Status line (Ctrl-E)

`S 307 tr=368 th=2 pt=1 g=9 nz=12 nm=3186 ov=0 lt=0 jb=0 ss=0 isr=35/45% d=0 r=0 f=3 lat=13 sk=0`

| Field | Meaning |
|---|---|
| `S` | Seconds since boot |
| `tr` | Triggered buffers since boot |
| `th` / `pt` | Trigger threshold / pre-trigger (near-miss) threshold, ADC counts |
| `g` | PGA gain step |
| `nz` | Noise estimate |
| `nm` | Near-misses since the last line |
| `ov` | Triggered buffers dropped because the UDP send queue was full (since boot) |
| `lt` | Triggered buffers dropped because the scan copy came too late (since boot) |
| `jb` | Jabber (trigger-storm) count |
| `ss` | Trigger-suppression countdown |
| `isr` | ADC scan interrupt load since the last line, average/peak % of one buffer's time (270 µs) |
| `d` / `r` | Detector (0 edge, 1 STA/LTA) / STA/LTA peak ratio ×16 |
| `f` | Impulse filter mode |
| `lat` | Peak delay from DMA buffer complete to the scan interrupt, µs, since the last line (normally ~15) |
| `sk` | ADC buffers never scanned since boot (should stay 0) |

In normal running `ov`, `lt` and `sk` should stay at 0. Rising values mean triggers are being lost.

## Server (remote) settings

detectorsrv9 can send these as `key:value` in its poll reply. A missing key leaves the firmware default. Out-of-range values are
ignored.

| Key | Setting | Range | Default |
|---|---|---|---|
| `tt` | Trigger level modifier (added to the threshold) | 0-4049 | 0 |
| `pt` | Server poll interval, seconds | 1-900 | 900 |
| `al` | LCD TRIGGER overlay / tone level, mV at the PGA input | 1-10000 | 100 |
| `dsp` | Impulse filter mode (as Ctrl-F) | 0-3 | 3 |
| `dsk` | Impulse filter jump threshold, ADC counts | 1-4095 | 60 |
| `dsn` | Impulse filter hold, samples after a spike | 0-20 | 3 |
| `bld`, `fw`, `crc1`, `crc2`, `srv` | Advertised firmware build, file prefix, A/I image CRCs, loader host | | |
| `lbl`, `lcd`, `siz` | Advertised LCD build (0 = don't check), LCD file, size | | |

`tt`, `pt`, `al`, `dsp`, `dsk`, `dsn` and `srv` print `Server -> ...` on the console when they change. If the advertised build differs from the running one, the unit downloads
the image for its other bank and reboots into it, so never flash a build the server isn't advertising.
