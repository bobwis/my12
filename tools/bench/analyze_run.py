"""Summarize one experiment run: console AGC state over time + UDP batches vs the 1 Hz stimulus."""
import re
import sys
import numpy as np
from pktdecode import read_capture, decode, batches

con_path, udp_path = sys.argv[1], sys.argv[2]

print("== Console status lines (thresh / gain / noise / cumulative triggers) ==")
for line in open(con_path, encoding="utf-8", errors="replace"):
    m = re.search(r"triggers:(\d+), gain:(\S+), noise:(\d+), thresh:(\d+)", line)
    if m:
        print("  %s  trig=%5s gain=%s noise=%3s thresh=%3s" % (line[:8], m.group(1), m.group(2).rstrip(","), m.group(3), m.group(4)))
    elif re.search(r"Jabbering|Arming|error|fail", line, re.I):
        print("  " + line.rstrip())

recs = read_capture(udp_path)
samples, status = decode(recs)
bs = batches(samples)
print("\n== UDP: %d packets, %d sample packets, %d status packets, %d batches ==" % (len(recs), len(samples), len(status), len(bs)))
if not recs:
    sys.exit()
t0 = recs[0][0]
for s in status:
    print("  %7.1fs %-6s thresh=%3d gain=%d noise=%3d batch=%3d pktssent=%3d trigcount=%5d udpover=%d jabcnt=%d" % (
        s["pc"] - t0, s["type"], s["thresh"], s["gain"], s["noise"], s["batch"], s["pktssent"], s["trigcount"], s["udpover"], s["jabcnt"]))

if bs:
    # stimulus alignment: phase of each batch start within the 1 s stimulus period (PC clock)
    first = np.array([b["pc_first"] for b in bs])
    phase = (first - first[0] + 0.5) % 1.0 - 0.5          # relative to first batch, in seconds
    aligned = np.abs(phase) < 0.05
    n = np.array([b["n"] for b in bs])
    span = np.array([b["span_ms"] for b in bs])
    dur = recs[-1][0] - recs[0][0]
    print("\n  capture span %.0f s; batches aligned with stimulus phase: %d, unaligned (noise/other): %d" % (dur, aligned.sum(), (~aligned).sum()))
    for label, sel in (("aligned", aligned), ("unaligned", ~aligned)):
        if sel.any():
            print("  %-9s buffers/batch: mean %.1f  median %d  max %d | train span ms: median %.1f  max %.1f" % (
                label, n[sel].mean(), np.median(n[sel]), n[sel].max(), np.median(span[sel]), span[sel].max()))
    print("\n  first 30 batches (t, phase ms, buffers, span ms):")
    for b, ph in list(zip(bs, phase))[:30]:
        print("   %7.2fs  %+6.0f  %3d  %6.1f" % (b["pc_first"] - t0, ph * 1e3, b["n"], b["span_ms"]))
