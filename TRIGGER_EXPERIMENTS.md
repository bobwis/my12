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

## Release 0.32 / build 10050 (2026-10-06) and what its soak found
Master 7eb6413 = the validated fixes from this branch (lockout, ISR copy, ISR load, prompt ENDSEQ,
console) + fixes found in the soak, all merged back here (STA/LTA, Ctrl-T, d=/r= kept on top;
CONSOLE_STATUS_SECS 1 here, 60 in the release). Details: ADC_TRIGGER_FIXES.md. Soak findings:
- **Stall watchdog false positive** (fixed e89e2ab): samples are now queued by the ISR, which then wakes
  startudp() whose watchdog saw a fresh item before NetSend ran; after >30 s of quiet the next trigger
  rebooted the detector. Now judged on sender liveness (sq_alive_sec). Verified twice with 50 s generator
  pauses (python psg.py: r10 "0,0" then restore).
- Console ISR average wrapped at 60 s intervals (b871046, 16-cycle units).
- LCD redraws page 0 after unplug/replug (014ec39; 0x88 ready or framing re-init -> lcd_repaint()).
- Status packet number + queue send made atomic (7040da3). The out-of-order arrivals seen on the PC are
  RECEIVER-side: samples are IP-fragmented (NETIF_MTU_OVERRIDE), status packets aren't.
- **Gentler first back-off step** (+1 instead of +2) tried: branch `experiment/backoff-tweak`. 2-3 packet
  batches 0.1% -> 2%, but dropped batches 1% -> 5%, ov/lt per trigger ~2x, AGC hunting gain 8<->9. Not
  shipped. Batches stay one packet mainly because of ov: startudp() shares osPriorityNormal with
  tcpip_thread, so the 270 us sigsend handshake is often missed and the next buffer isn't scanned.
- **lwIP raw API called without the core lock** (pre-existing, also in 10049): sendudp() udp_sendto() from
  NetSend, and www.c's client (LOCK_TCPIP_CORE commented out), race tcpip_thread (ARP queue, frag).
  Seen once: mem.c sanity asserts "heap element link valid"/"unused?" (double free) at 09:01 on 7eb6413.
  First item for 10051, with an overnight soak.
- **lt (late ISR copies) tracks the SEND rate** (1/min at 2 batches/s -> ~47/min with 1000-10000 repeat
  bursts) while ISR load stays ~37%. Hypothesis, unmeasured: MEM_SANITY_CHECK=1 / MEM_OVERFLOW_CHECK=2 walk
  the whole lwIP heap on every malloc/free inside a BASEPRI critical section, masking the ADC IRQ (prio 5).
- Generator worked all day today (1 Hz burst, 25 -> 50 -> 1000 -> 10000 repeats); each stroke gives ~2
  one-packet batches 2-100 ms apart. Tools added: udplive.py (live per-interval rx counts, flushes),
  batchstats.py (batch sizes / groups per window), endseq_check.py.
- Next on this branch: core lock (10051), measure the heap-check latency, startudp() priority / drop the
  sigsend gate, then revisit the back-off tweak and STA/LTA sweeps (cd0b90e LTA fix still unvalidated).

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
