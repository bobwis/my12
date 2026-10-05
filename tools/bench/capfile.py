"""Parse lightsrv .cap files: raw detector UDP payloads concatenated (1472-byte samples type 4/0, 156-byte status type 1/2)."""
import struct
import numpy as np


def iter_packets(path):
    d = open(path, "rb").read()
    i, n = 0, len(d)
    while i + 4 <= n:
        t = d[i + 3]
        if t in (1, 2) and i + 156 <= n and d[i + 152:i + 156] == b"\xfe\xed\xc0\xde":
            yield "status", d[i:i + 156]
            i += 156
        elif t in (0, 4) and i + 1472 <= n:
            yield "sample", d[i:i + 1472]
            i += 1472
        else:
            i += 1      # resync


def sample_fields(p):
    w1, epoch, ts = struct.unpack_from("<III", p, 4)
    return dict(uid=w1 >> 16, batch=(w1 >> 8) & 0xff, epoch=epoch, ts=ts,
                adc=np.frombuffer(p, dtype="<u2", offset=16, count=728).astype(np.int32))


def status_fields(p):
    trigoff, adcbase = struct.unpack_from("<HH", p, 96)
    uid = struct.unpack_from("<H", p, 92)[0]
    noise = struct.unpack_from("<H", p, 114)[0]
    build = struct.unpack_from("<H", p, 132)[0]
    return dict(uid=uid, thresh=trigoff & 0xfff, gain=trigoff >> 12, noise=noise, build=build, type=p[3])
