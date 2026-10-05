"""Non-halting SWD sampler of AGC/trigger variables (ST-Link hot-plug reads, D-cache is off).

usage: swdpoll.py <elf> <duration_s> <out.csv> [period_s]
Addresses come from the elf actually flashed (they move between builds).
"""
import csv
import re
import subprocess
import sys
import time

TOOLS = r"C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins"
NM = TOOLS + r"\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-nm.exe"
CLI = TOOLS + r"\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe"

# name, size, signed, offset-within-symbol
VARS = [("trigthresh", 2, False, 0), ("pretrigthresh", 2, False, 0), ("pretrigcnt", 4, False, 0),
        ("pgagain", 2, True, 0), ("globaladcnoise", 4, False, 0), ("meanwindiff", 2, True, 0),
        ("jabbertimeout", 4, True, 0), ("sigsuppress", 2, False, 0), ("adcbatchid", 1, False, 0),
        ("globaladcavg", 4, False, 0), ("t1sec", 4, False, 0), ("trigcomp", 4, False, 0),
        ("gpslocked", 1, False, 0), ("trigbuflate", 4, False, 0),
        ("statuspkt.adcudpover", 4, False, 0x78), ("statuspkt.trigcount", 4, False, 0x7C),
        ("statuspkt.udpsent", 4, False, 0x80), ("statuspkt.jabcnt", 2, False, 0x86)]
OPTIONAL = {"trigcomp", "gpslocked", "trigbuflate"}


def symbols(elf):
    out = subprocess.run([NM, elf], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}


def main():
    elf, dur, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    period = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    sym = symbols(elf)
    plan = []
    for name, size, signed, off in VARS:
        base = name.split(".")[0]
        if base not in sym:
            if base in OPTIONAL:
                continue
            raise SystemExit("symbol %s not in %s" % (base, elf))
        plan.append((name, sym[base] + off, size, signed))
    # read each variable's containing aligned word(s)
    words = sorted({(a & ~3) for _, a, s, _ in plan} | {((a + s - 1) & ~3) for _, a, s, _ in plan})
    args = []
    for w in words:
        args += ["-r32", hex(w), "4"]
    with open(out, "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(["pc_time"] + [p[0] for p in plan])
        t_end = time.time() + dur
        while time.time() < t_end:
            t = time.time()
            r = subprocess.run([CLI, "-c", "port=SWD", "mode=HOTPLUG"] + args, capture_output=True, text=True)
            mem = {}
            for m in re.finditer(r"^(0x[0-9A-Fa-f]{8}) : ([0-9A-Fa-f]{8})", r.stdout, re.M):
                v = int(m.group(2), 16)
                a = int(m.group(1), 16)
                for i in range(4):
                    mem[a + i] = (v >> (8 * i)) & 0xff
            row = [round(t, 3)]
            for name, a, size, signed in plan:
                if all((a + i) in mem for i in range(size)):
                    v = int.from_bytes(bytes(mem[a + i] for i in range(size)), "little", signed=signed)
                else:
                    v = ""
                row.append(v)
            wr.writerow(row)
            f.flush()
            time.sleep(max(0.0, period - (time.time() - t)))


if __name__ == "__main__":
    main()
