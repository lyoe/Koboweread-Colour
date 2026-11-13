import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
/** WeRead 1.9.7 font-size adaptation without a LevelDB read per view
 * (2026-11-09, docs/1.9.7版本.md).
 *
 * FontSizeManager registers a static layout listener on every ViewGroup of
 * every registered window (and a hierarchy listener on some). On each layout
 * it calls dispatch() for every child whose "current text size" tag differs
 * from getCurrentFontSize(); ViewGroups never get that tag, so every layout
 * walks the whole subtree, and every TextView in it calls
 * getCurrentFontSize(), which builds a KVDomain, reads LevelDB and parses JSON
 * each time (30-38% of the main thread after a book opens).
 *
 * This replaces the two listener objects (the static fields that dispatch()
 * installs, and the instances already on views) with copies of WeRead's own
 * logic that use a cached font size. The cache is cleared by WeRead's
 * OnFontSizeChangeListener callback (setCurrentFontSize) and re-read at least
 * every 2 s. The per-view work (tags, the adapt function, invalidate) is
 * WeRead's own, reached by reflection. Any error puts WeRead's listeners back.
 * Main process only, starts once the first Activity exists (so the class is
 * initialized by WeRead itself). Off: fast-v197/fontcache-disabled
 * (data/local/tmp/kobo-fontcache-disabled). Logs WEREAD_FONTSIZE_*. */
final class FontSizeCache implements Runnable {
 private static final String FSM = "com.tencent.weread.fontsize.FontSizeManager";
 private static final long TTL_MS = 2000;
 private static FontSizeCache instance; // the change listener proxy is held weakly by WeRead
 private final Handler handler;
 private Object fsm, oldLayout, oldHierarchy, changeListener;
 private Field layoutField, hierarchyField, listenerInfoField, layoutListenersField, groupHierarchyField;
 private Method getCurrent, hModeGetter, isFunctionOfArity, function1Invoke;
 private Object hModeHolder, hierarchyMode;
 private Class<?> qqFaceView;
 private int tagLogic, tagCurrent;
 private int cached = Integer.MIN_VALUE; private long cachedAt;
 private boolean active, failed;
 private long reads, hits; private int walks, quietWalks, polls;
 final View.OnLayoutChangeListener layout = new View.OnLayoutChangeListener() {
  public void onLayoutChange(View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) {
   if (!active) { ((View.OnLayoutChangeListener) oldLayout).onLayoutChange(v, l, t, r, b, ol, ot, or, ob); return; }
   try {
    if (!(v instanceof ViewGroup)) return;
    ViewGroup g = (ViewGroup) v; int n = g.getChildCount();
    for (int i = 0; i < n; i++) { View c = g.getChildAt(i); if (c != null && !matches(c)) dispatch(c); }
   } catch (Throwable error) { fail(error); }
  }
 };
 final ViewGroup.OnHierarchyChangeListener hierarchy = new ViewGroup.OnHierarchyChangeListener() {
  public void onChildViewAdded(View parent, View child) {
   if (!active) { ((ViewGroup.OnHierarchyChangeListener) oldHierarchy).onChildViewAdded(parent, child); return; }
   try { if (child != null && !matches(child)) dispatch(child); } catch (Throwable error) { fail(error); }
  }
  public void onChildViewRemoved(View parent, View child) { }
 };
 private FontSizeCache(Handler handler) { this.handler = handler; }

 static void start() {
  if (new File("/data/local/tmp/kobo-fontcache-disabled").exists()) { System.out.println("WEREAD_FONTSIZE_CACHE_DISABLED"); return; }
  Thread t = new Thread(new Runnable() { public void run() {
   try {
    for (int i = 0; i < 600 && Looper.getMainLooper() == null; i++) Thread.sleep(100);
    Looper main = Looper.getMainLooper();
    if (main == null) return;
    Handler h = new Handler(main);
    instance = new FontSizeCache(h);
    h.postDelayed(instance, 500);
   } catch (Throwable error) { System.out.println("WEREAD_FONTSIZE_CACHE_START_FAILED " + error); }
  } }, "kobo-fontsize");
  t.setDaemon(true); t.start();
 }

