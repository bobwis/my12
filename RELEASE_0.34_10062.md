# Release note: my12 10062 (fw 0.34), trial build, 24 Oct 2026

Trial build for detectors 18 and 3 (and bench detector 15). Source: branch `experiment/agc-tt-decouple` (not merged to master). Same detection arithmetic as 10061 at tt 0.

## Changes
1. **AGC near-miss band independent of the server's tt** (`nm_mode`, default `NM_FIXED`). A near miss is a rise in (pretrigthresh, trigthresh]; the trigger stays
   trigthresh + tt. In 10061 the band was (pretrigthresh + tt, trigthresh + tt], so any tt above the noise rise left the AGC blind: the gain stayed at its maximum, the
   floating threshold sat at its floor, and the packet rate was flat up to a cliff (impulsive noise) or a flood at small tt (bursts). Console Ctrl-N and server key `nmm`
   (0 stock, 1 fixed) switch back; status line `nmm=`.
2. **AGC state in the status packet** (spare bits only, packet size unchanged): `auxstatus1` 31..24 pressure byte (8 x log2(count + 1) since the previous status packet),
   23..20 / 19..16 gain min / max; `reserved2` 31..24 trigthresh, 7..5 gain steps, bit 4 near-miss mode. Details in `PACKET_TRIGGER_FIELDS.md`.
3. The edge-scan loop is written twice (macro `EDGE_LOOP(NMFIX)`) so the stock path compiles as before: a single shared loop with an extra live variable cost +19 points of ADC ISR load.

## Measured (bench, detector 15)
ADC ISR (average/peak of the buffer budget): stock 47/52 %; fixed band 58/63 % under bursts (peak 70 %); lost buffers 0. Behaviour and the stock comparison: instrumentation repo
`docs/2026-10-24_agc_tt_bench_findings.md`.

## Images
`my12-A22-10062.bin` = `my12-A11-10062.bin` crc1 0xce40462c; `my12-I22-10062.bin` = `my12-I11-10062.bin` crc2 0xd507346d (307856 bytes). `my12-A21-10062.bin` 307976 bytes crc1 0x714a7ba4 and `my12-I21-10062.bin` crc2 0xa7219c3b (LIGHTNINGBOARD1 prototype, built 27 Oct 2026 with `LIGHTNINGBOARD1_BUILD` on; A links at 0x8000000, I at 0x8100000; the normal A/I builds reproduce the published CRCs byte for byte). The server's crc is the STM32 hardware CRC (poly 0x04C11DB7, init 0xFFFFFFFF, big-endian words). No PCB 30 image exists for any build (detector 8).

## Rollback
Advertise 10061 for the detector (detectorsrv10: `target_build` 10061); the units update themselves back.
