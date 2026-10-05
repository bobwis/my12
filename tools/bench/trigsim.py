"""Bit-faithful model of ADC_Conv_complete()'s trigger loop (adcstream.c), to
check how it responds to sharp vs smooth pulses. Scratch analysis only."""
import numpy as np

FS = 27e6 / 5          # triple-interleaved, 5-cycle delay, ADCCLK = 108MHz/4 -> 5.4 MSps
BUFN = 728             # ADCBUFSIZE >> 1 samples per buffer
W = 32                 # WINSIZE
SH = 5                 # WINSHIFT


class Existing:
    """Exact integer semantics of the current loop, incl. j = i & 31 per buffer."""
    def __init__(self, T):
        self.T = T
        self.lastsamp = [0] * W
        self.windiff = [0] * W
        self.wmeanacc = 0
        self.wdacc = 0
        self.lastmwd = 0
        self.trace = []

    def warm(self, dc):
        self.lastsamp = [dc] * W
        self.wmeanacc = dc * W

    def buffer(self, samples, record=False):
        trig = False
        for i, s in enumerate(samples):
            j = i & (W - 1)
            s = int(s)
            self.wmeanacc += s - self.lastsamp[j]
            winmean = self.wmeanacc >> SH
            self.lastsamp[j] = s
            d = abs(s - winmean)
            self.wdacc = self.wdacc - self.windiff[j] + d
            mwd = self.wdacc >> SH
            self.windiff[j] = mwd
            if abs(mwd) > self.lastmwd + self.T:
                trig = True
            self.lastmwd = abs(mwd)
            if record:
                self.trace.append(mwd)
        return trig


def step_response():
    """Feed the |deviation| path a clean step (winmean pinned): does meanwindiff overshoot/ring?"""
    windiff = [0] * W
    wdacc = 0
    out = []
    n = 0
    for b in range(2):
        for i in range(BUFN):
            j = i & (W - 1)
            d = 0 if n < 100 else 100
            wdacc = wdacc - windiff[j] + d
            mwd = wdacc >> SH
            windiff[j] = mwd
            out.append(mwd)
            n += 1
    out = np.array(out)
    print("Step 0->100 in |deviation| fed to the wdacc/windiff recursion:")
    print("  meanwindiff peak = %d (a true 32-sample average would settle at 100, never exceed it)" % out.max())
    print("  meanwindiff min after peak = %d" % out[out.argmax():].min())
    print("  samples to first reach 100 = %d" % np.argmax(out >= 100 - 1))


def pulse(kind, A, n):
    t = np.arange(n) / FS
    if kind == "sharp":       # close stroke: ~0.5us rise, ~20us decay
        p = np.exp(-t / 20e-6) - np.exp(-t / 0.5e-6)
    elif kind == "smooth":    # distant stroke: one smooth ~10kHz cycle (ground-wave filtered)
        f = 10e3
        env = np.where(t < 1 / f, np.sin(np.pi * f * t) ** 2, 0.0)
        p = env * np.sin(2 * np.pi * f * t)
    elif kind == "spike":     # 2-sample impulsive interference / clipped edge
        p = np.zeros(n)
        p[0:2] = 1.0
    elif kind == "smooth5k":
        f = 5e3
        env = np.where(t < 1 / f, np.sin(np.pi * f * t) ** 2, 0.0)
        p = env * np.sin(2 * np.pi * f * t)
    p = p / np.abs(p).max()
    return A * p


NB = 6                 # buffers per trial: 2 settle + pulse lands in buffer 2 + room for a 200us tail


def make_trial(kind, A, sigma, rng):
    dc = 2048
    x = dc + rng.normal(0, sigma, BUFN * NB)
    start = BUFN * 2 + rng.integers(0, BUFN)
    p = pulse(kind, A, BUFN * 3)
    x[start:start + len(p)] += p
    return np.clip(np.round(x), 0, 4095), start // BUFN


def min_detect(kind, T, sigma=8.0, trials=24, rng=np.random.default_rng(1)):
    """Smallest pulse amplitude (ADC counts) detected in >= 50% of trials."""
    dc = 2048
    lo, hi = 1.0, 2000.0
    for _ in range(16):
        A = (lo + hi) / 2
        hits = 0
        for k in range(trials):
            det = Existing(T)
            det.warm(dc)
            x, _ = make_trial(kind, A, sigma, rng)
            for b in range(2):                         # settle
                det.buffer(x[b * BUFN:(b + 1) * BUFN])
            got = any(det.buffer(x[b * BUFN:(b + 1) * BUFN]) for b in range(2, NB))
            hits += got
        if hits >= trials / 2:
            hi = A
        else:
            lo = A
    return hi


