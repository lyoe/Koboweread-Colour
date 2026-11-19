# Third-party sources kept in this repository

| Directory | What | License | Origin |
|---|---|---|---|
| `fbink/` | FBInk by NiLuJe, upstream v1.25.0-544-gded92fa5 (commit ded92fa510c957e0157c92ae83bd5c51af710c05, for the Libra Colour / MTK hwtcon), without its fonts/, libevdev/, libunibreak/ and resources/ (not used by this build); stb trimmed as before | GPL-3.0-or-later (`fbink/LICENSE`); bundled parts keep their own licenses (stb: public domain/MIT, i2c-tools: GPL-2.0+, qimagescale: see headers) | https://github.com/NiLuJe/FBInk |
| `aosp/` | Android Open Source Project headers and reference sources used by the HWC and display shims (`include/`, `include-shim/` adapters, `ref/` for comparison) | Apache-2.0 (see file headers) | https://android.googlesource.com |
