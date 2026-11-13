"""Check kernel modules against a kernel's exported symbol CRCs (ksymcrc.py):
  python3 -I modversions.py CRCS.json MODULE.ko...
Reads each module's __versions section (32-bit CRC + 60-byte name per
entry) and vermagic, and lists symbols the kernel lacks or whose CRC differs."""
import json, struct, sys

def sections(d):
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2e)
    sh = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
    so = sh[shstrndx][4]
    return {d[so + s[0]:d.index(b"\0", so + s[0])].decode(): d[s[4]:s[4] + s[5]] for s in sh}

crcs = {k: int(v, 16) for k, v in json.load(open(sys.argv[1]))["crcs"].items()}
bad = 0
for path in sys.argv[2:]:
    sec = sections(open(path, "rb").read())
    v = sec["__versions"]
    want = {v[i + 4:i + 64].split(b"\0")[0].decode(): struct.unpack_from("<I", v, i)[0] for i in range(0, len(v), 64)}
    magic = [s for s in sec[".modinfo"].split(b"\0") if s.startswith(b"vermagic=")][0].decode()
    missing = sorted(s for s in want if s not in crcs)
    differ = sorted(s for s in want if s in crcs and crcs[s] != want[s])
    bad += len(missing) + len(differ)
    print("%s: %d symbols, missing %s, differ %s; %s" % (path.rsplit("/", 1)[-1], len(want), missing or "none", differ or "none", magic))
sys.exit(1 if bad else 0)
