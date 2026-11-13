import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.lang.reflect.Field;
import java.util.Collection;
import java.util.Map;
/** WeRead's word-segmentation process (":seg", 2026-11-09, docs/底层梳理.md).
 * The IK dictionary (moai.ik.dic.Dictionary.singleton, ~21 MB of live Java
 * objects) is only needed to segment a chapter that has no segment file on
 * disk yet (SegmentParser.segmentChapter -> Dictionary.createInstance, which
 * creates it again whenever the field is null); selection, book-title and
 * URL marks read the stored segment files. WeRead never drops it.
 *
 * 1. Release when no book is open: closing a book stops SegmentService
 *    (ContentSegment.unbindBook -> unbindService + stopService). Every 5 s,
 *    when the dictionary is loaded, no SegmentService is running (or the one
 *    running has nothing loaded, loading or segmenting) and the last service
 *    seen is not segmenting either, for two checks in a row, the field is
 *    cleared; if a segmentation started meanwhile it is put back. Then a
 *    concurrent GC and VMRuntime.trimHeap() give the pages back.
 * 2. Low CPU priority: every second all other threads of :seg get nice 19, so
 *    loading the dictionary and segmenting a new chapter only use the CPU
 *    WeRead itself leaves free (after a book opens WeRead is busy for 15-18 s).
 *    A binder call from WeRead still runs at the caller's priority while it
 *    lasts (binder sets the nice of the thread serving it), so long-press
 *    selection is not slowed; the next tick lowers a long call (the dictionary
 *    load happens inside the segmentChapter binder call).
 * Main process: does nothing. Off: fast-v188/segdict-disabled (1) and
 * fast-v188/seglow-disabled (2) (data/local/tmp/kobo-*). Logs WEREAD_SEG_DICT_*.
 * Stops after 5 errors; any unexpected structure stops part 1. */
final class SegDictRelease implements Runnable {
 private static final String SERVICE = "com.tencent.weread.reader.segment.aidl.SegmentService";
 private final boolean release, low;
 private Field singleton, services, loaded, loading, segmenting;
 private Object lastService;
 private SegDictRelease(boolean release, boolean low) { this.release = release; this.low = low; }

 static void start() {
  boolean release = !new File("/data/local/tmp/kobo-segdict-disabled").exists();
  boolean low = !new File("/data/local/tmp/kobo-seglow-disabled").exists();
  if (!release && !low) return;
  Thread t = new Thread(new SegDictRelease(release, low), "kobo-seg-dict");
  t.setDaemon(true); t.start();
 }

 private static String processName() {
  try (BufferedReader in = new BufferedReader(new FileReader("/proc/self/cmdline"))) {
   String s = in.readLine(); if (s == null) return null; int z = s.indexOf('\0'); return z >= 0 ? s.substring(0, z) : s;
  } catch (Throwable error) { return null; }
 }

 private static int rssKb() {
  try (BufferedReader in = new BufferedReader(new FileReader("/proc/self/status"))) {
   for (String l; (l = in.readLine()) != null;) if (l.startsWith("VmRSS:")) return Integer.parseInt(l.substring(6).trim().split("\\s+")[0]);
  } catch (Throwable error) { }
  return -1;
 }

 private static long heapKb() { Runtime rt = Runtime.getRuntime(); return (rt.totalMemory() - rt.freeMemory()) >> 10; }

 /** Threads that got a higher priority since the last tick go back to 19. */
 private static int lowerThreads(int self) {
  int n = 0; String[] tids = new File("/proc/self/task").list();
  if (tids == null) return 0;
  for (String s : tids) {
   try {
    int tid = Integer.parseInt(s);
    if (tid == self || android.os.Process.getThreadPriority(tid) >= 19) continue;
    android.os.Process.setThreadPriority(tid, 19); n++;
   } catch (Throwable gone) { }
  }
  return n;
 }

