"""Flash an elf, wait for boot, collect console status lines for N seconds and summarise.

usage: flashmeasure.py <elf> <seconds_after_arming> <tag>
"""
import re
import subprocess
import sys
import time
import numpy as np
import serial

CLI = r"C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe"
elf, secs, tag = sys.argv[1], float(sys.argv[2]), sys.argv[3]
out = subprocess.run([CLI, "-c", "port=SWD", "mode=UR", "-w", elf, "-v", "-rst"], capture_output=True, text=True).stdout
print("flash:", "verified" if "verified successfully" in out else "FAILED\n" + out[-800:])
pat = re.compile(r"S (\d+) tr=(\d+) th=(\d+) pt=(\d+) g=(-?\d+) nz=(\d+) nm=(\d+) ov=(\d+) lt=(\d+) jb=(\d+) ss=(\d+)(?: isr=(\d+)/(\d+)%)?")
rows = []
with serial.Serial("COM7", 115200, timeout=0.2) as s, open(tag + "_console.log", "w", encoding="utf-8") as log:
    buf = b""
    t = time.time()
    armed = None
    while time.time() - t < 240 + secs:
        buf += s.read(8192)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.decode("ascii", "replace").rstrip("\r")
            log.write("%.3f %s\n" % (time.time(), line))
            if armed is None and "Arming" in line:
                armed = time.time()
            m = pat.search(line)
            if m and armed is not None and time.time() - armed > 5:
                rows.append([int(v) if v is not None else -1 for v in m.groups()])
        if armed is not None and time.time() - armed > secs + 5:
            break
if not rows:
    print("no status lines captured")
    sys.exit()
r = np.array(rows)
print("%s: %d status lines" % (tag, len(r)))
if (r[:, 11] >= 0).any():
    print("  ISR load avg%%: median %d (range %d-%d)   peak%%: max %d, median %d" % (
        np.median(r[:, 11]), r[:, 11].min(), r[:, 11].max(), r[:, 12].max(), np.median(r[:, 12])))
dt = max(r[-1, 0] - r[0, 0], 1)
print("  triggers/s %.2f  overruns(ov) +%d  late(lt) +%d  jabber +%d" % (
    (r[-1, 1] - r[0, 1]) / dt, r[-1, 7] - r[0, 7], r[-1, 8] - r[0, 8], r[-1, 9] - r[0, 9]))
print("  thresh median %d (%d-%d)  gain %s  noise median %d  near-miss/line median %d" % (
    np.median(r[:, 2]), r[:, 2].min(), r[:, 2].max(), sorted(set(r[:, 4])), np.median(r[:, 5]), np.median(r[:, 6])))
