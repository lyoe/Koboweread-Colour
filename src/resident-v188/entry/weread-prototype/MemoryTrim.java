import android.os.Handler;
import android.os.Looper;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.lang.reflect.Method;
/** Low-memory housekeeping inside WeRead's processes (2026-11-07).
 * A 3-hour realistic soak (docs/1.8.8版本.md) ended with 76-110 MB available
 * and book opens slowing from ~7 s to ~60 s, nearly all I/O wait from
 * re-reading evicted pages. Android would normally tell apps to trim
 * (onTrimMemory) and ART returns a process's free heap pages only when the
 * process changes state; in this environment neither happens while WeRead
 * stays in front. Every 30 s this thread reads MemAvailable and
 *  - below 160 MB (at most every 2 min): runs a concurrent GC and then
 *    VMRuntime.trimHeap(), which gives the kernel back heap pages that hold
 *    no live objects (the :seg process kept ~40 MB of heap for ~2 MB live);
 *  - below 120 MB, main process only (at most every 5 min): delivers
 *    TRIM_MEMORY_RUNNING_CRITICAL (15) through ActivityThread.handleTrimMemory
 *    on the main thread, exactly as the system would: WeRead's own memory
 *    watchers and Glide (image cache halved) react as on a phone.
 * Nothing of WeRead's code is replaced. Logs WEREAD_MEM_TRIM lines with the
 * process RSS before and after. Off: fast-v188/memtrim-disabled
 * (data/local/tmp/kobo-memtrim-disabled). Stops after 3 errors. */
final class MemoryTrim implements Runnable {
 private static final long PERIOD_MS = 30000, HEAP_EVERY_MS = 120000, APP_EVERY_MS = 300000;
 private static final int HEAP_BELOW_KB = 160 * 1024, APP_BELOW_KB = 120 * 1024, TRIM_MEMORY_RUNNING_CRITICAL = 15;
 private boolean main; private String name;
 private long lastHeap, lastApp; private int failures;

 static void start() {
  if (new File("/data/local/tmp/kobo-memtrim-disabled").exists()) { System.out.println("WEREAD_MEM_TRIM_DISABLED"); return; }
  Thread t = new Thread(new MemoryTrim(), "kobo-mem-trim");
  t.setDaemon(true); t.setPriority(Thread.MIN_PRIORITY); t.start();
 }

 private static String processName() {
  try (BufferedReader in = new BufferedReader(new FileReader("/proc/self/cmdline"))) {
   String s = in.readLine(); if (s == null) return null; int z = s.indexOf('\0'); return z >= 0 ? s.substring(0, z) : s;
  } catch (Throwable error) { return null; }
 }

 private static int meminfo(String file, String key) {
  try (BufferedReader in = new BufferedReader(new FileReader(file))) {
   for (String l; (l = in.readLine()) != null;) if (l.startsWith(key)) return Integer.parseInt(l.substring(key.length()).trim().split("\\s+")[0]);
  } catch (Throwable error) { }
  return -1;
 }

 public void run() {
  // The process name is set by the time the first period has passed.
  try { Thread.sleep(PERIOD_MS); } catch (InterruptedException stop) { return; }
  name = processName();
  if (name == null || !name.startsWith("com.tencent.weread.eink")) { System.out.println("WEREAD_MEM_TRIM_SKIPPED process=" + name); return; }
  main = "com.tencent.weread.eink".equals(name);
  System.out.println("WEREAD_MEM_TRIM_READY process=" + name);
  while (failures < 3) {
   try {
    Thread.sleep(PERIOD_MS);
    int avail = meminfo("/proc/meminfo", "MemAvailable:");
    long now = android.os.SystemClock.uptimeMillis();
    if (avail > 0 && avail < HEAP_BELOW_KB && now - lastHeap >= HEAP_EVERY_MS) { lastHeap = now; trimHeap(avail); }
    if (main && avail > 0 && avail < APP_BELOW_KB && now - lastApp >= APP_EVERY_MS) { lastApp = now; trimApp(avail); }
   } catch (Throwable error) {
    failures++; System.out.println("WEREAD_MEM_TRIM_ERROR process=" + name + " " + error);
   }
  }
  System.out.println("WEREAD_MEM_TRIM_STOPPED process=" + name);
 }

 private void trimHeap(int avail) throws Exception {
  int before = meminfo("/proc/self/status", "VmRSS:");
  Class<?> vm = Class.forName("dalvik.system.VMRuntime");
  Object runtime = vm.getMethod("getRuntime").invoke(null);
  vm.getMethod("concurrentGC").invoke(runtime);
  vm.getMethod("trimHeap").invoke(runtime);
  System.out.println("WEREAD_MEM_TRIM heap process=" + name + " avail_kb=" + avail + " rss_kb=" + before + "->" + meminfo("/proc/self/status", "VmRSS:")
   + " avail_after_kb=" + meminfo("/proc/meminfo", "MemAvailable:"));
 }

 private void trimApp(final int avail) throws Exception {
  final Class<?> at = Class.forName("android.app.ActivityThread");
  final Object thread = at.getMethod("currentActivityThread").invoke(null);
  if (thread == null || Looper.getMainLooper() == null) return;
  final Method handle = at.getDeclaredMethod("handleTrimMemory", int.class);
  handle.setAccessible(true);
  new Handler(Looper.getMainLooper()).post(new Runnable() { public void run() {
   try {
    int before = meminfo("/proc/self/status", "VmRSS:");
    handle.invoke(thread, TRIM_MEMORY_RUNNING_CRITICAL);
    System.out.println("WEREAD_MEM_TRIM app level=" + TRIM_MEMORY_RUNNING_CRITICAL + " avail_kb=" + avail + " rss_kb=" + before + "->" + meminfo("/proc/self/status", "VmRSS:"));
   } catch (Throwable error) { System.out.println("WEREAD_MEM_TRIM_ERROR app " + error); }
  } });
 }
}
