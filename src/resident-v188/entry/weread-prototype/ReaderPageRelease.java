import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.lang.reflect.Field;
import java.util.ArrayList;
/** WeRead 1.8.8 keeps every page it ever laid out while a book stays open
 * (2026-11-07, docs/1.8.8版本.md): Page.bindView registers the page's
 * mExtraDataObserver (a Page$1) with its chapter's progress adapter
 * (BookProgressImpl.mChapterProgress, a DataSetObservable), but
 * Page.releaseView, called when the page leaves the screen, only removes the
 * page's LiveData observers. The adapter's observer list then holds every
 * page ever shown (~0.2 MB each: paragraphs, typeset characters, review data)
 * until the book is closed; after ~250-300 pages the 96 MB heap is full.
 *
 * Every 30 s, on the main thread, this unregisters the Page$1 observers of
 * pages that are off screen (drawViewCallback == null, which releaseView
 * sets) and clears their mChapterProgress, the state bindBookProgress
 * expects for a page that is not registered: if the page is shown again,
 * bindView -> bindBookProgress registers it again as for a new page. Pages
 * on screen are not touched. Nothing of WeRead's code is replaced; only its
 * own public unregisterObserver and that field are used.
 * Off: fast-v188/pagefix-disabled (data/local/tmp/kobo-pagefix-disabled).
 * Any unexpected structure stops it for the rest of the process. */
final class ReaderPageRelease implements Runnable {
 private static final long PERIOD_MS = 30000;
 private static final String PAGE_OBSERVER = "com.tencent.weread.reader.domain.Page$1";
 private final Handler handler;
 private Field delegateField, observersField, outerField, callbackField, progressField;
 private int failures; private long released;
 private ReaderPageRelease(Handler handler) { this.handler = handler; }

 static void start() {
  if (new File("/data/local/tmp/kobo-pagefix-disabled").exists()) { System.out.println("WEREAD_PAGE_RELEASE_DISABLED"); return; }
  Thread t = new Thread(new Runnable() { public void run() {
   try {
    // ActivityThread.main prepares the main looper right after this starts.
    for (int i = 0; i < 600 && Looper.getMainLooper() == null; i++) Thread.sleep(100);
    Looper main = Looper.getMainLooper();
    if (main == null) { System.out.println("WEREAD_PAGE_RELEASE_NO_LOOPER"); return; }
    Handler h = new Handler(main);
    h.postDelayed(new ReaderPageRelease(h), PERIOD_MS);
    System.out.println("WEREAD_PAGE_RELEASE_READY period_ms=" + PERIOD_MS);
   } catch (Throwable error) { System.out.println("WEREAD_PAGE_RELEASE_START_FAILED " + error); }
  } }, "kobo-page-release");
  t.setDaemon(true); t.start();
 }

 public void run() {
  try {
   prune();
   failures = 0;
  } catch (Throwable error) {
   System.out.println("WEREAD_PAGE_RELEASE_ERROR " + error);
   if (++failures >= 3) { System.out.println("WEREAD_PAGE_RELEASE_STOPPED"); return; }
  }
  handler.postDelayed(this, PERIOD_MS);
 }

 private static Field field(Class<?> c, String name) throws NoSuchFieldException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Field f = k.getDeclaredField(name); f.setAccessible(true); return f; } catch (NoSuchFieldException next) { }
  }
  throw new NoSuchFieldException(c.getName() + "." + name);
 }

 private void prune() throws Exception {
  if (delegateField == null) {
   Object app = Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null);
   if (app == null) return;
   ClassLoader loader = ((android.content.Context) app).getClassLoader();
   delegateField = field(Class.forName("com.tencent.weread.reader.plugin.PluginLifecycle", false, loader), "mActionDelegate");
   observersField = field(android.database.Observable.class, "mObservers");
  }
  Object delegate = delegateField.get(null);
  if (delegate == null) return; // no book open
  Object fragment;
  try { fragment = field(delegate.getClass(), "this$0").get(delegate); } catch (NoSuchFieldException other) { return; }
  if (fragment == null || !fragment.getClass().getName().endsWith(".BookFragment")) return;
  Object progress = field(fragment.getClass(), "mBookProgress").get(fragment);
  if (progress == null) return;
  Object chapters = field(progress.getClass(), "mChapterProgress").get(progress);
  if (!(chapters instanceof android.util.SparseArray)) return;
  android.util.SparseArray<?> adapters = (android.util.SparseArray<?>) chapters;
  int pruned = 0, kept = 0;
  for (int i = 0; i < adapters.size(); i++) {
   Object adapter = adapters.valueAt(i);
   if (!(adapter instanceof android.database.DataSetObservable)) continue;
   ArrayList<?> list = (ArrayList<?>) observersField.get(adapter);
   Object[] observers;
   synchronized (list) { observers = list.toArray(); }
   for (Object observer : observers) {
    if (observer == null || !PAGE_OBSERVER.equals(observer.getClass().getName())) continue;
    if (outerField == null) outerField = field(observer.getClass(), "this$0");
    Object page = outerField.get(observer);
    if (page == null) continue;
    if (callbackField == null) { callbackField = field(page.getClass(), "drawViewCallback"); progressField = field(page.getClass(), "mChapterProgress"); }
    if (callbackField.get(page) != null) { kept++; continue; } // on screen
    ((android.database.DataSetObservable) adapter).unregisterObserver((android.database.DataSetObserver) observer);
    if (progressField.get(page) == adapter) progressField.set(page, null);
    pruned++;
   }
  }
  if (pruned > 0) { released += pruned; System.out.println("WEREAD_PAGE_RELEASE released=" + pruned + " on_screen=" + kept + " total=" + released); }
 }
}
