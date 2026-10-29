# Release note: my12 10065 (fw 0.34), 30 Oct 2026 - the version to stabilise on

Branch `experiment/agc-tt-decouple`, one commit on top of 10064. Rolled out to every detector that has an image.

## What changed from 10064
1. **`agt` defaults to 60 in the firmware** (`agc_th_gate`), so every unit behaves the same without the server sending the key (a detector keeps the last value it was sent, and a key that is merely cleared on the server is not undone). The server key `agt` still overrides: send `agt:0` explicitly for the stock attack.
2. **The threshold step in a near-miss cascade is +5 per 100 ms block** (it was +20 in the 10064 trial). The threshold falls back by 1 per quiet block, so a +5 hit decays in 5 blocks = 0.5 s, the same as the dead time (`sigsuppress` 2000 buffers) that `bumppga()` inserts after a PGA step so that the trigger averaging can assimilate the DC step. A severe cascade still reaches the gate (60 counts, from about 5) in about 1.1 s.

## The whole mechanism (since 10064)
In the 100 ms block, when more than 768 near-misses were counted (`PRETRIGCOUNTLIM`):
- `agt` 0 (stock): threshold +1 and the gain steps down at once. The threshold update is then skipped until the next 1 s mark (`gainchanged`), so the threshold cannot follow: 9 -> 1 in under 10 s.
- `agt` N: threshold +5; the gain steps down (through `bumppga(-1)`, so the 0.5 s dead time is kept) only once the threshold has reached N counts. The existing safety (threshold above 100: clamp and step down) is unchanged. Gain steps are rarer, so the DC steps that come with them are too.

## Why
With the fixed near-miss band (10062) the stock attack made noisy detectors' gain hunt (70 to 145 steps an hour on detectors 16, 18, 1, 4, 11, 17) and walked detector 13 to gain 1-2 (0.3 triggers a minute, in no solved strike). On 10064 with `agt` 60: detector 18 held gain 9 with no steps for 1.5 days while untreated detectors still hunted; detector 13 went back to gain 9, 0 to 45 steps an hour, threshold 25-35. Details in the instrumentation repo, `docs/2026-10-28_agc_gain_collapse_and_first_storm.md`.

## Images
| File | Bytes | CRC |
|---|---|---|
| `my12-A22-10065.bin`, `my12-A11-10065.bin` | 308248 | `0x7a00fb46` crc1 |
| `my12-I22-10065.bin`, `my12-I11-10065.bin` | 308248 | `0x2900bb3e` crc2 |
| `my12-A21-10065.bin` | 308368 | `0xf4f70e83` crc1 |
| `my12-I21-10065.bin` | 308368 | `0x0f22c025` crc2 |

## Rollout and rollback
Local units first (15, 18, 3, and 13 which is already a test unit), checked for ten minutes, then every other detector with an image (PCB 22 and 11 share one image, detector 1 is PCB 21; PCB 30 has no image). Rollback: advertise 10063 (stock-compatible) or 10062.
