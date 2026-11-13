"""All symbols (kallsyms) of a decompressed ARM Linux 4.1 kernel image,
loaded at 0x80008000:  python3 -I kallsyms.py vmlinux.bin OUT.txt
Writes "ADDRESS TYPE NAME" lines. Layout (absolute addresses, 4.1):
kallsyms_addresses, kallsyms_num_syms, kallsyms_names, kallsyms_markers,
kallsyms_token_table, kallsyms_token_index, each 4-byte aligned."""
import struct, sys
d = open(sys.argv[1], "rb").read()
n = len(d)


def u16(o): return struct.unpack_from("<H", d, o)[0]
def u32(o): return struct.unpack_from("<I", d, o)[0]


def token_table():
    for p in range(0, n - 512, 4):
        if u16(p) != 0 or u16(p + 2) == 0:
            continue
        idx = [u16(p + 2 * i) for i in range(256)]
        if any(b <= a for a, b in zip(idx, idx[1:])) or idx[255] > 2048:
            continue
        for pad in range(0, 64):
            t = p - pad - 1
            last = d.rfind(b"\0", 0, t)
            start = last + 1 - idx[255]
            if start <= 0 or d[t] != 0:
                continue
            if all(d[start + idx[i + 1] - 1] == 0 and d[start + idx[i]] != 0 for i in range(255)):
                return start, idx
    raise SystemExit("token table not found")


start, idx = token_table()
tokens = [d[start + o:d.index(b"\0", start + o)].decode("latin-1") for o in idx]
# markers: u32 offsets into names, ascending from 0, ending before the table
m_end = start
while u32(m_end - 4) == 0 and m_end - 4 > start - 8: m_end -= 4  # padding
nmark = 1
while u32(m_end - 4 * nmark) != 0: nmark += 1
mark = m_end - 4 * nmark
# names: walk back from markers to the count that decodes exactly
best = None
for back in range(4, 4 * 1024 * 1024, 4):
    o = mark - back
    num = u32(o)
    if not 256 * (nmark - 1) < num <= 256 * nmark:
        continue
    for skip in (4, 8, 12, 16):  # labels may be aligned beyond 4 bytes
        p = o + skip
        for _ in range(num):
            p += 1 + d[p]
            if p > mark: break
        if mark - 16 < p <= mark:
            best = (o, num, o + skip); break
    if best: break
assert best, "names not found"
o, num, p = best
a_end = o
while u32(a_end - 4) == 0: a_end -= 4  # padding after the addresses
addrs = struct.unpack_from("<%dI" % num, d, a_end - 4 * num)
with open(sys.argv[2], "w") as f:
    for a in addrs:
        k = d[p]
        s = "".join(tokens[c] for c in d[p + 1:p + 1 + k])
        p += 1 + k
        f.write("%08x %s %s\n" % (a, s[0], s[1:]))
print(num, "symbols")
