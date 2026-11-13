import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.Map;
/** At most one refresh per book (or chapter) every 12 hours of three lists
 * WeRead 1.9.7 re-downloads on every book open (2026-11-10,
 * docs/1.9.7版本.md "开书、退书采样"):
 *  - book light reads (书友评价), BaseBookDetailService.LoadBookLightRead,
 *    saved row by row: ~4.4 s CPU per open;
 *  - best marks (热门划线), BaseBestMarkContentService.LoadBestBookMarks,
 *    then users looked up one by one: 2.3-4.5 s;
 *  - chapter reviews (本章想法), BaseBookReviewListService.LoadBookChapterReviewList,
 *    then the chapter's reviews re-read: ~2.1 s.
 * Each service object (WRKotlinService.of / ServiceHolder) holds its Retrofit
 * interface in $$delegate_0; that field is replaced by a java.lang.reflect.Proxy
 * that passes every call through, except these three methods for a key
 * (book id, plus list type and chapter for reviews) that was refreshed less
 * than 12 h ago: they return Observable.empty(), so WeRead's own map/save
 * steps never run and what is already in its database stays on screen.
 * Calls within 30 s of a refresh still pass (paging inside one refresh).
 * Refresh times are kept in files/kobo-sync-throttle (wall clock), so the
 * limit holds across sessions. Main process only, installed once the first
 * Activity exists. Off: fast-v197/syncthrottle-disabled
 * (data/local/tmp/kobo-syncthrottle-disabled). Logs WEREAD_SYNC_THROTTLE_*. */
final class SyncThrottle implements Runnable, InvocationHandler {
 private static final long WINDOW_MS = 12L * 3600 * 1000, PAGING_MS = 30 * 1000;
 private static final String[][] TARGETS = {
  // service object source, Retrofit interface, throttled method, key argument indexes
  {"holder:getBookDetailService", "com.tencent.weread.bookdetailservice.model.BaseBookDetailService", "LoadBookLightRead", "0"},
  {"of:com.tencent.weread.bestmarkcontentservice.model.BestMarkContentService", "com.tencent.weread.bestmarkcontentservice.model.BaseBestMarkContentService", "LoadBestBookMarks", "0"},
  {"holder:getBookReviewListService", "com.tencent.weread.bookreviewlistservice.model.BaseBookReviewListService", "LoadBookChapterReviewList", "0,1,2"},
 };
 private static SyncThrottle instance;
 private final Handler handler;
 // Keyed by identity: a HashMap would call proxy.hashCode(), which comes back
 // into invoke() and looks the proxy up again (endless recursion, 11-10 crash).
 private final Map<Object, String[]> targets = new IdentityHashMap<Object, String[]>(); // proxy -> target row
 private final Map<Object, Object> real = new IdentityHashMap<Object, Object>();         // proxy -> Retrofit object
 private final Map<String, Long> last = new HashMap<String, Long>();
 private File store;
 private Method empty;
 private int polls, passed, skipped;
 private SyncThrottle(Handler handler) { this.handler = handler; }

 static void start() {
  if (new File("/data/local/tmp/kobo-syncthrottle-disabled").exists()) { System.out.println("WEREAD_SYNC_THROTTLE_DISABLED"); return; }
  Thread t = new Thread(new Runnable() { public void run() {
   try {
    for (int i = 0; i < 600 && Looper.getMainLooper() == null; i++) Thread.sleep(100);
    Looper main = Looper.getMainLooper();
    if (main == null) return;
    Handler h = new Handler(main);
    instance = new SyncThrottle(h);
    h.postDelayed(instance, 500);
   } catch (Throwable error) { System.out.println("WEREAD_SYNC_THROTTLE_START_FAILED " + error); }
  } }, "kobo-syncthrottle");
  t.setDaemon(true); t.start();
 }

