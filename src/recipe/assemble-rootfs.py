"""Build the image tree (a tar like filter-rootfs.py's output) from a recipe
(extract-recipe.py / publicize-recipe.py) and the user's inputs:

  python3 -I assemble-rootfs.py --recipe DIR --google-system DIR --ramdisk DIR \
      --webview-apk FILE [--apk WEREAD.apk] --out ROOTFS.tar \
      [--lab DIR] [--build NAME=FILE ...] [--lab-tar ROOTFS.tar]

Content per entry SOURCE (entries.tsv):
  google:P  Google /system file P        ramdisk:P  Google ramdisk file P
  patch:ID  byte patch (patches/ID.json) of a Google file, both hashes checked
  apk       the WeRead APK               apk:E      entry E of the WeRead APK
  file:P    RECIPE/files/P               empty      empty file
  lab:P     DIR/P from --lab (src/runtime/build.sh output; --lab-tar takes
            lab/P from an existing image tree instead, for verification)
  build:N   the file given as --build N=FILE (e.g. the launcher APK)
Hashes in entries.tsv are checked for google, ramdisk, patch, apk and file
sources; lab and build files are taken as built (their hashes are printed in
the summary). Owner, mode and time come from entries.tsv, so directory times
match what PackageManager recorded.
"""
import argparse, hashlib, io, json, os, sys, tarfile, zipfile


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    for a in ("recipe", "google-system", "ramdisk", "webview-apk", "out"):
        ap.add_argument("--" + a, required=True)
    ap.add_argument("--apk", help="WeRead APK, required for a complete rootfs")
    ap.add_argument("--lab")
    ap.add_argument("--lab-tar")
    ap.add_argument("--build", action="append", default=[])
    ap.add_argument("--lab-cert", help="hex DER certificate of the user's signing key (fills @LAB_CERT@)")
    ap.add_argument("--payload", action="store_true",
                    help="installer form: leave the WeRead APK and its native libraries out of the tar and write apk-meta and dirtimes beside it")
    o = ap.parse_args()
    builds = dict(b.split("=", 1) for b in o.build)
    if not o.payload and not o.apk:
        ap.error("--apk is required without --payload")
    apk = open(o.apk, "rb").read() if not o.payload else None
    z = zipfile.ZipFile(io.BytesIO(apk)) if apk is not None else None
    webview = zipfile.ZipFile(o.webview_apk)
    labtar = tarfile.open(o.lab_tar) if o.lab_tar else None
    labmembers = {m.name: m for m in labtar} if labtar else {}

    def base_of(ref):
        kind, path = ref.split(":", 1)
        if kind == "google":
            return open(os.path.join(o.google_system, path), "rb").read()
        if kind == "webview":
            return webview.read(path)
        raise ValueError(ref)

    def content(source, digest, name):
        kind, _, arg = source.partition(":")
        check = True
        if kind == "google":
            data = open(os.path.join(o.google_system, arg), "rb").read()
        elif kind == "ramdisk":
            data = open(os.path.join(o.ramdisk, arg), "rb").read()
        elif kind == "patch":
            p = json.load(open(os.path.join(o.recipe, "patches", arg + ".json")))
            data = bytearray(base_of(p["base"]))
            if sha(data) != p["input_sha256"]:
                raise SystemExit("PATCH_INPUT_MISMATCH %s" % arg)
            for off, old, new in p["edits"]:
                old, new = bytes.fromhex(old), bytes.fromhex(new)
                if data[off:off + len(old)] != old:
                    raise SystemExit("PATCH_CONTEXT_MISMATCH %s @%d" % (arg, off))
                data[off:off + len(new)] = new
            data = bytes(data)
            if sha(data) != p["output_sha256"]:
                raise SystemExit("PATCH_OUTPUT_MISMATCH %s" % arg)
        elif kind == "apk" and not arg:
            data = apk
        elif kind == "apk":
            data = z.read(arg)
        elif kind == "file":
            data = open(os.path.join(o.recipe, "files", arg), "rb").read()
            if b"@LAB_CERT@" in data:
                if not o.lab_cert or not all(c in "0123456789abcdef" for c in o.lab_cert):
                    raise SystemExit("LAB_CERT_REQUIRED for " + name)
                data = data.replace(b"@LAB_CERT@", o.lab_cert.encode())
        elif kind == "empty":
            data = b""
        elif kind == "lab":
            check = False
            if labtar:
                data = labtar.extractfile(labmembers["./lab/" + arg]).read()
                check = True
            else:
                data = open(os.path.join(o.lab, arg), "rb").read()
        elif kind == "build":
            check = False
            data = open(builds[arg], "rb").read()
        elif kind == "unresolved" and labtar:
            data = labtar.extractfile(labmembers["./" + name]).read()  # verification only
        else:
            raise SystemExit("UNKNOWN_SOURCE %s for %s" % (source, name))
        if check and digest != "-" and sha(data) != digest:
            raise SystemExit("HASH_MISMATCH %s (%s)" % (name, source))
        return data

    out = tarfile.open(o.out, "w", format=tarfile.GNU_FORMAT)
    counts, unchecked, apk_meta, dirtimes = {}, [], [], []
    for line in open(os.path.join(o.recipe, "entries.tsv")):
        f = line.rstrip("\n").split("\t")
        typ, mode, uid, gid, mtime, size, digest, source, name = f[:9]
        extra = f[9] if len(f) > 9 else ""
        ti = tarfile.TarInfo("." if name in ("", ".") else "./" + name)
        ti.mode, ti.uid, ti.gid, ti.mtime = int(mode, 8), int(uid), int(gid), int(mtime)
        data = None
        if typ == "d":
            ti.type = tarfile.DIRTYPE
        elif typ == "l":
            ti.type, ti.linkname = tarfile.SYMTYPE, extra
        elif typ in "cb":
            ti.type = tarfile.CHRTYPE if typ == "c" else tarfile.BLKTYPE
            ti.devmajor, ti.devminor = (int(x) for x in extra.split(","))
        elif typ == "f" and o.payload and source.split(":")[0] == "apk":
            # install.sh restores these files from the user's APK.
            fields = [ti.name, uid, gid, mode, mtime]
            if source != "apk":
                fields.append(source.partition(":")[2])
            apk_meta.append(" ".join(fields))
            continue
        elif typ == "f":
            data = content(source, digest, name)
            ti.size = len(data)
            if source.split(":")[0] in ("lab", "build") and digest != "-" and sha(data) != digest:
                unchecked.append(name)
        else:
            raise SystemExit("UNKNOWN_TYPE %s %s" % (typ, name))
        if typ == "d":
            dirtimes.append("%s %s" % (ti.name, mtime))
        out.addfile(ti, io.BytesIO(data) if data is not None else None)
        k = source.split(":")[0]
        counts[k] = counts.get(k, 0) + 1
    out.close()
    if o.payload:
        d = os.path.dirname(os.path.abspath(o.out))
        open(os.path.join(d, "apk-meta"), "w").write("\n".join(sorted(apk_meta)) + "\n")
        open(os.path.join(d, "dirtimes"), "w").write("\n".join(dirtimes) + "\n")
    print("ASSEMBLED", o.out, counts, "built-differs-from-recipe", len(unchecked))
    for n in unchecked:
        print("  built:", n)


if __name__ == "__main__":
    main()
