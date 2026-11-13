"""Make a per-kernel module set for the launcher (kernels/<release>/ on the Kobo):
  python3 -I kernel-set.py SRC_DIR OUT_ROOT RELEASE FIRMWARE STATUS [CRCS.json]
SRC_DIR holds the six modules built for one kernel (ashmem_linux, binder_linux,
kobo_madvise, kobo_zsmalloc, kobo_zram, kobo_pagecache). For another kernel
release of the same length the vermagic string is replaced in place (the
only change; nothing else in the module is touched). With CRCS.json (from
ksymcrc.py for the target kernel) every module's __versions entries must be
present with the same CRC, otherwise nothing is written: such a kernel needs
the modules rebuilt. binder/ashmem/madvise also look up 13 unexported
functions by name at load time; compare their addresses/code separately
(kallsyms.py, docs/固件兼容.md) before calling a set more than untested.
Writes OUT_ROOT/RELEASE/{*.ko,SHA256SUMS,firmware,status}."""
import hashlib, json, os, re, struct, sys

NAMES = ["ashmem_linux", "binder_linux", "kobo_madvise", "kobo_zsmalloc", "kobo_zram", "kobo_pagecache"]


def sections(d):
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2e)
    sh = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
    so = sh[shstrndx][4]
    return {d[so + s[0]:d.index(b"\0", so + s[0])].decode(): (s[4], s[5]) for s in sh}


def main():
    src, out_root, release, firmware, status = sys.argv[1:6]
    crcs = None
    if len(sys.argv) > 6:
        crcs = {k: int(v, 16) for k, v in json.load(open(sys.argv[6]))["crcs"].items()}
    if status not in ("device-verified", "untested"):
        sys.exit("status must be device-verified or untested")
    if not re.fullmatch(r"[0-9A-Za-z.+_-]+", release):
        sys.exit("bad release")
    files = {}
    for n in NAMES:
        d = bytearray(open(os.path.join(src, n + ".ko"), "rb").read())
        sec = sections(bytes(d))
        off, size = sec[".modinfo"]
        info = bytes(d[off:off + size])
        m = re.search(rb"vermagic=(\S+) ", info)
        old = m.group(1)
        new = release.encode()
        if old != new:
            if len(old) != len(new):
                sys.exit("%s: vermagic %s and %s differ in length; rebuild instead" % (n, old.decode(), release))
            if bytes(d).count(old) != 1:
                sys.exit("%s: release string occurs %d times" % (n, bytes(d).count(old)))
            p = off + m.start(1)
            d[p:p + len(new)] = new
        if crcs is not None:
            vo, vs = sec["__versions"]
            v = bytes(d[vo:vo + vs])
            want = {v[i + 4:i + 64].split(b"\0")[0].decode(): struct.unpack_from("<I", v, i)[0] for i in range(0, len(v), 64)}
            bad = sorted(s for s in want if crcs.get(s) != want[s]
                         and not (n == "kobo_zram" and s.startswith("zs_")))  # zs_* come from kobo_zsmalloc
            if bad:
                sys.exit("%s: symbols missing or with another CRC: %s" % (n, bad))
        files[n + ".ko"] = bytes(d)
    out = os.path.join(out_root, release)
    os.makedirs(out, exist_ok=True)
    sums = []
    for name, data in files.items():
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
        sums.append("%s  %s\n" % (hashlib.sha256(data).hexdigest(), name))
    open(os.path.join(out, "SHA256SUMS"), "w").write("".join(sums))
    open(os.path.join(out, "firmware"), "w").write(firmware + "\n")
    open(os.path.join(out, "status"), "w").write(status + "\n")
    print("".join(sums), end="")
    print("KERNEL_SET %s firmware=%s status=%s" % (release, firmware, status))


main()
