"""Check ENDSEQ behaviour on the wire: for each batch of sample packets, find its ENDSEQ status packet,
how long after the batch's last sample it arrived, its batch id and its adcpktssent count.

usage: endseq_check.py <udp.bin from udpcap.py>
"""
import struct
import sys
from pktdecode import read_capture

recs = read_capture(sys.argv[1])
batches = {}      # batch id -> dict(first, last, n)
order = []
endseqs = []
for pc, d in recs:
    if len(d) == 1472 and d[3] == 4:
        w1 = struct.unpack_from("<I", d, 4)[0]
        b = (w1 >> 8) & 0xff
        if b not in batches or pc - batches[b]["last"] > 5:
            batches[b] = dict(first=pc, last=pc, n=0)
            order.append(b)
        batches[b]["last"] = pc
        batches[b]["n"] += 1
    elif len(d) == 156 and d[3] == 1:
        aux1 = struct.unpack_from("<I", d, 116)[0]
        pktssent = struct.unpack_from("<H", d, 94)[0]
        endseqs.append(dict(pc=pc, batch=aux1 & 0xff, pktssent=pktssent))

print("sample batches: %d   ENDSEQ packets: %d" % (len(order), len(endseqs)))
print(" batch  samples  ENDSEQ delay after last sample   ENDSEQ batch id   pktssent")
for b in order:
    info = batches[b]
    e = next((x for x in endseqs if x["pc"] >= info["last"] - 0.001 and x["pc"] - info["last"] < 30), None)
    if e is None:
        print(" %5d  %7d   (no ENDSEQ within 30 s)" % (b, info["n"]))
    else:
        print(" %5d  %7d   %8.1f ms                      %3d %s          %3d %s" % (
            b, info["n"], (e["pc"] - info["last"]) * 1e3, e["batch"], "OK" if e["batch"] == b else "WRONG",
            e["pktssent"], "OK" if e["pktssent"] == info["n"] else "MISMATCH"))
