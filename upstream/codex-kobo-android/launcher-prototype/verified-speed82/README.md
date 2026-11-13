# Build 82: focused shelf candidate, not deployed

The user confirmed book opening and shelf return work again after reverting build 81. That baseline reading run ended manually after 338.60 seconds with exit 0, exact framebuffer restoration and clean teardown.

This new candidate selects 1223 methods around the observed shelf ANR stack, shelf models and AndroidX DiffUtil. It explicitly includes BaseShelfAdapter.diffAndNotify, which build 81 missed. This is a preset informed by a stack trace, not a recorded hot-method profile. The original APK remains unchanged.

Compilation took 91.50 seconds. All 13 embedded DEX ZIP CRCs, sizes and non-instruction bytes match the original; ART-generated quickened instructions may differ. Boot image checksums match. Native executable text grows by 241088 bytes. The candidate is kept privately on the host, not distributed or installed for daily reading.

The benchmark loads the actual APK's AndroidX DiffUtil using PathClassLoader, with our own synthetic callbacks. It never initializes WeRead's Application or reads account/library data. It checks unchanged lists, 97 moves plus 23 content changes, and 192 removals plus 192 insertions, repeated three times. Compile-only API stubs are excluded from runtime DEX. An initial diagnostic helper lacked Binder and aborted before the benchmark; its environment was fixed and all subsequent benchmark cases passed.

With JIT enabled, the first rotating-list case took 193.9 ms on the baseline and 11.1 ms on the candidate; two later samples were 7.6/14.1 ms versus 3.9/3.9 ms. These are small synthetic samples, not book-opening or shelf-loading measurements. Pure interpretation regressed, roughly 250 ms to 410 ms for rotation; this candidate should not be presented as an unconditional speedup. See the complete synthetic result files and verification.json.

Candidate testing used a temporary file bind mount over the original cache during an exclusive offline maintenance session. It was then unmounted; the underlying original cache stayed unchanged and its SHA was verified afterward. The device-side temporary candidate was removed to restore the reading free-space guard. All temporary modules, mounts and loops were cleaned. Daily reading remains on the user-validated build 80 cache.

The user's later background-process question was checked separately: live-display already pauses both exact KOReader processes when interactive display starts, then resumes them after exit. Nickel was not running in the host audit. KOReader PSS was about 22 MiB, with about 21 MiB private memory. This is a memory audit, not a measured speedup from terminating KOReader. A future independent-entry mode must replace the display bridge's explicit two-reader precondition and provide graceful state saving and restart; simply killing them would break the current launcher.

These scripts are developer probes for this already-configured private runtime, not an installer. APK, profiles, compiled caches and personal logs are excluded from the source distribution.
