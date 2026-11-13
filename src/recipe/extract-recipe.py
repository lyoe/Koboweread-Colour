"""Derive the public image recipe from a filtered rootfs (docs/公开版设计.md):

  python3 -I extract-recipe.py --rootfs ROOTFS.tar --google-system DIR \
      --ramdisk DIR --webview-apk WEBVIEW.apk --apk WEREAD.apk --out RECIPE_DIR

ROOTFS.tar is the installer's filtered image tree (filter-rootfs.py output),
DIR the extracted Google API 24 ARM system.img and ramdisk.img. Every tar
member gets one line in RECIPE_DIR/entries.tsv, in tar order:

  TYPE MODE UID GID MTIME SIZE SHA256 SOURCE NAME [LINK|MAJOR,MINOR]

TYPE is d (directory), f (file), l (symlink), c/b (device). SOURCE says where
assemble-rootfs.py takes the content from:
  google:PATH        file of the Google system image (path below /system)
  ramdisk:PATH       file of the Google ramdisk
  patch:ID           byte patch of a Google file (patches/ID.json)
  apk:ENTRY          entry of the user's WeRead APK
  apk                the WeRead APK itself
  file:PATH          our own file shipped in RECIPE_DIR/files/PATH
  lab:PATH           runtime-layer file, built from source (see lab.tsv)
  empty / -          empty file / no content (directory, link, device)
Files that fit none of these are listed in unresolved.txt and stop a clean
recipe. Nothing of Google's or Tencent's is copied into RECIPE_DIR; files/
only receives small text files that are ours (checked: not identical to a
Google file, under 256 KB, valid UTF-8) and lab/ binaries are only named.
"""
import argparse, hashlib, json, os, sys, tarfile, zipfile

TEXT_LIMIT = 256 * 1024


def sha(b):
    return hashlib.sha256(b).hexdigest()


def file_sha(path):
    with open(path, "rb") as f:
        return sha(f.read())


def byte_patch(src, dst):
    """Edits [offset, old_hex, new_hex] for two equal-size buffers, or None."""
    if len(src) != len(dst):
        return None
    edits, i, n = [], 0, len(src)
    while i < n:
        if src[i] == dst[i]:
            i += 1
            continue
        j = i
        while j < n and src[j] != dst[j]:
            j += 1
        edits.append([i, src[i:j].hex(), dst[i:j].hex()])
        i = j
    return edits


