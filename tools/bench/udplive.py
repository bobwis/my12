"""Record detector UDP (port 5000) like udpcap.py, flushing every packet, and print one summary line
per interval: sample packets, ENDSEQ (type 1) and TIMED (type 2) status, last packet number, pknum gaps.
usage: udplive.py <out.bin> <secs> <report_every_secs>"""
import socket
import struct
import sys
import time

out, dur, every = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
s.bind(("0.0.0.0", 5000))
s.settimeout(0.2)
t_end = time.time() + dur
nxt = time.time() + every
cnt = {"S": 0, "E": 0, "T": 0, "?": 0}
lastpk = None
gaps = 0
lastrx = None
with open(out, "wb") as f:
    while time.time() < t_end:
        try:
            d, a = s.recvfrom(4096)
            now = time.time()
            f.write(struct.pack("<dH", now, len(d)) + d)
            f.flush()
            lastrx = now
            pk = d[0] | d[1] << 8 | d[2] << 16
            if lastpk is not None and pk != lastpk + 1:
                gaps += 1
            lastpk = pk
            cnt["S" if len(d) == 1472 else {1: "E", 2: "T"}.get(d[3], "?")] += 1
        except socket.timeout:
            pass
        if time.time() >= nxt:
            nxt += every
            print("%s rx: samples=%d endseq=%d timed=%d other=%d lastpk=%s pkgaps=%d last_rx=%s" % (
                time.strftime("%H:%M:%S"), cnt["S"], cnt["E"], cnt["T"], cnt["?"], lastpk, gaps,
                time.strftime("%H:%M:%S", time.localtime(lastrx)) if lastrx else "-"), flush=True)
            cnt = dict.fromkeys(cnt, 0)
            gaps = 0
