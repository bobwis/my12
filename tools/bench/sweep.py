"""Detector sweep: for each detector and CH1 stroke amplitude, measure % of 1 Hz strokes detected and
noise-only triggered buffers per second, at fixed PGA gain with AGC off.

usage: sweep.py <elf> <gain digit> <secs per point> <detectors e.g. 0,1> <amps mV e.g. 200,100,50> [ratios_q4 e.g. 64]
Ratios only apply to the STA/LTA detector (1); each listed ratio is swept.
"""
import re
import socket
import struct
import subprocess
import sys
import threading
import time
import numpy as np
import serial
from psg import PSG

TOOLS = r"C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins"
NM = TOOLS + r"\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-nm.exe"
CLI = TOOLS + r"\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe"
STAT = re.compile(r"S (\d+) tr=(\d+) th=(\d+) pt=(\d+) g=(-?\d+) nz=(\d+) .*?isr=(\d+)/(\d+)% d=(\d+)")


class Console:
    def __init__(self):
        self.s = serial.Serial("COM7", 115200, timeout=0.2)
        self.lines = []
        self.run = True
        threading.Thread(target=self._rx, daemon=True).start()

    def _rx(self):
        buf = b""
        while self.run:
            buf += self.s.read(4096)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                self.lines.append((time.time(), line.decode("ascii", "replace").rstrip("\r")))

    def key(self, k):
        self.s.write(k)
        time.sleep(1.5)

    def last_status(self, after=0.0):
        for t, l in reversed(self.lines):
            m = STAT.search(l)
            if m and t > after:
                return [int(v) for v in m.groups()]
        return None

    def wait_status(self):
        t = time.time()
        while time.time() - t < 5:
            st = self.last_status(t)
            if st:
                return st
            time.sleep(0.2)
        return None

    def text_since(self, t):
        return "\n".join(l for tt, l in self.lines if tt > t)


def capture(secs):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
    s.bind(("0.0.0.0", 5000))
    s.settimeout(0.2)
    out, t_end = [], time.time() + secs
    while time.time() < t_end:
        try:
            d, _ = s.recvfrom(4096)
            out.append((time.time(), d))
        except socket.timeout:
            pass
    s.close()
    return out


def score(pkts, secs):
    samples = [(pc, d) for pc, d in pkts if len(d) == 1472 and d[3] == 4]
    status = [d for pc, d in pkts if len(d) == 156 and d[3] in (1, 2)]
    clk = np.median([struct.unpack_from("<I", d, 88)[0] for d in status]) if status else 108e6
    if len(samples) < 3:
        return dict(det=0.0, noise=len(samples) / secs, n=len(samples), endseq=len(status))
    pc = np.array([p for p, _ in samples])
    frac = np.array([struct.unpack_from("<I", d, 12)[0] / clk for _, d in samples])
    t = np.round(pc - frac) + frac
    best = max(((abs(np.exp(2j * np.pi * (t % T) / T).mean()), T) for T in np.arange(0.9995, 1.0005, 0.000002)))
    T = best[1]
    ph = t % T
    h, e = np.histogram(ph, bins=1000, range=(0, T))
    c = e[np.convolve(h, np.ones(5), "same").argmax()]
    d = np.abs(((ph - c) + T / 2) % T - T / 2)
    stim = d < 0.010
    nstim = max(int(round(secs / T)), 1)
    detected = len(np.unique(np.round((t[stim] - t.min()) / T)))
    return dict(det=100.0 * min(detected, nstim) / nstim, noise=(~stim).sum() / secs, n=len(samples),
                endseq=len(status), vs=best[0])


def main():
    elf, gain, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
    dets = [int(x) for x in sys.argv[4].split(",")]
    amps = [int(x) for x in sys.argv[5].split(",")]
    ratios = [int(x) for x in sys.argv[6].split(",")] if len(sys.argv) > 6 else [64]
    sym = {l.split()[2]: int(l.split()[0], 16) for l in subprocess.run([NM, elf], capture_output=True, text=True).stdout.splitlines() if len(l.split()) == 3}
    con = Console()
    g = PSG()
    amp0 = g.read_all()[15]
    try:
        st = con.wait_status()
        t0 = time.time()
        con.key(b"\x03")								# toggle AGC; make sure it ends up OFF
        if "AGC is ON" in con.text_since(t0):
            con.key(b"\x03")
        con.key(gain.encode())
        print("AGC off, gain %s" % gain, flush=True)
        for det in dets:
            st = con.wait_status()
            if st and st[8] != det:
                con.key(b"\x14")
                st = con.wait_status()
            print("detector %d (status d=%s)" % (det, st[8] if st else "?"), flush=True)
            for ratio in (ratios if det == 1 else [None]):
                if ratio is not None:
                    subprocess.run([CLI, "-c", "port=SWD", "mode=HOTPLUG", "-w32", hex(sym["stalta_ratio_q4"]), str(ratio)], capture_output=True)
                for a in amps:
                    g.set_amp1_mV(a)
                    if int(g.read_all()[15]) != a:
                        raise RuntimeError("generator amplitude not set")
                    time.sleep(5)
                    pk = capture(secs)
                    r = score(pk, secs)
                    st = con.last_status()
                    print("  det=%d ratio=%-4s amp=%4d mV : detected %5.1f%%  noise %5.2f/s  sample pkts %3d  status pkts %3d | th=%s g=%s nz=%s isr=%s/%s%%" % (
                        det, ratio if ratio else "-", a, r["det"], r["noise"], r["n"], r["endseq"],
                        st[2] if st else "?", st[4] if st else "?", st[5] if st else "?", st[6] if st else "?", st[7] if st else "?"), flush=True)
    finally:
        g.write_raw(15, amp0)
        g.close()
        con.run = False


if __name__ == "__main__":
    main()