def main():
    ap = argparse.ArgumentParser()
    for a in ("rootfs", "google-system", "ramdisk", "webview-apk", "apk", "out"):
        ap.add_argument("--" + a, required=True)
    ap.add_argument("--manifest", help="filter-rootfs.py manifest: adds the APK copies it took out of the tree")
    o = ap.parse_args()
    out = o.out
    os.makedirs(os.path.join(out, "files"), exist_ok=True)
    os.makedirs(os.path.join(out, "patches"), exist_ok=True)

    # Google files by hash (path below /system) and ramdisk files by hash.
    google = {}
    for root, _, names in os.walk(o.google_system):
        for n in names:
            p = os.path.join(root, n)
            if os.path.isfile(p) and not os.path.islink(p):
                google.setdefault(file_sha(p), os.path.relpath(p, o.google_system))
    ramdisk = {}
    for root, _, names in os.walk(o.ramdisk):
        for n in names:
            p = os.path.join(root, n)
            if os.path.isfile(p) and not os.path.islink(p):
                ramdisk.setdefault(file_sha(p), os.path.relpath(p, o.ramdisk))
    apk_bytes = open(o.apk, "rb").read()
    apk_sha = sha(apk_bytes)
    z = zipfile.ZipFile(o.apk)
    apk_entries = {sha(z.read(n)): n for n in z.namelist() if not n.endswith("/")}
    wz = zipfile.ZipFile(o.webview_apk)
    webview_lib = wz.read("lib/armeabi-v7a/libwebviewchromium.so")

    t = tarfile.open(o.rootfs)
    lines, unresolved, lab = [], [], []
    for m in t:
        name = m.name
        rel = name[2:] if name.startswith("./") else name
        typ = "d" if m.isdir() else "l" if m.issym() else "c" if m.ischr() else "b" if m.isblk() else "f" if m.isfile() else "?"
        extra = m.linkname if m.issym() else ("%d,%d" % (m.devmajor, m.devminor) if typ in "cb" else "")
        digest, source = "-", "-"
        if typ == "f":
            data = t.extractfile(m).read()
            digest = sha(data)
            if not data:
                source = "empty"
            elif digest == apk_sha:
                source = "apk"
            elif rel.startswith("system/") and digest in google and google[digest] == rel[len("system/"):]:
                source = "google:" + google[digest]
            elif digest in google:
                source = "google:" + google[digest]
            elif digest in ramdisk:
                source = "ramdisk:" + ramdisk[digest]
            elif digest in apk_entries:
                source = "apk:" + apk_entries[digest]
            else:
                base = None
                if rel.startswith("system/") and os.path.isfile(os.path.join(o.google_system, rel[len("system/"):])):
                    base = ("google:" + rel[len("system/"):], open(os.path.join(o.google_system, rel[len("system/"):]), "rb").read())
                elif rel.endswith("libwebviewchromium-software-surface.so"):
                    base = ("webview:lib/armeabi-v7a/libwebviewchromium.so", webview_lib)
                edits = byte_patch(base[1], data) if base else None
                if edits is not None and sum(len(e[1]) // 2 for e in edits) <= 4096:
                    pid = rel.replace("/", "_")
                    json.dump({"base": base[0], "input_sha256": sha(base[1]), "output_sha256": digest, "edits": edits},
                              open(os.path.join(out, "patches", pid + ".json"), "w"), indent=1)
                    source = "patch:" + pid
                elif rel.startswith("lab/") and (data[:4] == b"\x7fELF" or data[:4] == b"dex\n" or len(data) > TEXT_LIMIT):
                    source = "lab:" + rel[len("lab/"):]
                    lab.append("%s\t%s\t%d" % (rel[len("lab/"):], digest, len(data)))
                else:
                    try:
                        data.decode("utf-8")
                        is_text = len(data) <= TEXT_LIMIT
                    except UnicodeDecodeError:
                        is_text = False
                    # Tiny state files of the runtime layer (lab/virtual-fb.state, 24 bytes).
                    if not is_text and rel.startswith("lab/") and len(data) <= 4096 and data[:4] not in (b"\x7fELF", b"dex\n"):
                        is_text = True
                    if is_text:
                        p = os.path.join(out, "files", rel)
                        os.makedirs(os.path.dirname(p), exist_ok=True)
                        open(p, "wb").write(data)
                        source = "file:" + rel
                    else:
                        unresolved.append("%s\t%s\t%d" % (rel, digest, len(data)))
                        source = "unresolved"
        lines.append("\t".join([typ, "%o" % m.mode, str(m.uid), str(m.gid), str(int(m.mtime)), str(m.size if typ == "f" else 0),
                                digest, source, rel, extra]))
    if o.manifest:
        # filter-rootfs.py removed the WeRead APK copies (install.sh puts them back).
        for path, m in sorted(json.load(open(o.manifest))["apk"].items()):
            rel = path[2:]
            if m["size"] != len(apk_bytes):
                raise SystemExit("MANIFEST_APK_SIZE_MISMATCH " + rel)
            lines.append("\t".join(["f", m["mode"], str(m["uid"]), str(m["gid"]), str(m["mtime"]), str(m["size"]), apk_sha, "apk", rel, ""]))
    open(os.path.join(out, "entries.tsv"), "w").write("\n".join(lines) + "\n")
    open(os.path.join(out, "lab.tsv"), "w").write("\n".join(sorted(lab)) + ("\n" if lab else ""))
    open(os.path.join(out, "unresolved.txt"), "w").write("\n".join(unresolved) + ("\n" if unresolved else ""))
    counts = {}
    for l in lines:
        s = l.split("\t")[7].split(":")[0]
        counts[s] = counts.get(s, 0) + 1
    print("entries", len(lines), counts, "lab", len(lab), "unresolved", len(unresolved))
    return 1 if unresolved else 0


if __name__ == "__main__":
    sys.exit(main())
