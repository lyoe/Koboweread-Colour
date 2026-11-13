import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.LinkedHashMap;
import java.util.Map;
/** Session cache for the user lookups behind 热门划线 (best marks), WeRead
 * 1.9.7 (2026-11-11, docs/1.9.7版本.md "开书：热门划线查用户缓存").
 *
 * Every book open, BestMarkContentService.getBestBookMarksFromDB reads the
 * book's best marks and, for each one, getBestMarkContentUsersByIds looks up
 * every user id in its list through the static Function1
 * BestMarkContentService.getUserById (ModuleInitializer's lambda:
 * UserService.getUserById, one SQLite query per id). That was ~2.1 s CPU of
 * each open (sampling, 2026-11-10), on WRCommonPool, just after the page shows.
 * The same ids repeat across marks and across opens, so the field is replaced
 * by a Proxy of the same interface that remembers found users for the session
 * (up to 4000, least recently used dropped). Not found (null) is not
 * remembered: a later sync may add the user. What WeRead shows is unchanged.
 * Main process only, installed once the first Activity exists. Off:
 * fast-v197/markusers-disabled (data/local/tmp/kobo-markusers-disabled).
 * Logs WEREAD_MARK_USERS_* (running totals 10 s after each burst of lookups). Any error: one line,
 * the original function is put back and the cache stays off. */
final class MarkUsersCache implements Runnable, InvocationHandler {
 private static final int MAX = 4000;
 private final Handler handler;
 private Field field;
 private Object original, proxy;
 private Method invoke1;
 private volatile boolean off;
 private int polls;
 private final Runnable reporter = new Runnable() { public void run() { report(); } };
 private long calls, hits, missNanos, misses;
 private final LinkedHashMap<Object, Object> cache = new LinkedHashMap<Object, Object>(256, 0.75f, true) {
  protected boolean removeEldestEntry(Map.Entry<Object, Object> e) { return size() > MAX; }
 };
 private MarkUsersCache(Handler h) { handler = h; }

 static void start() {
  if (new File("/data/local/tmp/kobo-markusers-disabled").exists()) { System.out.println("WEREAD_MARK_USERS_DISABLED"); return; }
  Thread t = new Thread(new Runnable() { public void run() {
   try {
    for (int i = 0; i < 600 && Looper.getMainLooper() == null; i++) Thread.sleep(100);
    Looper main = Looper.getMainLooper();
    if (main == null) return;
    Handler h = new Handler(main);
    h.postDelayed(new MarkUsersCache(h), 500);
   } catch (Throwable error) { System.out.println("WEREAD_MARK_USERS_START_FAILED " + error); }
  } }, "kobo-markusers");
  t.setDaemon(true); t.start();
 }

 private static Field field(Class<?> c, String name) throws NoSuchFieldException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Field f = k.getDeclaredField(name); f.setAccessible(true); return f; } catch (NoSuchFieldException next) { }
  }
  throw new NoSuchFieldException(c.getName() + "." + name);
 }

 /** Main thread: wait for the first Activity (WeRead's modules are initialized), then install. */
 public void run() {
  try {
   if (++polls > 1200) { System.out.println("WEREAD_MARK_USERS_NO_ACTIVITY"); return; }
   Class<?> atc = Class.forName("android.app.ActivityThread");
   Object at = atc.getMethod("currentActivityThread").invoke(null);
   Object app = atc.getMethod("currentApplication").invoke(null);
   if (at == null || app == null) { handler.postDelayed(this, 100); return; }
   android.content.Context ctx = (android.content.Context) app;
   String proc = (String) atc.getMethod("currentProcessName").invoke(null);
   if (proc != null && !proc.equals(ctx.getApplicationInfo().processName)) return; // :seg and other processes
   Map<?, ?> acts = (Map<?, ?>) field(at.getClass(), "mActivities").get(at);
   if (acts == null || acts.isEmpty()) { handler.postDelayed(this, 100); return; }
   install(ctx.getClassLoader());
  } catch (Throwable error) { System.out.println("WEREAD_MARK_USERS_ERROR " + error); }
 }

 private void install(ClassLoader cl) throws Exception {
  Class<?> service = Class.forName("com.tencent.weread.bestmarkcontentservice.model.BestMarkContentService", true, cl);
  Field f = field(service, "getUserById");
  Class<?> iface = f.getType();
  Object old = f.get(null);
  if (!iface.isInterface() || old == null || !iface.isInstance(old)) { System.out.println("WEREAD_MARK_USERS_SKIP type=" + iface.getName() + " value=" + old); return; }
  Method m = null;
  for (Method k : iface.getMethods()) if (k.getName().equals("invoke") && k.getParameterTypes().length == 1) m = k;
  if (m == null) { System.out.println("WEREAD_MARK_USERS_SKIP no_invoke"); return; }
  invoke1 = m; original = old; field = f;
  proxy = Proxy.newProxyInstance(cl, new Class<?>[] { iface }, this);
  f.set(null, proxy);
  System.out.println("WEREAD_MARK_USERS_READY interface=" + iface.getName() + " max=" + MAX);
 }

 /** WRCommonPool threads. */
 public Object invoke(Object p, Method method, Object[] args) throws Throwable {
  if (method.getDeclaringClass() == Object.class) {
   String n = method.getName();
   if (n.equals("equals")) return p == args[0];
   if (n.equals("hashCode")) return System.identityHashCode(p);
   if (n.equals("toString")) return "MarkUsersCache@" + Integer.toHexString(System.identityHashCode(p));
  }
  if (off || args == null || args.length != 1 || !method.getName().equals("invoke") || args[0] == null) return call(method, args);
  Object key = args[0], user;
  synchronized (cache) { user = cache.get(key); calls++; if (user != null) hits++; }
  if (user == null) {
   long t0 = System.nanoTime();
   user = call(method, args);
   long dt = System.nanoTime() - t0;
   synchronized (cache) { misses++; missNanos += dt; if (user != null) cache.put(key, user); }
  }
  // One summary line 10 s after the last lookup of a burst (one book open).
  handler.removeCallbacks(reporter); handler.postDelayed(reporter, 10000);
  return user;
 }

 private Object call(Method method, Object[] args) throws Throwable {
  try { return method.invoke(original, args); }
  catch (InvocationTargetException error) { throw error.getCause(); }
  catch (Throwable error) { disable(error); return invoke1.invoke(original, args); }
 }

 private void disable(Throwable error) {
  off = true;
  try { if (field.get(null) == proxy) field.set(null, original); } catch (Throwable ignored) { }
  System.out.println("WEREAD_MARK_USERS_ERROR " + error + " restored=1");
 }

 private void report() {
  synchronized (cache) {
   System.out.println("WEREAD_MARK_USERS calls=" + calls + " hits=" + hits + " misses=" + misses + " miss_ms=" + (missNanos / 1000000)
    + " saved_ms_est=" + (misses > 0 ? hits * missNanos / misses / 1000000 : 0) + " cached=" + cache.size());
  }
 }
}
