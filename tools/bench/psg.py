"""Minimal Junctek PSG9000 control over its CH340 serial port (protocol decoded from the vendor app).

VERIFIED on this unit (DDS-60M): read and write use the SAME register number
(w15=650 changed r15). The vendor app's numbers (w21 wave, w25 amp...) are 10
higher than this unit's and must NOT be used. Writes return ':ok' even when a
register ignores the value, so always confirm by reading back a changed value.
Frequency registers are value,unit with value in mHz (r13=20000000,0 = 20 kHz).
Known on this unit: 10 outputs, 11/12 wave (101 = arb memory 01), 13/14 freq,
15/16 amp mV, 17/18 offset (1000 = 0 V). Burst/trigger setup lives somewhere in
r24, r26, r40, r41, r43, r60, r61 (see psg_bob_burst_1hz.json vs
psg_original_full.json) - not individually decoded yet.
"""
import time
import serial

# read-register numbers for the settings experiments touch
R_OUT = 10       # "ch1,ch2" output on/off
R_WAVE1 = 11     # ch1 waveform (101 = arbitrary memory used for the lightning recording)
R_FREQ1 = 13     # "value,unit"
R_FREQ2 = 14
R_AMP1 = 15      # mV
R_AMP2 = 16
R_OFS1 = 17      # 1000 = 0 V


class PSG:
    def __init__(self, port="COM11"):
        self.s = serial.Serial(port, 115200, timeout=0.1)

    def close(self):
        self.s.close()

    def _xfer(self, cmd, wait=1.0):
        self.s.reset_input_buffer()
        self.s.write(cmd.encode("ascii") + b"\r\n")
        buf = b""
        t_end = time.time() + wait
        while time.time() < t_end:
            chunk = self.s.read(4096)
            if chunk:
                buf += chunk
                t_end = time.time() + 0.25
        return buf.decode("ascii", errors="replace")

    def read_all(self):
        regs = {}
        for line in self._xfer(":r00=86.").splitlines():
            line = line.strip()
            if line.startswith(":r") and "=" in line:
                k, v = line[2:].split("=", 1)
                regs[int(k)] = v.rstrip(".")
        return regs

    def write_raw(self, reg, value):
        """Write register reg (same number as it reads back); value is the raw string."""
        cmd = ":w%02d=%s." % (reg, value)
        resp = self._xfer(cmd, wait=0.6).strip()
        if resp != ":ok":
            raise RuntimeError("PSG write %s -> %r" % (cmd, resp))
        return resp

    def set_amp1_mV(self, mv):
        self.write_raw(R_AMP1, str(int(mv)))

    def get(self, read_reg):
        return self.read_all()[read_reg]

    def snapshot(self, regs=(R_OUT, R_WAVE1, R_FREQ1, R_FREQ2, R_AMP1, R_AMP2, R_OFS1)):
        allr = self.read_all()
        return {r: allr[r] for r in regs}

    def restore(self, snap):
        for r, v in snap.items():
            if r in (R_FREQ1, R_FREQ2):
                val, unit = v.split(",")
                v = "%d,%s" % (int(val), unit)
            elif r != R_OUT:
                v = str(int(v))
            self.write_raw(r, v)
        now = self.snapshot(tuple(snap))
        bad = {r: (snap[r], now[r]) for r in snap if snap[r] != now[r]}
        if bad:
            raise RuntimeError("PSG restore mismatch: %r" % bad)