def stalta_min_detect(kind, R, sigma=8.0, kdc=8, ksta=5, klta=14, trials=24,
                      rng=np.random.default_rng(2)):
    """Alternative: EMA DC tracker + EMA STA/LTA of |x-dc|. Integer-only, no arrays."""
    dc0 = 2048
    lo, hi = 1.0, 2000.0
    for _ in range(16):
        A = (lo + hi) / 2
        hits = 0
        for k in range(trials):
            x, _ = make_trial(kind, A, sigma, rng)
            x = x.astype(np.int64)
            dcacc = dc0 << kdc
            sta = int(sigma * 0.8) << ksta
            lta = int(sigma * 0.8) << klta
            got = False
            for n, s in enumerate(x):
                dc = dcacc >> kdc
                dcacc += s - dc
                e = abs(s - dc)
                sta += e - (sta >> ksta)
                lta += e - (lta >> klta)
                if n >= BUFN * 2 and (sta >> ksta) * 16 > (lta >> klta) * R:
                    got = True
                    break
            hits += got
        if hits >= trials / 2:
            hi = A
        else:
            lo = A
    return hi


def self_retrigger(kind, A, T=2, sigma=8.0, trials=60, rng=np.random.default_rng(3)):
    """One isolated pulse: how many buffers trigger, and how many of those contain
    no pulse sample above the noise sigma (i.e. triggered by the filter's own ringing)."""
    tot, ghost = 0, 0
    for k in range(trials):
        det = Existing(T)
        det.warm(2048)
        dc = 2048
        x = dc + rng.normal(0, sigma, BUFN * NB)
        start = BUFN * 2 + rng.integers(0, BUFN)
        p = pulse(kind, A, BUFN * 3)
        active = np.zeros(BUFN * NB, bool)
        active[start:start + len(p)] = np.abs(p) > sigma
        x[start:start + len(p)] += p
        x = np.clip(np.round(x), 0, 4095)
        for b in range(2):
            det.buffer(x[b * BUFN:(b + 1) * BUFN])
        for b in range(2, NB):
            if det.buffer(x[b * BUFN:(b + 1) * BUFN]):
                tot += 1
                if not active[b * BUFN:(b + 1) * BUFN].any():
                    ghost += 1
    return tot / trials, ghost / trials


if __name__ == "__main__":
    step_response()
    print()
    print("Single isolated pulse, existing detector (T=2, sigma=8): triggered buffers per pulse "
          "[of which buffers with no pulse energy above noise]")
    for kind in ("spike", "sharp"):
        for A in (300, 1000, 2000):
            t, g = self_retrigger(kind, A)
            print("  %-6s A=%4d: %.2f buffers  [%.2f ghost]" % (kind, A, t, g))
    print()
    print("Existing detector, minimum detectable pulse amplitude (noise sigma = 8 counts):")
    for T in (2, 6, 12):
        r = {k: min_detect(k, T) for k in ("sharp", "smooth", "smooth5k")}
        print("  trigthresh+trigcomp=%2d: sharp %6.0f   smooth 10kHz %6.0f (%.1fx)   smooth 5kHz %6.0f (%.1fx)"
              % (T, r["sharp"], r["smooth"], r["smooth"] / r["sharp"], r["smooth5k"], r["smooth5k"] / r["sharp"]))
    print()
    print("Alternative STA/LTA (dc EMA 256 samples, STA EMA 32, LTA EMA 16384), ratio threshold R/16:")
    for R in (64, 96):
        r = {k: stalta_min_detect(k, R) for k in ("sharp", "smooth", "smooth5k")}
        print("  R=%3d (%.1fx LTA): sharp %6.0f   smooth 10kHz %6.0f (%.1fx)   smooth 5kHz %6.0f (%.1fx)"
              % (R, R / 16, r["sharp"], r["smooth"], r["smooth"] / r["sharp"], r["smooth5k"], r["smooth5k"] / r["sharp"]))
