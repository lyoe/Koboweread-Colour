import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
/** Diagnostic: Java heap dump of a WeRead process on request, written by the
 * process itself (2026-11-07). The ActivityManager route (src/diag/HeapDump)
 * needs a new root JVM, which cannot start once memory is tight (128 MB heap
 * reservation refused). A thread polls /data/local/tmp/kobo-heapdump-request
 * every 2 s; a line "N main" or "N seg" with a new N makes the named process
 * call Debug.dumpHprofData into its cache directory as kobo-<main|seg>-N.hprof
 * (WeRead's uid cannot write /data/local/tmp, and cannot delete the request;
 * N is remembered instead). Only a root shell can create the request. */
final class HeapDumpRequest implements Runnable {
 private static final String REQUEST = "/data/local/tmp/kobo-heapdump-request";
 private String role; private String last = "";

 static void start() {
  Thread t = new Thread(new HeapDumpRequest(), "kobo-heap-dump");
  t.setDaemon(true); t.setPriority(Thread.MIN_PRIORITY); t.start();
 }

 public void run() {
  try {
   Thread.sleep(30000);
   String name;
   try (BufferedReader in = new BufferedReader(new FileReader("/proc/self/cmdline"))) { name = in.readLine(); }
   if (name == null) return;
   int z = name.indexOf('\0'); if (z >= 0) name = name.substring(0, z);
   if ("com.tencent.weread.eink".equals(name)) role = "main";
   else if ("com.tencent.weread.eink:seg".equals(name)) role = "seg";
   else return;
   File request = new File(REQUEST);
   try (BufferedReader in = new BufferedReader(new FileReader(request))) { String l = in.readLine(); if (l != null) last = l.trim(); } catch (Throwable none) { }
   while (true) {
    Thread.sleep(2000);
    if (!request.exists()) continue;
    String line;
    try (BufferedReader in = new BufferedReader(new FileReader(request))) { line = in.readLine(); }
    if (line == null || (line = line.trim()).equals(last)) continue;
    last = line;
    String[] p = line.split("\\s+");
    if (p.length != 2 || !p[1].equals(role) || !p[0].matches("[0-9]+")) continue;
    String path = "/data/data/com.tencent.weread.eink/cache/kobo-" + role + "-" + p[0] + ".hprof";
    long begin = android.os.SystemClock.uptimeMillis();
    android.os.Debug.dumpHprofData(path);
    System.out.println("WEREAD_HEAP_DUMPED path=" + path + " bytes=" + new File(path).length() + " ms=" + (android.os.SystemClock.uptimeMillis() - begin));
   }
  } catch (Throwable error) {
   System.out.println("WEREAD_HEAP_DUMP_ERROR " + error);
  }
 }
}
