"""Record every UDP packet from the detector on port 5000 to a binary file.

Record format: <d PC time><H length><payload>. Optional console status polling
(Ctrl-D every N seconds) is done by a separate process (statpoll.py).
"""
import socket
import struct
import sys
import time

out = sys.argv[1]
dur = float(sys.argv[2])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
s.bind(("0.0.0.0", 5000))
s.settimeout(0.2)
n = 0
t_end = time.time() + dur
with open(out, "wb") as f:
    while time.time() < t_end:
        try:
            d, a = s.recvfrom(4096)
        except socket.timeout:
            continue
        f.write(struct.pack("<dH", time.time(), len(d)) + d)
        n += 1
print("captured", n, "packets to", out)
