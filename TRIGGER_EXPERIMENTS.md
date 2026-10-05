# Trigger-detection bench experiments (branch `experiment/trigger-detection`, started 2026-10-05)

Goal: keep near ground strokes clean (one or a few good packets, quick recovery from trains)
while also catching distant strokes that sit just above the noise floor at medium/high PGA gain.
Build number deliberately unchanged (10049) for local development.

## Rig
- Detector 15 on ST-Link (flash with STM32_Programmer_CLI), console COM7 (115200), UDP to this PC
  10.10.201.172:5000. Live variables readable over SWD hot-plug without halting (`tools/bench/swdpoll.py`).
- Junctek PSG9000 (DDS-60M) on COM11, see `tools/bench/psg.py` header for the VERIFIED register facts.
  CH1 = lightning recording in arbitrary memory 01 (2048 pts, staircase-shaped), burst once per second.
  Snapshots: `psg_original_full.json` (as found: CH1 replaying CONTINUOUSLY at 20 kHz - not bursting),
  `psg_bob_burst_1hz.json` (Bob's 1 Hz burst, CH1 at 130 kHz = 7.7 us playback, 0.7 V).
  **Current generator state:** Bob's burst setup but CH1 playback 5 kHz (200 us, w13=5000000,0) and
  amplitude 100 mV (w15=100). A resistor bridge sums CH1+CH2 into the detector input. CH1's burst is
  self-triggered by the generator's internal 1 s period (r43=1000), not by CH2. **CH2 is now Noise
  (code 14, set by Bob on the panel) at 20 mV (w16=20)** - snapshot before that change: `psg_selftrigger_1hz.json`. With it the
  detector sat at gain 2, noise reading 16-23, threshold 2, no noise triggers in a short look.
  Waveform codes on THIS unit differ from the vendor app's list: 11 = negative ladder (my w12=11
  mistake), 14 = noise, 101 = arb memory 01. Verify any other code on the panel before relying on it.
- Sample rate measured 2.70 MSps (two ways); 728 samples = 270 us/buffer = 58240 CPU cycles.

## Findings so far
1. **AGC lockout (master):** near-miss test reduced to a fixed "rise > 2", so raising trigthresh never
   reduced the near-miss count: threshold pinned at 100, gain stepped to 0, detector deaf indefinitely
   when noise > ~12. Bob has seen this on live detectors. Fixed: 698d9dc.
2. **Wrong/torn buffers sent:** the sender copied whatever buffer was current when it ran; ~40% of sent
   "events" lacked the stimulus waveform. Root cause is finding 3.
3. **ADC ISR CPU load 83% avg / 89% peak** of each buffer period (measured with DWT). Cut to ~38% / 43%:
   DMA buffer into DTCM (b807fc6, 83->75%) and hot-loop locals / no volatile SRAM1 loads (1c2ade9, ->38%).
4. **ENDSEQ is late and mislabelled:** only sent when the NEXT batch's first trigger notifies the task,
   carrying the next batch's id (confirmed on the wire). Not yet fixed.
5. Follow-on triggers 8-62 ms after a clipped stimulus (front-end overload recovery at gain 8).
6. Detector is an edge detector (32-sample window high-pass): smooth/distant waveforms need ~3-10x the
   amplitude of sharp ones (sim: `tools/bench/trigsim.py`). Main target for the STA/LTA work.
7. With the near-miss fix the AGC equilibrates but triggers on noise (~0.5/s) - set point regulates
   near-misses, not trigger rate. Needs a rate-based AGC.
8. Console input was dead (receive left disarmed); fixed 9f3a2ee (re-arm from LP loop). Ctrl-D, Ctrl-C
   (AGC toggle), digits (gain, AGC off) work again.

## Where we stopped
- Phase 4 step (c) `7721270` VALIDATED (fixed gain 8, AGC off): ISR 38% avg / 47% peak, late 2 and
  overrun 1 in 90 s. Baseline for detector comparison with CH1 100 mV + CH2 noise 20 mV: 73% of
  stimuli detected (66/91, +-10 ms of the generator's 1.000142 s period), ~1.1 noise-only buffers/s,
  stimulus peaks median 521 vs noise 373 counts. Earlier partial note: - ISR load median
  38% / peak 47%, late (lt) +0 over 120 s. Still to check: sent packets contain the stimulus waveform
  (`analyze_stim.py` on a fresh `udpcap.py` capture). Detector 15 is flashed with this build.
  Note lt (late/torn ISR copies) reached 9 after ~290 s - investigate.
- Then: ENDSEQ prompt delivery (finding 4); generator re-config for noise; rate-based AGC; STA/LTA
  detector; peak selection of the best buffer per event in netsendtask().
- Bob's original `#if 0` adcstream.c experiment is in `git stash` (stash@{0}).

## Phase 5 (STA/LTA detector) - status when paused (2026-10-05 evening)
- Commits: `cab4c44` STA/LTA detector (console Ctrl-T toggles; status line d= detector, r= peak STA/LTA*16;
  tuning globals stalta_ratio_q4 / stalta_ks / stalta_kl writable over SWD), `fdafba2` prompt ENDSEQ,
  `cd0b90e` LTA always learns (4x slower on triggered buffers) - fixes a runaway where the LTA froze after a
  noise step and every buffer triggered (sweep saw 27-46 noise triggers/s, more at a HIGHER ratio).
- **cd0b90e is built but NOT flashed.** Detector 15 runs `fdafba2` (STA/LTA without the LTA fix), left with
  AGC on, edge detector selected.
- STA/LTA ISR load: **13% avg / 14% peak** (edge detector 38% / 46%).
- Sweep results so far are INVALID: the generator wiring/bridge became unreliable. With CH2 at 0 mV the
  detector still saw a strong 20.15 kHz periodic signal (autocorrelation r=0.8 at 49.6 us) and noise ~1430
  at gain 8, and the 1 s stroke was not visible (Bob's scope agreed). Bob is repairing the cabling.
- Detector 15 rebooted once unexplained during the runaway STA/LTA sweep (uptime reset to 12 s) - possibly
  the UDP stall guard under ~46 triggers/s; keep a console log running in future sweeps to catch the cause.
- Gotcha: sample packets are only sent once GPS is locked (~1-2 min after boot); after any reboot wait for
  lock before scoring detection.
- Generator: CH2 = Noise (code 14) 20 mV; CH1 burst setup as last fixed by Bob (r24 = 00,01,11,00), 5 kHz
  playback, 100 mV; snapshot `tools/bench/psg_stim_noise.json`.
- Next: once wiring is fixed - flash cd0b90e, confirm the 1 s stroke with CH2 off (`sweep.py` scoring),
  calibrate STA/LTA ratio to match the edge detector's noise-trigger rate, then amplitude sweep both detectors.
- Storm captures: `C:\projects\lightning\captures\<storm>\lightsrv*.cap` = raw detector UDP payloads
  concatenated (parser `tools/bench/capfile.py`); README per storm. Some samples carry bit 12/13 flags (mask
  0x0FFF; meaning to confirm with Bob). Plan: load real distant-stroke waveforms into spare generator
  memories at real-time rate (~1.32 kHz for 2048 pts) and/or replay buffers offline through both detectors.