 private static Field field(Class<?> c, String name) throws NoSuchFieldException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Field f = k.getDeclaredField(name); f.setAccessible(true); return f; } catch (NoSuchFieldException next) { }
  }
  throw new NoSuchFieldException(c.getName() + "." + name);
 }

 /** Main thread: wait for the first Activity, then wrap the three services once. */
 public void run() {
  try {
   if (++polls > 1200) { System.out.println("WEREAD_SYNC_THROTTLE_NO_ACTIVITY"); return; }
   Class<?> atc = Class.forName("android.app.ActivityThread");
   Object at = atc.getMethod("currentActivityThread").invoke(null);
   Object app = atc.getMethod("currentApplication").invoke(null);
   if (at == null || app == null) { handler.postDelayed(this, 100); return; }
   android.content.Context ctx = (android.content.Context) app;
   String proc = (String) atc.getMethod("currentProcessName").invoke(null);
   if (proc != null && !proc.equals(ctx.getApplicationInfo().processName)) return; // :seg and other processes
   Map<?, ?> acts = (Map<?, ?>) field(at.getClass(), "mActivities").get(at);
   if (acts == null || acts.isEmpty()) { handler.postDelayed(this, 100); return; }
   install(ctx.getClassLoader(), new File(ctx.getFilesDir(), "kobo-sync-throttle"));
  } catch (Throwable error) { System.out.println("WEREAD_SYNC_THROTTLE_ERROR " + error); }
 }

 private void install(ClassLoader cl, File file) throws Exception {
  store = file; load();
  empty = Class.forName("rx.Observable", true, cl).getMethod("empty");
  Object holder = Class.forName("com.tencent.weread.serviceholder.ServiceHolder", true, cl).getField("INSTANCE").get(null);
  Object companion = Class.forName("com.tencent.weread.network.WRKotlinService", true, cl).getField("Companion").get(null);
  int wrapped = 0;
  for (String[] row : TARGETS) {
   try {
    Object service;
    if (row[0].startsWith("holder:")) {
     Object fn = holder.getClass().getMethod(row[0].substring(7)).invoke(holder);
     service = fn.getClass().getMethod("invoke").invoke(fn);
    } else {
     service = companion.getClass().getMethod("of", Class.class).invoke(companion, Class.forName(row[0].substring(3), true, cl));
    }
    Class<?> iface = Class.forName(row[1], true, cl);
    Field d = field(service.getClass(), "$$delegate_0");
    Object old = d.get(service);
    if (old == null || !iface.isInterface() || !iface.isInstance(old)) { System.out.println("WEREAD_SYNC_THROTTLE_SKIP " + row[2] + " delegate=" + old); continue; }
    if (Proxy.isProxyClass(old.getClass()) && Proxy.getInvocationHandler(old) == this) continue;
    iface.getMethod(row[2], findParams(iface, row[2]));
    Object proxy = Proxy.newProxyInstance(cl, new Class<?>[] { iface }, this);
    synchronized (this) { targets.put(proxy, row); real.put(proxy, old); }
    d.set(service, proxy);
    if (d.get(service) != proxy) { System.out.println("WEREAD_SYNC_THROTTLE_SKIP " + row[2] + " set_failed"); continue; }
    wrapped++;
   } catch (Throwable error) { System.out.println("WEREAD_SYNC_THROTTLE_SKIP " + row[2] + " " + error); }
  }
  System.out.println("WEREAD_SYNC_THROTTLE_READY wrapped=" + wrapped + " hours=" + (WINDOW_MS / 3600000) + " known=" + last.size());
 }

 private static Class<?>[] findParams(Class<?> iface, String name) throws NoSuchMethodException {
  for (Method m : iface.getMethods()) if (m.getName().equals(name)) return m.getParameterTypes();
  throw new NoSuchMethodException(iface.getName() + "." + name);
 }

 /** Any thread (WeRead's pools). */
 public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
  if (method.getDeclaringClass() == Object.class) {
   String n = method.getName();
   if (n.equals("equals")) return proxy == args[0];
   if (n.equals("hashCode")) return System.identityHashCode(proxy);
   if (n.equals("toString")) return "SyncThrottle@" + Integer.toHexString(System.identityHashCode(proxy));
  }
  String[] row; Object target;
  synchronized (this) { row = targets.get(proxy); target = real.get(proxy); }
  if (target == null) throw new IllegalStateException("SyncThrottle: unknown proxy");
  if (row != null && method.getName().equals(row[2]) && args != null) {
   StringBuilder key = new StringBuilder(row[2]);
   for (String i : row[3].split(",")) { int k = Integer.parseInt(i); key.append('/').append(k < args.length ? args[k] : "?"); }
   String k = key.toString(); long now = System.currentTimeMillis(); boolean pass;
   synchronized (this) {
    Long t = last.get(k);
    pass = t == null || now - t >= WINDOW_MS || now < t || now - t < PAGING_MS;
    if (pass && (t == null || now - t >= PAGING_MS || now < t)) { last.put(k, now); save(now); }
    if (pass) passed++; else skipped++;
   }
   System.out.println("WEREAD_SYNC_THROTTLE " + (pass ? "pass " : "skip ") + k + " passed=" + passed + " skipped=" + skipped);
   if (!pass) return empty.invoke(null);
  }
  try { return method.invoke(target, args); }
  catch (InvocationTargetException error) { throw error.getCause(); }
 }

 private void load() {
  try {
   if (!store.exists()) return;
   byte[] b = new byte[(int) Math.min(store.length(), 1 << 20)];
   FileInputStream in = new FileInputStream(store);
   try { int n = 0, r; while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r; } finally { in.close(); }
   for (String line : new String(b, "UTF-8").split("\n")) {
    int tab = line.indexOf('\t');
    if (tab > 0) try { last.put(line.substring(tab + 1), Long.parseLong(line.substring(0, tab))); } catch (NumberFormatException skip) { }
   }
  } catch (Throwable error) { System.out.println("WEREAD_SYNC_THROTTLE_LOAD_FAILED " + error); }
 }

 /** Called with the lock held; drops entries past the window, rewrites the small file. */
 private void save(long now) {
  try {
   StringBuilder s = new StringBuilder();
   for (Iterator<Map.Entry<String, Long>> it = last.entrySet().iterator(); it.hasNext();) {
    Map.Entry<String, Long> e = it.next();
    if (now - e.getValue() >= WINDOW_MS || now < e.getValue()) { it.remove(); continue; }
    s.append(e.getValue()).append('\t').append(e.getKey()).append('\n');
   }
   File tmp = new File(store.getPath() + ".tmp");
   FileOutputStream out = new FileOutputStream(tmp);
   try { out.write(s.toString().getBytes("UTF-8")); } finally { out.close(); }
   if (!tmp.renameTo(store)) System.out.println("WEREAD_SYNC_THROTTLE_SAVE_FAILED rename");
  } catch (Throwable error) { System.out.println("WEREAD_SYNC_THROTTLE_SAVE_FAILED " + error); }
 }
}
