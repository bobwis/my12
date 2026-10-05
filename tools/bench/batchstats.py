"""Batch statistics for a capture window: samples per batch (from ENDSEQ adcpktssent), events per 1 Hz
stimulus (sample packets grouped by time), packets per stimulus, off-stimulus (noise) packets, order check.
usage: batchstats.py <capture.bin> <HH:MM:SS from> <HH:MM:SS to>"""
import collections
import struct
import sys
import time
import numpy as np
from pktdecode import read_capture

recs = read_capture(sys.argv[1])
day = time.strftime("%Y-%m-%d ", time.localtime(recs[0][0]))
t0 = time.mktime(time.strptime(day + sys.argv[2], "%Y-%m-%d %H:%M:%S"))
t1 = time.mktime(time.strptime(day + sys.argv[3], "%Y-%m-%d %H:%M:%S"))
recs = [(pc, d) for pc, d in recs if t0 <= pc < t1]
secs = t1 - t0

per_batch = collections.Counter()
samples, last, disorder = [], None, 0
for pc, d in recs:
    pk = d[0] | d[1] << 8 | d[2] << 16
    if last is not None and pk != last + 1:
        disorder += 1
    last = pk
    if len(d) == 1472 and d[3] == 4:
        frac = struct.unpack_from("<I", d, 12)[0] / 108e6
        samples.append(round(pc - frac) + frac)
    elif len(d) == 156 and d[3] == 1:
        per_batch[struct.unpack_from("<H", d, 94)[0]] += 1

t = np.array(sorted(samples))
# group sample packets into stimulus events: anything within 300 ms of the previous packet
groups = []
for x in t:
    if groups and x - groups[-1][-1] < 0.3:
        groups[-1].append(x)
    else:
        groups.append([x])
sizes = collections.Counter(len(g) for g in groups)
nb = sum(per_batch.values())
print("window %s-%s (%.0f s): sample pkts %d, ENDSEQ %d, numbering breaks %d" % (sys.argv[2], sys.argv[3], secs, len(t), nb, disorder))
print("  samples per batch (ENDSEQ count):", dict(sorted(per_batch.items())),
      " mean %.2f" % (sum(k * v for k, v in per_batch.items()) / max(nb, 1)))
print("  packet groups (<300 ms apart): %d = %.2f/s; packets per group: %s" % (len(groups), len(groups) / secs, dict(sorted(sizes.items()))))
print("  batches per group: %.2f" % (nb / max(len(groups), 1)))
