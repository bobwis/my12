# Release note: my12 10064 (fw 0.34), trial, 28 Oct 2026

Trial build for detectors 18 and 15 only. Branch `experiment/agc-tt-decouple`, one commit on top of 10063 (drop it to go back).

## What it adds: server key `agt`, the AGC gain-down threshold (0-100, default 0)
Why: with the fixed near-miss band (10062) the AGC steps the gain down on every near-miss cascade (more than 768 near-misses in a 100 ms block). On a site with bursts of interference that is a stream of attacks (detectors 16, 18, 1, 4, 11, 17: 70-140 gain steps an hour) and on detector 13 it walks the gain to step 1 and keeps it there.
The stock attack (`main.c`, the 100 ms block) adds 1 to the threshold and steps the gain down at once, and because the threshold update is skipped while `gainchanged` is set (until the next 600 ms mark of the second) the threshold cannot follow: 9 -> 1 in under 10 s with the threshold still at the floor.

- `agt:0` (default): stock behaviour, unchanged.
- `agt:N`: in a cascade the threshold takes the first hit (+20 per 100 ms block, as the AGC-off path already does) and the gain steps down only once the threshold has reached N counts. The existing safety (threshold above 100: clamp and step down) is unchanged, so a noise that the threshold alone can never absorb still lowers the gain.
- The threshold falls again by 1 per quiet 100 ms block, so after a burst it is back to its floor in a few seconds, against about 10 s per gain step for the release.

Server side: registry entry `agt` (since build 10064), set per detector on the Manage page or with `PATCH /detectors/N {"agt": 60}`. A missing key leaves the value unchanged: to go back to stock send `agt:0` explicitly.

## Images
| File | Bytes | CRC |
|---|---|---|
| `my12-A22-10064.bin`, `my12-A11-10064.bin` | 308248 | `0xfb7df9a3` crc1 |
| `my12-I22-10064.bin`, `my12-I11-10064.bin` | 308248 | `0xa87db9db` crc2 |
| `my12-A21-10064.bin` | 308368 | `0xd4085e73` crc1 |
| `my12-I21-10064.bin` | 308368 | `0x2fdd90d5` crc2 |

## Results so far (28 Oct 2026)
- Field, detector 18 (live, noisy): `agt` 60 against `agt` 0 over the first hour: gain steps an hour about 73 -> 6 to 42, mean gain 5.8 -> 7.8, threshold median 5 -> 9, max 50 (the threshold takes the hit). Different weather windows, so an alternating A/B is still to do.
- Bench, detector 15, generator spike train at 300 mV, impulse filter 0, 150 s per trial (instrumentation `tests/agc_e6.py`): `agt` 0 / 60 / 0 / 60 gave gain at end 8 / 9 / 7 / 7, gain changes 7 / 0 / 3 / 7, triggers 291 / 13 / 46 / 326. The two stock runs differ as much as stock differs from `agt` 60: this stimulus sits at the edge of the cascade condition and does not decide. A burstier or stronger stimulus is needed for a clean bench comparison.
- Detector 13 (remote, the collapse case) has not been given `agt` yet: it is held at gain 6 (10063 keys agc/pga) for 24 h to see whether it contributes to strikes.

## Test plan
Bench (detector 15, generator CH1 direct): spike train at the E5 level, gain trace at 1 Hz from the console status line, `agt` 0 against 60 (and 30/100), same stimulus. Field (detector 18): gain steps per hour, mean gain and trigger rate over hours with `agt` 0 against 60.
