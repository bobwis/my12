"""Console logger that also requests a status print (Ctrl-D) every `period` seconds.

Ctrl-D output is ~1.3 KB of blocking UART output in the LP task (which also runs
the 10ms/100ms AGC), so keep `period` long (default 10 s) to limit the observer effect.
"""
import sys
import time
import serial

dur = float(sys.argv[1])
out = sys.argv[2]
period = float(sys.argv[3]) if len(sys.argv) > 3 else 10.0
with serial.Serial("COM7", 115200, timeout=0.2) as s, open(out, "a", encoding="utf-8") as f:
    t0 = time.time()
    next_poll = t0 + 1.0
    pending = b""
    while time.time() - t0 < dur:
        if period > 0 and time.time() >= next_poll:
            s.write(b"\x04")
            next_poll += period
        pending += s.read(4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            f.write("%.3f %s\n" % (time.time(), line.decode("ascii", "replace").rstrip("\r")))
            f.flush()