 private static Field field(Class<?> c, String name) throws NoSuchFieldException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Field f = k.getDeclaredField(name); f.setAccessible(true); return f; } catch (NoSuchFieldException next) { }
  }
  throw new NoSuchFieldException(c.getName() + "." + name);
 }

 /** Main thread: wait for the first Activity, install, then re-walk the windows a few times. */
 public void run() {
  if (failed) return;
  try {
   if (!active) {
    if (++polls > 1200) { System.out.println("WEREAD_FONTSIZE_CACHE_NO_ACTIVITY"); return; }
    Object at = Class.forName("android.app.ActivityThread").getMethod("currentActivityThread").invoke(null);
    Object app = Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null);
    if (at == null || app == null) { handler.postDelayed(this, 100); return; }
    String name = ((android.content.Context) app).getApplicationInfo().processName;
    String proc = (String) Class.forName("android.app.ActivityThread").getMethod("currentProcessName").invoke(null);
    if (proc != null && !proc.equals(name)) return; // :seg and other processes
    Map<?, ?> acts = (Map<?, ?>) field(at.getClass(), "mActivities").get(at);
    if (acts == null || acts.isEmpty()) { handler.postDelayed(this, 100); return; }
    install(((android.content.Context) app).getClassLoader());
    handler.postDelayed(this, 5000);
    return;
   }
   int replaced = walkWindows();
   walks++;
   quietWalks = replaced == 0 ? quietWalks + 1 : 0;
   System.out.println("WEREAD_FONTSIZE_CACHE_WALK n=" + walks + " replaced=" + replaced + " reads=" + reads + " hits=" + hits);
   if (quietWalks < 3) handler.postDelayed(this, walks < 2 ? 30000 : 60000);
  } catch (Throwable error) { fail(error); }
 }

 private void install(ClassLoader cl) throws Exception {
  Class<?> m = Class.forName(FSM, true, cl);
  fsm = field(m, "INSTANCE").get(null);
  getCurrent = m.getMethod("getCurrentFontSize"); getCurrent.setAccessible(true);
  layoutField = field(m, "mOnLayoutChangeListener");
  hierarchyField = field(m, "mOnHierarchyChangeListener");
  oldLayout = layoutField.get(null); oldHierarchy = hierarchyField.get(null);
  Class<?> ids = Class.forName("com.tencent.weread.fontsize.R$id", true, cl);
  tagLogic = ids.getField("weread_adaptive_text_size_logic").getInt(null);
  tagCurrent = ids.getField("weread_adaptive_current_text_size").getInt(null);
  qqFaceView = Class.forName("com.qmuiteam.qmui.qqface.QMUIQQFaceView", false, cl);
  Class<?> h = Class.forName("d2.h", true, cl);
  hModeHolder = field(h, "j").get(null);
  hModeGetter = Class.forName("d2.h$e", false, cl).getMethod("a", ViewGroup.class); hModeGetter.setAccessible(true);
  hierarchyMode = field(Class.forName("d2.h$d", true, cl), "LISTEN_ON_HIERARCHY_CHANGE").get(null);
  isFunctionOfArity = Class.forName("kotlin.jvm.internal.H", true, cl).getMethod("f", Object.class, int.class); isFunctionOfArity.setAccessible(true);
  function1Invoke = Class.forName("l4.l", false, cl).getMethod("invoke", Object.class); function1Invoke.setAccessible(true);
  listenerInfoField = field(View.class, "mListenerInfo");
  layoutListenersField = field(Class.forName("android.view.View$ListenerInfo"), "mOnLayoutChangeListeners");
  groupHierarchyField = field(ViewGroup.class, "mOnHierarchyChangeListener");
  if (oldLayout == null || oldHierarchy == null || hModeHolder == null) throw new IllegalStateException("listeners not set");
  // Font size changes go through setCurrentFontSize -> fontSizeChange -> these listeners.
  final Class<?> iface = Class.forName("com.tencent.weread.fontsize.OnFontSizeChangeListener", false, cl);
  changeListener = Proxy.newProxyInstance(cl, new Class<?>[] { iface }, new InvocationHandler() {
   public Object invoke(Object proxy, Method method, Object[] args) {
    String n = method.getName();
    if (n.equals("equals")) return proxy == args[0];
    if (n.equals("hashCode")) return System.identityHashCode(proxy);
    if (n.equals("toString")) return "kobo-fontsize-cache";
    cached = Integer.MIN_VALUE;
    return null;
   }
  });
  m.getMethod("addFontSizeChangeListener", iface).invoke(fsm, changeListener);
  int size = font();
  layoutField.set(null, layout); hierarchyField.set(null, hierarchy);
  active = true;
  int replaced = walkWindows();
  System.out.println("WEREAD_FONTSIZE_CACHE_READY size=" + size + " replaced=" + replaced);
 }

 private int font() throws Exception {
  long now = android.os.SystemClock.uptimeMillis();
  if (cached != Integer.MIN_VALUE && now - cachedAt < TTL_MS) { hits++; return cached; }
  reads++;
  cached = (Integer) getCurrent.invoke(fsm); cachedAt = now;
  return cached;
 }

 /** getViewFontSizeCurrent(child) == getCurrentFontSize() (null never matches). */
 private boolean matches(View v) throws Exception {
  Object cur = v.getTag(tagCurrent);
  return cur instanceof Integer && ((Integer) cur).intValue() == font();
 }

 /** WeRead's FontSizeManager.dispatch with the cached size. */
 private void dispatch(View v) throws Exception {
  if (v instanceof ViewGroup) {
   ViewGroup g = (ViewGroup) v;
   if (hModeGetter.invoke(hModeHolder, g) == hierarchyMode) g.setOnHierarchyChangeListener(hierarchy);
   else g.addOnLayoutChangeListener(layout);
   int n = g.getChildCount();
   for (int i = 0; i < n; i++) { View c = g.getChildAt(i); if (c != null) dispatch(c); }
   return;
  }
  if (!(v instanceof TextView) && !qqFaceView.isInstance(v)) return;
  Object logic = v.getTag(tagLogic);
  Object cur = v.getTag(tagCurrent);
  int size = font();
  if (cur instanceof Integer && ((Integer) cur).intValue() == size) return;
  if (!(Boolean) isFunctionOfArity.invoke(null, logic, 1)) return;
  v.setTag(tagCurrent, Integer.valueOf(size));
  function1Invoke.invoke(logic, v);
  v.invalidate();
 }

 /** Swap WeRead's listener instances on every attached view for ours (or back). */
 private int walkWindows() throws Exception {
  Class<?> wmg = Class.forName("android.view.WindowManagerGlobal");
  Object global = wmg.getMethod("getInstance").invoke(null);
  List<?> roots = (List<?>) field(wmg, "mRoots").get(global);
  int n = 0;
  Object[] list; synchronized (field(wmg, "mLock").get(global)) { list = roots.toArray(); }
  for (Object root : list) {
   View v = (View) field(root.getClass(), "mView").get(root);
   if (v != null) n += swap(v, active ? oldLayout : layout, active ? layout : oldLayout, active ? oldHierarchy : hierarchy, active ? hierarchy : oldHierarchy);
  }
  return n;
 }

 @SuppressWarnings("unchecked")
 private int swap(View v, Object fromL, Object toL, Object fromH, Object toH) throws Exception {
  int n = 0;
  Object info = listenerInfoField.get(v);
  if (info != null) {
   ArrayList<Object> ls = (ArrayList<Object>) layoutListenersField.get(info);
   if (ls != null) {
    int i = ls.indexOf(fromL);
    if (i >= 0) { if (ls.contains(toL)) ls.remove(i); else ls.set(i, toL); n++; }
   }
  }
  if (v instanceof ViewGroup) {
   ViewGroup g = (ViewGroup) v;
   if (groupHierarchyField.get(g) == fromH) { groupHierarchyField.set(g, toH); n++; }
   int c = g.getChildCount();
   for (int i = 0; i < c; i++) { View child = g.getChildAt(i); if (child != null) n += swap(child, fromL, toL, fromH, toH); }
  }
  return n;
 }

 /** Put WeRead's own listeners back for good. */
 private void fail(Throwable error) {
  System.out.println("WEREAD_FONTSIZE_CACHE_ERROR " + error);
  if (failed) return;
  failed = true;
  if (!active) return;
  active = false; // our listeners now forward to WeRead's
  try {
   layoutField.set(null, oldLayout); hierarchyField.set(null, oldHierarchy);
   System.out.println("WEREAD_FONTSIZE_CACHE_RESTORED replaced=" + walkWindows());
  } catch (Throwable again) { System.out.println("WEREAD_FONTSIZE_CACHE_RESTORE_FAILED " + again); }
 }
}
