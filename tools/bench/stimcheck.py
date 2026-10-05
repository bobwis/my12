"""Passive check: generator registers vs original snapshot, then capture N s and test for 1 s periodicity."""
import subprocess
import sys
import numpy as np
from psg import PSG
from pktdecode import read_capture, decode

ORIG = {20: "2000", 21: "00000", 24: "00,01,00,00", 26: "02", 10: "1,1", 11: "101",
        13: "000020000000,0", 14: "000000001000,0", 15: "00700", 16: "03300", 17: "1000"}
dur = sys.argv[1] if len(sys.argv) > 1 else "60"
g = PSG(); now = g.read_all(); g.close()
print("generator regs differing from original:", {k: (v, now.get(k)) for k, v in ORIG.items() if now.get(k) != v} or "none")
subprocess.run([sys.executable, "udpcap.py", "stimcheck.bin", dur], check=True)
recs = read_capture("stimcheck.bin"); samples, status = decode(recs)
print("sample packets:", len(samples))
if len(samples) > 3:
    clk = np.median([s["clktrim"] for s in status]) if status else 108e6
    t = np.array([s["epoch"] + s["ts"] / clk for s in samples])
    vs1 = abs(np.exp(2j * np.pi * (t % 1.0)).mean())
    peak = max(np.abs(s["adc"] - np.median(s["adc"])).max() for s in samples)
    print("1-second vector strength %.2f (1.0 = perfectly periodic, ~%.2f = random)" % (vs1, 1 / np.sqrt(len(t))))
    print("largest excursion in any buffer: %d counts" % peak)
