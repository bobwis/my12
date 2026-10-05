"""Passive console logger for COM7 (read-only): timestamps each line into a log file."""
import sys
import time
import serial

dur = float(sys.argv[1]) if len(sys.argv) > 1 else 60
out = sys.argv[2] if len(sys.argv) > 2 else "console.log"
with serial.Serial("COM7", 115200, timeout=0.2) as s, open(out, "a", encoding="utf-8") as f:
    t0 = time.time()
    pending = b""
    while time.time() - t0 < dur:
        pending += s.read(4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            f.write("%s %s\n" % (time.strftime("%H:%M:%S"), line.decode("ascii", "replace").rstrip("\r")))
            f.flush()
