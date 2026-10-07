# Trigger detection modes (my12, build 10055)

The ADC samples at 2.7 MSps into 728-sample buffers (270 µs each). The TIM5 interrupt scans each buffer as it
completes. If the detector triggers, that buffer is copied to the UDP send queue as a sample packet; consecutive
triggered buffers form one batch. Two detectors are built in. Console **Ctrl-T** switches between them, and the
status line shows the active one as `d=` (0 edge, 1 STA/LTA). Every boot starts with the edge detector.

## 1. Edge detector (default, `d=0`)

Looks for a sudden rise in short-term activity, i.e. the sharp leading edge of a stroke.

- **Measure:** a 32-sample sliding window (12 µs). For each sample it takes the window mean, then the sample's
  distance from that mean. The running average of that distance over the window is the "mean window difference"
  (`mwd`), a high-pass measure of how busy the signal is right now.
- **Trigger:** when `mwd` jumps above its previous value by more than `trigthresh + tt`. `tt` is the server's
  trigger offset. A smaller rise above `pretrigthresh + tt` counts as a near miss (`nm` on the status line).
- **Self-adjusting threshold:** every 100 ms the LP task looks at the near-miss count. Too many raises `trigthresh`
  (by 1, 2 or 4, or by 20 / a gain step down when far too many). Few lets it fall back towards the minimum. Each
  trigger also adds 2, so a burst limits itself. Above 100 the threshold is clamped and, with AGC on, the gain is
  reduced. The AGC sets the PGA gain from the noise level (`g`, `nz`).
- **Impulse filter:** before the scan, the buffer can pass through the spike filter (Ctrl-F, `f=`, server `dsp`).
  The default, mode 3 jump-hold, ignores single-sample spikes such as detector 13's switching-converter
  interference while letting a real edge through about 1 µs later. It only affects detection: the samples sent to
  the server are raw.
- **Strengths/limits:** sensitive to fast edges and cheap (~40% of the interrupt budget with the filter). Without
  the filter it is fooled by one-sample spikes, and it is less sensitive to slow-rising signals.

This mode, with filter mode 3, is the release configuration.

## 2. STA/LTA energy detector (experimental, `d=1`)

A classic short-term/long-term average ratio, as used in seismic triggering. It responds to energy relative to the
background, not to edge sharpness.

- **Measure:** `e = |sample - baseline|`, where the baseline is a slow average of buffer means.
  - **STA** is a running average of `e` over about 64 samples (24 µs).
  - **LTA** is a running average of the buffer-mean `e` over about 128 buffers (35 ms). It still learns on
    triggered buffers, but 4× slower, so a burst can't freeze it.
- **Trigger:** when STA exceeds LTA × 4.0. The status line's `r=` is the peak STA/LTA ratio ×16 since the last line.
- **Self-scaling:** because the threshold is a ratio to the noise floor, it doesn't use `trigthresh`, the near-miss
  loop or the server's `tt`. The AGC still sets the gain from the LTA level.
- **Not applied:** the impulse filter (this detector scans the raw buffer).
- **Tuning:** only over SWD, via `stalta_ratio_q4` (16 = 1.0×, default 64), `stalta_ks` (STA 2^ks samples, default 6)
  and `stalta_kl` (LTA 2^kl buffers, default 7).
- **Status:** bench experiment only, not validated. Early sweeps showed it reacting to noise steps. It is not
  recommended for field use.

## Quick comparison

| | Edge (default) | STA/LTA (experimental) |
|---|---|---|
| Responds to | sharp rise in short-term activity | energy above the background |
| Threshold | adaptive `trigthresh` + server `tt` | fixed ratio (4.0×) to the long-term average |
| Server `tt` offset | yes | no |
| Impulse filter | yes (default mode 3) | no |
| Console / status | Ctrl-T, `d=0` | Ctrl-T, `d=1`, `r=` |
| Use | field and release | bench experiments |

See also `CONSOLE_COMMANDS.md` (keys and status line) and `TRIGGER_EXPERIMENTS.md` (bench history).
