# Trigger-detector fields in the detector UDP packets (from my12 build 10061)

Firmware 10060 has two trigger detectors, independently enabled (see `DETECTION_MODES.md`): **edge** and **energy**
(STA/LTA). Build 10061 reports which detector fired each sample packet, and the enabled set plus the energy detector's
state in every status packet. Both use bits that older consumers already ignore.

**Interpret these fields only when the detector's build is 10061 or later.** The build is in the status packet
`firmwarever` field (offset 132, u16 = BUILD). Older firmware puts a cycling counter in the sample bits and zero in
the status bits.

## Sample packet (type 0/4, 1472 bytes): byte 4

| bits | meaning |
|---|---|
| 7..2 | `rtseconds`, GPS seconds of the minute (unchanged; lightsrv5 `calcrt` already reads `(byte4 and $FC) shr 2`) |
| 1..0 | **trigger source** of this buffer: bit 0 = edge detector fired, bit 1 = energy detector fired (1 edge, 2 energy, 3 both) |

- Every sample packet is a triggered buffer, so the value is 1, 2 or 3; 0 should not occur from 10061 on.
- Each buffer of a batch carries its own value, so a long batch shows which detector kept it going.
- Before 10061 these two bits were `adcbufnum & 3`, a 0-3 counter nothing used. 10061 drops that counter.

## Status packet (type 1/2, 156 bytes): `reserved2` (u32 at offset 148)

| bits | meaning |
|---|---|
| 0 | GPS unlocked (unchanged; lightsrv5 `LOCK_ERROR`) |
| 1 | PPS error (unchanged; lightsrv5 `PPS_ERROR`, detectorsrv10 `pps_error_bit`) |
| 3..2 | **enabled trigger detectors**: 1 edge, 2 energy, 3 both (0 = firmware before 10061) |
| 4 | **near-miss mode** (build 10062): 0 stock band (follows tt), 1 fixed band (independent of tt) |
| 7..5 | **gain steps** (build 10062): PGA gain changes since the previous status packet, saturating at 7 |
| 15..8 | **energy ratio in use** ×16 (32-160 = 2.0-10.0×). The energy detector's adaptive threshold at the moment the status packet was built. Meaningful only when energy is enabled |
| 23..16 | **energy margin** ×10 (10-40 = 1.0-4.0×), as set by the server `tr` suffix (default 15) |
| 31..24 | **trigthresh** (build 10062): the unit's floating edge threshold, ADC counts, saturating at 255. The threshold in force is `adctrigoff & 0xFFF` = trigthresh + tt, so tt in force = `(adctrigoff & 0xFFF) - trigthresh` |

- The fields are composed into the queued copy of the status packet when it is built, so the existing bit 0/1
  updates in the live structure are not disturbed.
- Every existing consumer tests bits 0 and 1 by mask (lightsrv5, detectorsrv10, lightsrv6 documentation), so the new
  bits don't change their behaviour. Code that treats a non-zero `reserved2` as an error would need updating; none
  was found.

## Status packet: `auxstatus1` (u32 at offset 116), build 10062

| bits | meaning |
|---|---|
| 7..0 | ADC batch id (unchanged) |
| 15..8 | jabber flag (unchanged) |
| 19..16 | **gain max** since the previous status packet (PGA step 0-9; equals the current gain when it did not change) |
| 23..20 | **gain min** since the previous status packet |
| 31..24 | **AGC pressure**: the near-miss plus trigger back-off count (`pretrigcnt`) accumulated since the previous status packet, as 8 x log2(n + 1) (0 = none; decode n + 1 = 2^(q/8), about 4 % steps, truncated so it reads up to 9 % low; saturates at 255). |

Existing consumers read bits 7..0 and 15..8 by mask (lightsrv5 `and $FF` / `and $FF00`, lightsrv6 `auxstatus1 & 0xFF00`), so the new bits do not change
their results. The AGC intervals restart at every status packet of either type (timed or end-of-batch), so a consumer that keeps only some status packets
sees the combined interval of the ones it dropped only through the gain min/max (not through the pressure byte, which is per packet).

## Setting the detectors (unchanged from 10060)
Server key `tr:<m>[<nn>]` in the poll reply: m 1 edge, 2 energy, 3 both; nn = energy margin ×10 (10-40). Console Ctrl-T.
See `DETECTORSRV10_BRIEF_10060.md` for the rate-control guidance.

## Decoding examples
- Sample byte 4 = `0x2B` → rtseconds 10, source 3 (both fired).
- `reserved2` = `0x000F350C` → lock and PPS OK, enabled 3 (both), ratio 0x35 = 53 → 3.31×, margin 0x0F = 15 → 1.5×.

## Consumers
- lightsrv5 0.7-12/0.7-13 (on b2 from 2026-10-18): per-hit `trigsrc` in strike.log and candidates.log; with `-gx 1`
  the detector status JSON adds `trigmode`, `energyratio`, `energymargin` and `edgepkts` / `energypkts` / `bothpkts`.
- detectorsrv10: pending (see `DETECTORSRV10_BRIEF_10061.md`).
