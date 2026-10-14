# Release 0.34 / build 10060 (2026-10-14)

Branch `feature/dual-trigger` (from master 10059 `dc16c22`). Brings the detection work of the `experiment/stalta-filter`
builds 10056-10058 into the release line, reworked so that the two trigger detectors are enabled independently.

## What changed since 10059
- **Two trigger detectors, independently enabled** (`trigmask`: 1 edge, 2 energy, 3 both). A buffer is sent if any
  enabled detector fires. **Default edge only**, which behaves exactly as 10059 (same gentle back-off and AGC path).
  - **Edge:** the existing 32-sample-window detector.
  - **Energy (STA/LTA):**
    - 32-sample STA over a 128-buffer LTA;
    - adaptive ratio: high-percentile idle track × margin (default 1.5), clamped 2.0-10.0, no learning for 64
      buffers after a trigger;
    - trigger-rate feedback.
    - Fixed at compile time except the margin.
- **Impulse filter (mode 3) feeds both detectors**; before, it fed only the edge detector.
- **Energy triggers don't feed the edge threshold or its near-miss AGC** (the 10057 detector-18 gain collapse).
- **Console Ctrl-T** cycles edge → energy → edge + energy and prints `Triggers enabled: ...`.
- **Server key `tr:<m>[<nn>]`:** `m` 1/2/3; optional two-digit energy margin in tenths, 10-40. Invalid values are logged
  and ignored, and an absent key leaves the setting unchanged.
  The experimental keys `det`, `slk`, `slr`, `slm` and `sls` are gone. See `DETECTION_MODES.md` and
  `DETECTORSRV10_BRIEF_10060.md`.
- **Status line:** `d=` is the enabled set (1/2/3), plus `r=` (energy peak ratio ×16), `ra=` (ratio in use ×16),
  `so=` / `eo=` (energy-only / edge-only triggered buffers when both are on), alongside 10059's `bo=` `bl=`.

## Bench test (detector 15, generator replay, gain 7 fixed; lowest real stroke caught >= 3/4, mV; `tests/trigger_check.py`)

| enabled | clean m06 / m10 | detector-13 spikes | noise 15 mV | ISR avg / peak |
|---|---|---|---|---|
| edge | 80 / 120 | 80 / 120 | 80 / 120 | 47 / 53 % |
| energy | **12 / 20** | 50 / 120 | 80 / 200 | 22 / 31 % |
| both | **12 / 50** | 50 / 50 | 80 / 200 (8 strokes/level: 120) | 61 / 68 % |

False triggers 0-0.07 triggered buffers/s with no strokes. The m10-in-noise point sits between 120 and 200 for both
settings (it reversed on a repeat). The `tr` decoder was tested on the PC against valid and invalid values.

## Images (my12 root, build 10060)

| File | Bytes | CRC | Server field |
|---|---|---|---|
| `my12-A22-10060.bin`, `my12-A11-10060.bin` | 307072 | `0xa61c082b` | crc1 |
| `my12-I22-10060.bin`, `my12-I11-10060.bin` | 307072 | `0xbae29194` | crc2 |
| `my12-A21-10060.bin` | 307192 | `0xfd8bcc08` | crc1 |
| `my12-I21-10060.bin` | 307192 | `0xdfda44d3` | crc2 |

## Deployment
Publish through detectorsrv10 and set `target_build` per detector. Without `tr` from the server the detectors run edge
only, exactly as 10059. Suggested first unit: detector 18 (local, replacing the 10058 experiment), then enable energy
there with `tr:3` (console Ctrl-T from the Pi until detectorsrv10 sends `tr`).

## Not tested yet
Energy / both on located strikes in the field; `tr` from the real server; PCB 21 image on detector 1.
