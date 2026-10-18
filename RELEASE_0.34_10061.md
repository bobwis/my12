# Release 0.34 / build 10061 (2026-10-18)

Branch `feature/trigger-fields` (from master 10060 `197ef59`). Detection is unchanged from 10060; the packets now say
which trigger detector fired and how the energy detector is set.

## What changed since 10060
- **Sample packet byte 4 bits 1..0 = trigger source** of that buffer: 1 edge, 2 energy, 3 both. Replaces the unused
  `adcbufnum & 3` counter (removed). Bits 7..2 are still `rtseconds`, so lightsrv5's `calcrt` is unaffected.
  The ISR ORs the bits into the buffer just before queueing it.
- **Status `reserved2`**, composed into the queued copy (the live GPS/PPS bits 0-1 are untouched):
  bits 3..2 enabled set, 15..8 energy ratio in use ×16, 23..16 energy margin ×10.
- Layout and decoding examples: `PACKET_TRIGGER_FIELDS.md`. Controller guidance: `DETECTORSRV10_BRIEF_10061.md`.
- Consumers must check `firmwarever >= 10061` before using the fields. Existing consumers mask bits 0-1 of
  `reserved2` and ignore the low bits of byte 4, so they are unaffected.

## Bench test (detector 15, test image reporting 10060, generator CH1 only, gain 7 fixed)
- **Fields** (`instrumentation/tests/trigfield_check.py`, m06 strokes, UDP captured on the PC):
  edge → source 1 only; energy → source 2 only; both at 200 mV → 3 (plus 2 on a batch tail), both at 12 mV → 2
  (only energy sees it). `reserved2` showed the enabled set, ratio 3.4-4.75 rising under repeated strokes, margin 1.5,
  bits 0-1 and the spare bits 0. No source 0 seen.
- **Detection and ISR** (`tests/trigger_check.py`, clean; lowest stroke caught m06 / m10, mV), against the published
  10060 on the same bench minutes earlier:

  | enabled | 10061 | 10060 | ISR avg / peak (both builds) |
  |---|---|---|---|
  | edge | 50 / 120 | 50 / 120 | 47 / 53 % |
  | energy | 12 / 20 | 12 / 12 | 22 / 28 % |
  | both | 12 / 12 | 12 / 20 | 60 / 66 % |

  The 12 / 20 swaps are within the run-to-run spread. No false triggers.
- Bench note: a leaky generator cable had raised the noise floor about 20× (`nz` 48-69 against 2-3) and cost the
  energy detector its sensitivity (80 / 200 mV); replacing it restored `nz` 3. Spike and noise conditions (generator
  CH2) were not rerun; the detection code is unchanged from 10060.

## Images (my12 root, build 10061)

| File | Bytes | CRC | Server field |
|---|---|---|---|
| `my12-A22-10061.bin`, `my12-A11-10061.bin` | 307136 | `0x2fa1422d` | crc1 |
| `my12-I22-10061.bin`, `my12-I11-10061.bin` | 307136 | `0xfef5bf4d` | crc2 |
| `my12-A21-10061.bin` | 307272 | `0x32b24d67` | crc1 |
| `my12-I21-10061.bin` | 307272 | `0x223f7374` | crc2 |

## Deployment
Publish through detectorsrv10 and set `target_build` per detector. Without changes to lightsrv5 and detectorsrv10 the
new fields are simply ignored. lightsrv5 passes `reserved2` through to its JSON status unchanged, so that value is
normally non-zero from 10061 on.

Published 2026-10-18 and rolled out to most detectors. lightsrv5 0.7-13 (deployed on b2 the same day, `-gx 1`) records
`trigsrc` per strike in strike.log and reports `trigmode`, `energyratio`, `energymargin` and per-detector
`edgepkts` / `energypkts` / `bothpkts` counts in its detector status JSON. detectorsrv10 changes are pending.
