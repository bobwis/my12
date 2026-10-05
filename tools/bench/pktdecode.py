"""Decode captured detector packets (from udpcap.py) into sample buffers and status records."""
import struct
import numpy as np

TIM2_HZ = 108e6     # nominal; status packets carry the GPS-measured value in clktrim


def read_capture(path):
    recs = []
    with open(path, "rb") as f:
        data = f.read()
    i = 0
    while i + 10 <= len(data):
        t, n = struct.unpack_from("<dH", data, i)
        i += 10
        recs.append((t, data[i:i + n]))
        i += n
    return recs


def decode(recs):
    samples, status = [], []
    for t, d in recs:
        seq = d[0] | d[1] << 8 | d[2] << 16
        ptype = d[3]
        if ptype == 4 and len(d) == 1472:
            w1, epoch, ts = struct.unpack_from("<III", d, 4)
            samples.append(dict(pc=t, seq=seq, uid=w1 >> 16, batch=(w1 >> 8) & 0xff,
                                rtsec=(w1 >> 2) & 0x3f, bufnum=w1 & 3, epoch=epoch, ts=ts,
                                adc=np.frombuffer(d, dtype="<u2", offset=16, count=728).astype(np.int32)))
        elif ptype in (1, 2) and len(d) == 156:
            clktrim, uid, pktssent, trigoff, adcbase = struct.unpack_from("<IHHHH", d, 88)
            sysup, netup, gpsup = struct.unpack_from("<III", d, 100)
            adcnoise, aux1, udpover, trigcount, udpsent = struct.unpack_from("<HIIII", d, 114)
            build, jabcnt = struct.unpack_from("<HH", d, 132)
            status.append(dict(pc=t, seq=seq, type="ENDSEQ" if ptype == 1 else "TIMED", clktrim=clktrim,
                               pktssent=pktssent, thresh=trigoff & 0xfff, gain=trigoff >> 12,
                               adcbase=adcbase & 0xfff, boost=(adcbase >> 12) & 1, noise=adcnoise,
                               batch=aux1 & 0xff, jabber=(aux1 >> 8) & 0xff, udpover=udpover,
                               trigcount=trigcount, udpsent=udpsent, build=build, jabcnt=jabcnt,
                               sysup=sysup, gpsup=gpsup))
    return samples, status


def batches(samples):
    """Group consecutive sample packets by batch id (a batch = one detected event train)."""
    out = []
    for s in samples:
        if out and out[-1]["batch"] == s["batch"] and s["pc"] - out[-1]["pc_last"] < 0.5:
            b = out[-1]
            b["n"] += 1
            b["ts_last"] = s["ts"]
            b["pc_last"] = s["pc"]
            b["bufs"].append(s)
        else:
            out.append(dict(batch=s["batch"], n=1, ts_first=s["ts"], ts_last=s["ts"],
                            pc_first=s["pc"], pc_last=s["pc"], bufs=[s]))
    for b in out:
        b["span_ms"] = ((b["ts_last"] - b["ts_first"]) & 0xffffffff) / TIM2_HZ * 1e3
    return out
