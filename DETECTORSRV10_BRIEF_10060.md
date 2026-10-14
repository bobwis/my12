# Brief for detectorsrv10: my12 firmware 10060 (fw 0.34) trigger detectors

## What changed in the detector firmware
From build 10060 a detector has **two trigger detectors**, each independently enabled:

- **Edge** (the existing one): fires on a sharp rise in short-term signal activity. Its sensitivity is set by its own
  adaptive threshold plus the server's existing **`tt`** offset (the knob the controller already uses).
- **Energy** (STA/LTA, new): fires when the short-term signal energy (12 µs) exceeds the long-term background (35 ms)
  by an **adaptive ratio**. The detector tracks its own quiet-time background and sets the ratio to that times a
  **margin** (default 1.5), clamped to 2.0-10.0. It also has its own rate feedback: each energy trigger nudges its
  ratio up, decaying over ~9 s. **`tt` has no effect on the energy detector.** The margin is its only server-side knob.

When both are enabled, both scan every 270 µs buffer and a buffer is sent if **either** fires. Sample packets don't say
which detector fired them.

Expected benefit: the energy detector catches smaller and smoother (more distant) strokes at quiet sites (bench and
site survey: 2.5-10× smaller strokes at quiet sites; little gain at noisy sites; worse than edge in broadband noise).
Not yet proven on located strikes in the field.

## New server key: `tr`
Format **`tr:<m>[<nn>]`**, a plain decimal integer in the existing `{key:value,...}` reply list:

| value | meaning |
|---|---|
| `1` | edge only (**firmware default at boot**) |
| `2` | energy only |
| `3` | edge + energy |
| `<m><nn>` | same `m`, plus the **energy margin in tenths**, `nn` = 10..40 (1.0×-4.0×). Default 15 when omitted |

Examples: `tr:3` both, margin 1.5; `tr:320` both, margin 2.0; `tr:212` energy only, margin 1.2; `tr:115` edge only
(the margin is stored but unused). Valid values are therefore 1, 2, 3 and 110-140, 210-240, 310-340.

Behaviour:
- Applied when the value **changes**; the detector prints `Server -> Triggers: edge + energy (energy margin 2.0)`.
- Invalid values (e.g. `31`, `309`, `341`, `99`) are logged on the detector console and ignored; the previous setting is kept.
- **Absent key = unchanged.** After a reboot the detector starts edge-only, then applies `tr` from its boot-time poll.
  So the server should send `tr` in every reply for detectors that should not be edge-only.
- Always send the full value: changing only the margin means re-sending the mode digit too (`tr:3` → `tr:320`).
- Firmware older than 10060 ignores unknown keys, so `tr` can be sent to every detector safely.
- Other existing keys are unchanged: `tt`, `pt`, `al`, `dsp`/`dsk`/`dsn` (impulse filter; it feeds both detectors).
- The experimental keys of builds 10056-10058 (`det`, `slk`, `slr`, `slm`, `sls`) **no longer exist**. Detector 18 ran
  those experiment builds and should move to 10060.

## Rate control in energy mode (the controller's job)
Today the controller steers each detector's **packet rate** with `tt`: a low target in quiet weather, a higher target
when a storm is first detected, so lightsrv5 gets more candidate packets to correlate. The same strategy should
carry over:

| Mode (`tr` m) | Knob for the packet rate | Direction |
|---|---|---|
| 1 edge | `tt` (as now) | higher `tt` = fewer packets |
| 2 energy | margin `nn` (10-40) | higher margin = fewer packets |
| 3 both | `tt` **and** margin | both contribute; the rate is the sum |

Practical guidance:
- **Margin steps:** move 1-2 tenths per adjustment, with hysteresis, and wait at least two poll intervals between
  steps. The detector's own adaptation settles in about 10-30 s, but the server only sees the effect in later packet
  counts. The response is non-linear: near the noise floor a small margin change moves the rate a lot.
- **Suggested starting points:** quiet weather margin 15-20; storm mode 12-15 (more sensitive, more packets). Keep it
  within 10-40. Below about 12 a noisy site will chatter.
- **Both mode:** the server can't tell from the packets which detector fired. A workable scheme:
  1. keep the existing `tt` loop for the edge detector unchanged;
  2. treat the margin as a second, slower loop: if the rate stays above target while `tt` is already at its usual
     upper limit, raise the margin; in storm mode lower it towards 12-15;
  3. if a site can't reach a reasonable rate even at margin 40, fall back to `tr:1` for that site.
- **Site differences** (from the site survey): quiet sites such as Mt Cotton (11) settle at low ratios and gain most.
  Noisy or interference sites such as 17, 13 and 18 (mains-locked house noise on 18) gain little and may need a higher
  margin or edge-only.
- The detector's own adaptive ratio already limits runaway false triggers. The margin is a per-site trim, comparable to
  `tt`, not something to change every poll.

## What the server can't see yet
The UDP status packet carries the edge threshold (`adctrigoff` = trigthresh + tt) and the gain, but **not** the trigger
mode, the energy ratio in use, or the energy-only / edge-only trigger counts. These are only on the console status line
(`d=` mode bitmask, `ra=` ratio ×16, `so=` / `eo=`). If the controller needs them, a later firmware change could put
them into spare status-packet fields. Ask Bob before relying on it.

## Status
10060 is on branch `feature/dual-trigger`. Bench test passed on detector 15 (detail in `my12/RELEASE_0.34_10060.md`).
Images in the my12 root: A22/A11 crc1 `0xa61c082b`, I22/I11 crc2 `0xbae29194`, A21 crc1 `0xfd8bcc08`, I21 crc2
`0xdfda44d3` (21 = detector 1 only). Not yet published.
