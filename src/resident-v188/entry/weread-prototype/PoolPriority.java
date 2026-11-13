import java.io.File;
import java.io.FileInputStream;
import java.util.HashMap;
/** WeRead's background pools at the lowest CPU priority (2026-11-09,
 * docs/1.8.8版本.md). After a book opens WeRead spends 10-18 s of CPU in the
 * 25 s that follow, almost all on WRCommonPool #1-#4 (book readers' thoughts,
 * popular highlights, chapter reviews: ~9 s) and WRNetworkPool (~1.5 s), not
 * on the main thread (~2 s); page turns in that window compete with them.
 * Every second, threads of the main process named WRCommonPool* or
 * WRNetworkPool* that are above nice 19 are set to 19, so they only use CPU
 * the main thread leaves free (when the main thread waits for them, they still
 * get the CPU). Main process only. Off: fast-v188/poollow-disabled
 * (data/local/tmp/kobo-poollow-disabled). Logs WEREAD_POOL_LOW_*.
 * 2026-11-10: each thread's name is read once (the first time its name is no
 * longer the process default), not every second for all ~60 threads with a
 * new BufferedReader each: that loop alone used ~50 ms of CPU per second
 * (2.7 s in the 55 s after the first screen) and fed the collector. */
final class PoolPriority implements Runnable {
 static void start() {
  if (new File("/data/local/tmp/kobo-poollow-disabled").exists()) { System.out.println("WEREAD_POOL_LOW_DISABLED"); return; }
  Thread t = new Thread(new PoolPriority(), "kobo-pool-low");
  t.setDaemon(true); t.start();
 }

 private static final byte[] buf = new byte[64];
 private static String read(String path) {
  try (FileInputStream in = new FileInputStream(path)) {
   int n = in.read(buf); if (n <= 0) return "";
   int e = 0; while (e < n && buf[e] != '\n' && buf[e] != 0) e++;
   return new String(buf, 0, e, "UTF-8");
  } catch (Throwable error) { return ""; }
 }

 public void run() {
  try {
   Thread.sleep(5000);
   String cmd = read("/proc/self/cmdline"); int z = cmd.indexOf('\0'); if (z >= 0) cmd = cmd.substring(0, z);
   if (!"com.tencent.weread.eink".equals(cmd)) return;
   System.out.println("WEREAD_POOL_LOW_READY");
   // tid -> pool thread or not, decided once the thread has its own name
   // (a new thread briefly shows the process name, "ent.weread.eink").
   String own = read("/proc/self/comm");
   HashMap<String, Boolean> known = new HashMap<String, Boolean>();
   long total = 0, logged = 0; int tick = 0, failures = 0;
   while (failures < 5) {
    try {
     String[] tids = new File("/proc/self/task").list();
     if (tids != null) for (String s : tids) {
      Boolean pool = known.get(s);
      if (pool == null) {
       String name = read("/proc/self/task/" + s + "/comm");
       if (name.length() == 0 || name.equals(own)) continue;
       pool = name.startsWith("WRCommonPool") || name.startsWith("WRNetworkPool");
       known.put(s, pool);
      }
      if (!pool) continue;
      try {
       int tid = Integer.parseInt(s);
       if (android.os.Process.getThreadPriority(tid) >= 19) continue;
       android.os.Process.setThreadPriority(tid, 19); total++;
      } catch (Throwable gone) { }
     }
     if (++tick % 60 == 0) {
      if (tids != null) known.keySet().retainAll(java.util.Arrays.asList(tids)); // ended threads
      if (total != logged) { System.out.println("WEREAD_POOL_LOW lowered=" + total + " known=" + known.size()); logged = total; }
     }
     Thread.sleep(1000);
    } catch (InterruptedException stop) { return; }
    catch (Throwable error) { failures++; System.out.println("WEREAD_POOL_LOW_ERROR " + error); }
   }
   System.out.println("WEREAD_POOL_LOW_STOPPED");
  } catch (Throwable error) { System.out.println("WEREAD_POOL_LOW_ERROR " + error); }
 }
}
