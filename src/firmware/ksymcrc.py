"""Exported symbols and their modversions CRCs from a decompressed ARM
Linux 4.1 kernel image (raw, loaded at 0x80008000: Kobo's kernels use a
2 GB user split):  python3 -I ksymcrc.py vmlinux.bin OUT.json
__ksymtab and __ksymtab_gpl are one run of (value, name pointer) pairs
whose names lie in __ksymtab_strings; __kcrctab and __kcrctab_gpl follow
directly, one 32-bit CRC per entry, in the same order."""
import json, re, struct, sys
BASE = 0x80008000
d = open(sys.argv[1], "rb").read()
n = len(d)
w = struct.unpack_from("<%dI" % (n // 4), d)
IDENT = re.compile(rb"[A-Za-z0-9_]+\0")

def name(p):
    o = p - BASE
    if not 0 < o < n:  # the first name follows the last CRC directly
        return None
    m = IDENT.match(d, o)
    return m and m.group()[:-1].decode()

best = None
for par in (0, 1):
    i, start, names = par, None, []
    while i + 1 < len(w):
        s = name(w[i + 1])
        if s:
            if start is None: start, names = i, []
            names.append(s)
        elif start is not None:
            if not best or len(names) > len(best[1]): best = (start, names, i)
            start = None
        i += 2
start, names, end = best
crc = w[end:end + len(names)]
out = {"symbols": len(names), "ksymtab": hex(BASE + 4 * start), "kcrctab": hex(BASE + 4 * end),
       "crcs": {s: "0x%08x" % c for s, c in zip(names, crc)}}
json.dump(out, open(sys.argv[2], "w"), indent=0, sort_keys=True)
print(out["symbols"], "symbols; module_layout", out["crcs"].get("module_layout"))
