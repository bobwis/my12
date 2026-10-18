# Brief for detectorsrv10: my12 build 10061 (fw 0.34) - the server can see the trigger detectors

Addendum to `DETECTORSRV10_BRIEF_10060.md` (the `tr` key and the rate-control guidance are unchanged). The exact bit
layout is in `PACKET_TRIGGER_FIELDS.md`; this is the controller's view of it.

## New information in the packets (only when the status `firmwarever` >= 10061)

**Status packet `reserved2`** (u32 at offset 148; bits 0 and 1 unchanged: GPS unlocked, PPS error):

| bits | field | use for the controller |
|---|---|---|
| 3..2 | enabled set: 1 edge, 2 energy, 3 both | confirms the `tr` mode was applied (it follows the console Ctrl-T too) |
| 15..8 | energy ratio in use ×16 (32-160 = 2.0-10.0) | how hard the energy detector is working against its background. At 32 (the floor) it is as sensitive as it gets; near 160 it is fighting noise or a high trigger rate |
| 23..16 | energy margin ×10 (10-40) | confirms the margin from the `tr` suffix |

Mask the old bits as before: test bit 1 with `pps_error_bit`, never `reserved2 != 0`. From 10061 `reserved2` is
normally non-zero (e.g. `0x000F350C`).

**Sample packet byte 4 bits 1..0** (1 edge fired, 2 energy fired, 3 both). If detectorsrv10 sees sample packets (or
lightsrv5 counts for it), counting these per detector gives the edge / energy split of the packet rate directly.

## Already decoded by lightsrv5 on b2 (0.7-13, running with `-gx 1` since 2026-10-18)
`GET http://localhost:9080/detector/<id>` on b2 (no auth; detectorsrv10 runs on the same host) (and the `/db/status` post to b3) carries, for detectors on 10061+:
`trigmode` (1/2/3), `energyratio` (e.g. 3.31), `energymargin` (e.g. 1.5), and `edgepkts`, `energypkts`, `bothpkts`:
sample packets received from that detector by trigger source since lightsrv5 started (a decrease means lightsrv5
restarted). Differences between two polls give the edge / energy split of the packet rate without decoding the
packets in detectorsrv10. Older firmware: all 0. `reserved2` is still passed through whole.
strike.log also records `trigsrc` (`id:src` in `dlist` order) for every located strike.

## Rate control with the new fields

- In **both** mode, split the measured packet rate by source: edge-only packets (1) answer to `tt`, energy-only
  packets (2) to the margin; packets with source 3 would have been sent by either and need no action.
  This replaces the "second, slower loop" guess in the 10060 brief: raise `tt` when source 1 dominates the excess,
  raise the margin when source 2 does.
- If only status packets are available: the ratio drifting up towards 10 with the margin unchanged means the energy
  detector's own feedback is already holding back a high rate (noise or storm). Raising the margin further then has
  little effect; consider `tr:1` for that site in quiet weather.
- After a reboot the detector reports set 1 (edge) until the next poll applies `tr`; check bits 3..2 against the
  configured mode before acting on rates.
- Firmware before 10061: bits 3..2 / 15..8 / 23..16 are 0 and the sample bits are a meaningless 0-3 counter. Use the
  build number, not a zero test.

## Status
Bench-tested on detector 15 (2026-10-18): all three modes report the expected sources and status fields; detection
sensitivity and ISR load are unchanged from 10060. Published 2026-10-18 (master `3da5bb5`); Bob is rolling it out to
most detectors.
