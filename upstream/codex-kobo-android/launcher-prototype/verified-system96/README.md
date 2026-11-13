# System-service diagnostic 96

Build95 eliminated missing-lmkd retries across the complete reading run: zero socket failures, zero SLOW OOM ADJ, 13 verified kernel-priority writes and clean teardown. Human times still varied (book 27/13 seconds, shelf 5/9 seconds). An independent ActivityStack handler held the AMS monitor for about 5.4 seconds; no timeouts were relaxed.

The first self-test using Android's profileControl API against the `system` process returned an accepted request but produced an empty file. It is recorded as FAILED, not as a usable capture. SystemMethodProbe.java preserves that attempted path; it is not used for the revised capture. Possible local/asynchronous profiler FD handling was inspected, but its exact failure cause was not proven.

The revised method uses our root-only SystemSamplingService in the system process and directly calls Debug.startMethodTracingSampling from a dedicated bounded worker. It accepts no caller-specified filenames, durations or targets. A one-second self-test must complete before one 120-second capture can start; only two jobs are allowed per runtime. Sampling interval is 100ms and buffer size is 8MiB. It binds to session readiness metadata and requests stop in the worker itself. The service remains idle until a root request; applications cannot trigger it. It changes neither APK nor application timeouts or permissions.

Files are fixed selftest.trace / transition.trace inside a system-owned directory with mode 0700, with file mode 0600. They remain private and are excluded from this archive. Disk headroom is checked. Original method93 trace copies were SHA-256 matched to private host archives before removing those two device-side copies to reclaim space.

API24 javac/D8 compilation passed, and the revised Binder service has been published on the actual device. The corrected in-process self-test passed on the actual device: 5867 bytes, 130 complete version-3 records, no overflow, about 1.25 seconds, system PID 11198. Transition capture remains pending. This diagnostic may slow operations and must not be treated as a speed comparison.

The initial in-process self-test failed before sampling: UID1000 attempted to read root-only readiness contents. An uncaught diagnostic-client exception ended that window (117); strict cleanup passed. The service now validates only stat metadata and retains original root-only permissions. The client catches errors and exits explicitly. The output directory was empty and checked before updating. Neither failure is counted as a successful trace.