 private Object currentService() throws Exception {
  Object at = Class.forName("android.app.ActivityThread").getMethod("currentActivityThread").invoke(null);
  if (at == null) return null;
  if (services == null) { services = at.getClass().getDeclaredField("mServices"); services.setAccessible(true); }
  Map<?, ?> m = (Map<?, ?>) services.get(at);
  if (m == null) return null;
  for (Object s : m.values().toArray()) if (s != null && s.getClass().getName().equals(SERVICE)) return s;
  return null;
 }

 private boolean busy(Object service, boolean includeLoaded) throws Exception {
  if (service == null) return false;
  if (segmenting == null) {
   Class<?> c = service.getClass();
   segmenting = c.getDeclaredField("segmentingSet"); segmenting.setAccessible(true);
   loading = c.getDeclaredField("loadingSet"); loading.setAccessible(true);
   loaded = c.getDeclaredField("segmentLoadedMap"); loaded.setAccessible(true);
  }
  if (!((Collection<?>) segmenting.get(service)).isEmpty() || !((Collection<?>) loading.get(service)).isEmpty()) return true;
  return includeLoaded && !((Map<?, ?>) loaded.get(service)).isEmpty();
 }

 /** True when no book is open and nothing is being segmented. */
 private boolean idle() throws Exception {
  Object s = currentService();
  if (s != null) lastService = s;
  if (busy(lastService, false)) return false;
  return s == null || !busy(s, true);
 }

 public void run() {
  try { Thread.sleep(20000); } catch (InterruptedException stop) { return; }
  if (!"com.tencent.weread.eink:seg".equals(processName())) return;
  int self = android.os.Process.myTid();
  boolean doRelease = release;
  System.out.println("WEREAD_SEG_DICT_READY release=" + (doRelease ? 1 : 0) + " low=" + (low ? 1 : 0));
  int failures = 0, idleChecks = 0, tick = 0; boolean wasLoaded = false; long lowered = 0;
  while (failures < 5) {
   try {
    Thread.sleep(1000); tick++;
    if (low) { int n = lowerThreads(self); lowered += n; if (tick <= 1 && n > 0) System.out.println("WEREAD_SEG_DICT_LOW threads=" + n); }
    if (!doRelease || tick % 5 != 0) continue;
    if (singleton == null) {
     // WeRead's classes live in the application's class loader, not in this entry's.
     Object app = Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null);
     if (app == null) continue;
     ClassLoader cl = (ClassLoader) app.getClass().getMethod("getClassLoader").invoke(app);
     Field f = Class.forName("moai.ik.dic.Dictionary", false, cl).getDeclaredField("singleton");
     f.setAccessible(true); singleton = f; System.out.println("WEREAD_SEG_DICT_FIELD_FOUND");
    }
    Object dict = singleton.get(null);
    if (dict != null && !wasLoaded) System.out.println("WEREAD_SEG_DICT_LOADED heap_kb=" + heapKb() + " rss_kb=" + rssKb() + " lowered=" + lowered);
    wasLoaded = dict != null;
    if (dict == null) { idleChecks = 0; continue; }
    idleChecks = idle() ? idleChecks + 1 : 0;
    if (idleChecks < 2) continue;
    singleton.set(null, null);
    if (!idle()) { singleton.set(null, dict); idleChecks = 0; System.out.println("WEREAD_SEG_DICT_KEPT busy"); continue; }
    dict = null; wasLoaded = false; idleChecks = 0;
    long before = heapKb(); int rss = rssKb();
    Class<?> vm = Class.forName("dalvik.system.VMRuntime");
    Object runtime = vm.getMethod("getRuntime").invoke(null);
    vm.getMethod("concurrentGC").invoke(runtime);
    vm.getMethod("trimHeap").invoke(runtime);
    System.out.println("WEREAD_SEG_DICT_RELEASED heap_kb=" + before + "->" + heapKb() + " rss_kb=" + rss + "->" + rssKb());
   } catch (Throwable error) {
    failures++; System.out.println("WEREAD_SEG_DICT_ERROR " + error);
    if (error instanceof NoSuchFieldException || error instanceof ClassNotFoundException || error instanceof ClassCastException) doRelease = false;
   }
  }
  System.out.println("WEREAD_SEG_DICT_STOPPED");
 }
}
