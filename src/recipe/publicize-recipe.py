"""Turn a recipe from extract-recipe.py into the public one (docs/公开版设计.md):

  python3 -I publicize-recipe.py --recipe DIR --runtime-list runtime-sha256.txt --out DIR

- Drops what a session rebuilds or never uses: /dev/__properties__ (removed
  by the inner script at every start), the switch
  markers data/local/tmp/kobo-*, kernel modules in lab/ (sessions load
  kernels/<uname -r>/), Codex's test probes and test scripts (lab files that
  neither src/runtime/build.sh produces nor KEEP_LAB_TEXT lists), the
  installed probe's odex.
- WeRead's odex (lab/fast/weread-speed-profile.odex and its .sha256) is
  dropped: it is compiled on the device from the user's APK.
- lab/libkobo_jit_config.so (a session mount target) becomes build:jit-config.
- The launcher (system/app/KoboProbe/KoboProbe.apk) and Codex's installed
  probe (data/app/com.kobo.lab.installed-1/base.apk) become build:launcher and
  build:installed-probe, signed with the user's own key; the lab key's
  certificate in packages.xml becomes @LAB_CERT@, filled in by
  assemble-rootfs.py --lab-cert.
- Fails if anything is left unresolved, or if a kept text file contains
  something that looks private (LAN address, e-mail, and the strings in
  public/private-patterns.txt when present).
"""
import argparse, os, re, shutil, sys

KEEP_LAB_TEXT = {
    "virtual-fb.state",  # 24-byte framebuffer state; the bridge opens it before SurfaceFlinger writes it
    "audit-runtime-fds.sh", "capture-reader-stacks.sh", "install-process-allowlist.txt", "module-cleanup.sh",
    "native-audio-identity.txt", "native-audio-services.txt", "patched-webview-mapping.txt", "startup-readiness.sh",
    "vold-owned-mounts", "installed-identity.txt", "native-resolution/native-memory-watchdog.sh", "native-resolution/run-native-weread.sh",
}
REMAP = {
    "system/app/KoboProbe/KoboProbe.apk": "build:launcher",
    "data/app/com.kobo.lab.installed-1/base.apk": "build:installed-probe",
    # mounted over by every session; the image copy is the same version's shim
    "lab/libkobo_jit_config.so": "build:jit-config",
    "lab/kobo-installed-probe.apk": "build:installed-probe",
}
DROP_EXACT = {
    "lab/fast/weread-speed-profile.odex", "lab/fast/weread-speed-profile.odex.sha256",
    "data/app/com.kobo.lab.installed-1/oat/arm/base.odex",
}
PRIVATE = [re.compile(p) for p in (r"192\.168\.\d+\.\d+", r"[\w.+-]+@(gmail|qq|163|outlook)\.com")]
# Extra strings (account ids, names) from a list kept outside the public tree.
_extra = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "public", "private-patterns.txt")
if os.path.exists(_extra):
    PRIVATE += [re.compile(re.escape(l.strip())) for l in open(_extra) if l.strip() and not l.startswith("#")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--recipe", required=True)
    ap.add_argument("--runtime-list", required=True)
    ap.add_argument("--out", required=True)
    o = ap.parse_args()
    built = {l.split()[1][2:] for l in open(o.runtime_list) if l.strip()}
    if os.path.exists(o.out):
        raise SystemExit("OUT_EXISTS " + o.out)
    shutil.copytree(o.recipe, o.out)
    files = os.path.join(o.out, "files")
    kept, dropped, problems = [], [], []
    for line in open(os.path.join(o.recipe, "entries.tsv")):
        f = line.rstrip("\n").split("\t")
        typ, digest, source, name = f[0], f[6], f[7], f[8]
        drop = (name.startswith("dev/__properties__") or name in DROP_EXACT
                or (name.startswith("data/local/tmp/kobo-") and typ == "f")
                or (name.startswith("lab/") and name.endswith(".ko")))
        if not drop and typ == "f" and name.startswith("lab/") and name not in REMAP:
            rel = name[4:]
            if source.startswith("lab:"):
                drop = rel not in built
            elif source.startswith("file:"):
                drop = rel not in KEEP_LAB_TEXT
            elif source == "unresolved":
                drop = True
        if drop:
            dropped.append(name)
            if source.startswith("file:"):
                p = os.path.join(files, source[5:])
                if os.path.exists(p):
                    os.remove(p)
            continue
        if name in REMAP:
            f[6], f[7] = "-", REMAP[name]
        elif source == "unresolved":
            problems.append("UNRESOLVED " + name)
        if name == "data/system/packages.xml":
            p = os.path.join(files, "data/system/packages.xml")
            x = open(p).read()
            certs = re.findall(r'<cert index="0" key="([0-9a-f]+)"', x)
            if len(certs) != 1:
                problems.append("LAB_CERT_NOT_FOUND")
            else:
                open(p, "w").write(x.replace(certs[0], "@LAB_CERT@"))
                f[6] = "-"
        kept.append("\t".join(f))
    # Directories left empty by the drops stay (the session expects lab/fast etc.).
    for root, _, names in os.walk(files):
        for n in names:
            p = os.path.join(root, n)
            try:
                t = open(p, encoding="utf-8").read()
            except UnicodeDecodeError:
                continue
            for r in PRIVATE:
                m = r.search(t)
                if m:
                    problems.append("PRIVATE %s: %s" % (os.path.relpath(p, files), m.group(0)))
    for d in ("lab.tsv", "unresolved.txt"):
        if os.path.exists(os.path.join(o.out, d)):
            os.remove(os.path.join(o.out, d))
    open(os.path.join(o.out, "entries.tsv"), "w").write("\n".join(kept) + "\n")
    print("kept", len(kept), "dropped", len(dropped))
    for p in problems:
        print(p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
