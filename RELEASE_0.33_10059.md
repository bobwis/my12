# Release 0.33 / build 10059 (2026-10-13)

Master (`a24d919` + version bump). Not part of this build: the dual-detector / adaptive STA/LTA experiments of builds
10056-10058 (`experiment/stalta-filter`).

## What changed since 10055

- **Gentler trigger back-off** (`adcstream.c`, ADC scan): the first buffer of a triggered batch raises `trigthresh` by 1
  (AGC near-miss count still +201); follow-on buffers raise it by 2 and add 100 instead of 201. Retested on detector 15
  after the 10054 latency fix: `ov`/`lt`/`sk` stayed 0, no gain hunting, about 14% more triggered buffers per real
  stroke, about 45% more trigger packets under a detector-13 style spike storm with the impulse filter off.
  Details and numbers in `ADC_TRIGGER_FIXES.md` ("Gentler back-off after a trigger"). Console Ctrl-B toggles back to
  the stock step; the status line gains `bo=` and `bl=a/b/c/d` (batches of 1/2/3/4+ buffers).
- **LIGHTNINGBOARD1 image** (detector 1, Colin VK4ZPU): `LIGHTNINGBOARD1_BUILD` switch in `version.h` makes
  `getboardpcb()` assign PCB 21 (GPS on USART6, no ESP); the unit then asks for `my12-{A,I}21-<build>.bin`. Default off.
- `version.h`: include guard and `#ifndef TESTING` nesting fixed (a `TESTING` build compiles again).
- `tools/build_release.ps1` builds all six images (see below).

## Images (my12 root, build 10059)

| File | Bytes | CRC | Server field |
|---|---|---|---|
| `my12-A22-10059.bin`, `my12-A11-10059.bin` | 306328 | `0x578d02b9` | crc1 |
| `my12-I22-10059.bin`, `my12-I11-10059.bin` | 306328 | `0x88d46be0` | crc2 |
| `my12-A21-10059.bin` | 306448 | `0x849cb571` | crc1 |
| `my12-I21-10059.bin` | 306448 | `0x0ed66e4c` | crc2 |

22 and 11 are the same image (PCB chosen at run time from PC0). 21 is a different binary and only detector 1 asks
for it. Always build both: `.\tools\build_release.ps1` from PowerShell (about 12 min).

## Deployment

Publish through detectorsrv10 (`POST /api/v1/firmware`), then set `target_build` per detector. The units update when
the server advertises a build different from the one they report, so only detectors whose `target_build` is changed
are affected. First unit: detector 15 (bench), to prove the detectorsrv10 update path.

## Not tested yet

PCB 21 image on detector 1 (hardware); gentle back-off on field units (bench only so far).
