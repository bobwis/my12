"""Read an arbitrary waveform memory from the PSG9000 (':bNN=0.' - read-only) and save/plot it."""
import sys
import time
import numpy as np
import serial

mem = int(sys.argv[1]) if len(sys.argv) > 1 else 0
with serial.Serial("COM11", 115200, timeout=0.2) as s:
    s.reset_input_buffer()
    s.write((":b%02d=0.\r\n" % mem).encode())
    buf = b""
    t_end = time.time() + 3.0
    while time.time() < t_end:
        c = s.read(65536)
        if c:
            buf += c
            t_end = time.time() + 1.0
txt = buf.decode("ascii", "replace").strip()
print("mem %02d: %d bytes; head: %r" % (mem, len(buf), txt[:80]))
body = txt.split("=", 1)[1].rstrip(".") if "=" in txt else ""
vals = [int(v) for v in body.replace("\r", "").replace("\n", "").split(",") if v.strip().lstrip("-").isdigit()]
if vals:
    a = np.array(vals)
    np.save("psg_mem%02d.npy" % mem, a)
    print("  points=%d min=%d max=%d mean=%.1f" % (len(a), a.min(), a.max(), a.mean()))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(11, 3.5))
        plt.plot(a, lw=0.8)
        plt.title("PSG9000 arbitrary memory %02d (%d points)" % (mem, len(a)))
        plt.xlabel("point"); plt.ylabel("raw value"); plt.grid(alpha=0.3); plt.tight_layout()
        plt.savefig("psg_mem%02d.png" % mem, dpi=110)
        print("  plot saved: psg_mem%02d.png" % mem)
    except ImportError:
        print("  (matplotlib not available - no plot)")
