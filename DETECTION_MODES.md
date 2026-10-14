# Trigger detectors (my12, from build 10060)

The ADC samples at 2.7 MSps into 728-sample buffers (270 µs each). The TIM5 interrupt scans each buffer as it
completes. If a detector triggers, that buffer is copied to the UDP send queue as a sample packet; consecutive
triggered buffers form one batch.

There are two trigger detectors, **independently enabled**. When both are on, both scan every buffer and either one
triggers it. The impulse filter (Ctrl-F, `f=`, server `dsp`; default mode 3) runs first and feeds both, so
single-sample spikes reach neither. The samples sent to the server are always raw.

| Enabled (status `d=`) | Console Ctrl-T prints | Server |
|---|---|---|
| 1: edge only (**default**) | `Triggers enabled: edge` | `tr:1` |
| 2: energy only | `Triggers enabled: energy` | `tr:2` |
| 3: both | `Triggers enabled: edge + energy` | `tr:3` |

Ctrl-T cycles edge → energy → both → edge. A server value applies at the next poll. Neither is stored: every boot
starts edge-only until the server's `tr` arrives (or Ctrl-T is pressed). "Neither" is not possible.

Server key `tr:<m>[<nn>]`: `m` as above; the optional two digits `nn` set the energy detector's adaptive margin in
tenths (10-40, default 15 = 1.5×). Examples: `tr:3` both, default margin; `tr:325` both, margin 2.5 (fewer false
energy triggers, less sensitive); `tr:212` energy only, margin 1.2. Anything else is logged
(`Server -> tr N invalid ...`) and ignored. A change prints `Server -> Triggers: edge + energy (energy margin 1.5)`.

## 1. Edge detector

Looks for a sudden rise in short-term activity, i.e. the sharp leading edge of a stroke.

- **Measure:** a 32-sample sliding window (12 µs). For each sample it takes the window mean, then the sample's
  distance from that mean. The running average of that distance over the window is the "mean window difference"
  (`mwd`), a high-pass measure of how busy the signal is right now.
- **Trigger:** when `mwd` jumps above its previous value by more than `trigthresh + tt`. `tt` is the server's
  trigger offset. A smaller rise above `pretrigthresh + tt` counts as a near miss (`nm`).
- **Self-adjusting threshold:** every 100 ms the LP task looks at the near-miss count. Too many raises `trigthresh`,
  few lets it fall. After its own trigger the threshold steps up (gentle back-off by default: first buffer of a batch
  +1, follow-on buffers +2 with a smaller near-miss charge; Ctrl-B selects the stock +2/+201). The AGC sets the PGA
  gain from the noise level (`g`, `nz`).
- **Only the edge detector's own triggers** feed its threshold and AGC. Energy triggers don't (in the 10057 soak they
  drove detector 18's gain down to 0-2).

## 2. Energy detector (STA/LTA)

Responds to energy relative to the site's background rather than to edge sharpness, so it catches smaller and
smoother strokes where the background is quiet.

- **Measure:** `e = |sample - baseline|` (baseline: slow average of buffer means).
  - **STA:** running average of `e` over 32 samples (12 µs).
  - **LTA:** running average of the buffer-mean `e` over 128 buffers (35 ms), learning 4× slower on triggered buffers.
- **Trigger:** when STA exceeds LTA × the **adaptive ratio** (status `ra=` is the ratio ×16).
- **Adaptive ratio:** the energy detector tracks the peak STA/LTA of untriggered buffers (rising 1/64 of the gap per
  buffer, falling over ~9 s, not learning for 64 buffers after any trigger) and multiplies it by the margin (default
  1.5), clamped to 2.0-10.0. Each energy trigger also raises the track a little (rate feedback), so persistent
  impulsive noise lifts its own threshold. A quiet site settles near 2-3, a noisy one at 4-6 or more, without tuning.
- **Fixed in the firmware:** STA and LTA lengths, the rate feedback step and the clamp limits. Only the margin can be
  changed (the `tr` suffix). The per-sample work is the same whatever the margin.
- **ISR cost:** about 22% of the buffer time (filter included), against about 44% for the edge detector; both together
  about 60% average, 70-74% peak.

## When to use which (bench and field results to 2026-10-14)

| Background | Edge | Energy | Both |
|---|---|---|---|
| Clean / quiet site | baseline | 2.5-10× more sensitive on real strokes | best |
| Detector-13 style spikes (filtered) | good | about level | about edge or better |
| Broadband noise | good | worse | about edge |
| Mains-locked house noise (detector 18) | false triggers | false triggers at up to ~0.5/s (ratio pinned at 6 in 10057) | sum of both |

The site survey (instrumentation `analysis/SITE_SURVEY.md`) predicts "both" at about 3.4× the edge detector's
sensitivity at quiet sites (Mt Cotton 11, detector 18) and little gain at noisy ones (Capalaba 17). It has not yet
been proven on located strikes. Status counters `so=` / `eo=` (energy-only / edge-only triggered buffers since boot)
show what each detector adds when both are on.

See also `CONSOLE_COMMANDS.md`, `TRIGGER_EXPERIMENTS.md` and instrumentation `analysis/FINDINGS_2026-10-08.md`.
