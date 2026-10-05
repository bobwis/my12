"""Per-stimulus analysis for a 1 Hz generator stimulus: detection, waveform-in-buffer, follow-on triggers.

usage: analyze_stim.py <udp.bin> [console.log] [skip_seconds]
"""
import re
import sys
import numpy as np
from pktdecode import read_capture, decode

WAVE_PK = 150          # counts: noise-only buffers peak ~40-50 at gain 8; stimulus buffers several hundred

udp = sys.argv[1]
con = sys.argv[2] if len(sys.argv) > 2 else None
skip = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0

recs = read_capture(udp)
s, st = decode(recs)
t0 = recs[0][0] + skip
s = [x for x in s if x["pc"] >= t0]
st = [x for x in st if x["pc"] >= t0]
pc = np.array([x["pc"] for x in s])
pk = np.array([np.abs(x["adc"] - np.median(x["adc"])).max() for x in s])
tsms = np.array([x["ts"] for x in s]) / 108e6 * 1e3

ev, cur = [], [0]
for i in range(1, len(pc)):
    if pc[i] - pc[cur[0]] < 0.15:
        cur.append(i)
    else:
        ev.append(cur)
        cur = [i]
ev.append(cur)
dur = recs[-1][0] - t0
nstim = int(round(dur))
first_wave = [pk[e[0]] > WAVE_PK for e in ev]
any_wave = [any(pk[j] > WAVE_PK for j in e) for e in ev]
print("analysed %.0f s (~%d stimuli): events %d (%.0f%%)" % (dur, nstim, len(ev), 100 * len(ev) / max(nstim, 1)))
print("events whose first buffer holds the waveform: %d/%d (%.0f%%); any buffer: %d" % (
    sum(first_wave), len(ev), 100 * sum(first_wave) / max(len(ev), 1), sum(any_wave)))
print("buffers per event histogram:", np.bincount([len(e) for e in ev]))
fol = sorted(round((tsms[e[k]] - tsms[e[0]]) % 1000, 1) for e in ev for k in range(1, len(e)))
print("follow-on buffers: %d; delays after first (ms): %s" % (len(fol), fol[:60]))
print("first-buffer peaks without waveform:", sorted(int(pk[e[0]]) for e in ev if pk[e[0]] <= WAVE_PK))
if st:
    a, b = st[0], st[-1]
    print("status: thresh %d..%d gain %s noise %s udpover %d->%d jabcnt %d->%d" % (
        min(x["thresh"] for x in st), max(x["thresh"] for x in st), sorted({x["gain"] for x in st}),
        sorted({x["noise"] for x in st})[:12], a["udpover"], b["udpover"], a["jabcnt"], b["jabcnt"]))

if con:
    rows = []
    for line in open(con, encoding="utf-8", errors="replace"):
        m = re.search(r"S (\d+) tr=(\d+) th=(\d+) pt=(\d+) g=(-?\d+) nz=(\d+) nm=(\d+) ov=(\d+) lt=(\d+) jb=(\d+) ss=(\d+)", line)
        if m:
            rows.append([int(v) for v in m.groups()])
    if rows:
        r = np.array(rows)
        print("\nconsole status lines: %d" % len(r))
        print("  thresh min/median/max: %d/%d/%d   gain: %s   noise median %d" % (
            r[:, 2].min(), np.median(r[:, 2]), r[:, 2].max(), sorted(set(r[:, 4])), np.median(r[:, 5])))
        print("  near-misses per line median %d  max %d" % (np.median(r[:, 6]), r[:, 6].max()))
        print("  last line: tr=%d ov=%d lt=%d jb=%d" % (r[-1, 1], r[-1, 7], r[-1, 8], r[-1, 9]))
        print("  triggers/s over status window: %.2f" % ((r[-1, 1] - r[0, 1]) / max(r[-1, 0] - r[0, 0], 1)))
