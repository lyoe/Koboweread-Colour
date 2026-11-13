import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
/** Typeset the chapters around the one being read before they are opened
 * (WeRead 1.9.7, 2026-11-10, docs/1.9.7版本.md "跳章预排版").
 *
 * A chapter entered for the first time (or after a font change) is converted
 * from the EPUB and paginated while WeRead shows "加载中": about 4 s of CPU on
 * this single core. WeRead itself does this ahead of time only for the next
 * chapter (BookFragment.checkTypeSettingNextChapter). This runs WeRead's own
 * routine for that, BookFragment.checkTypeSetting(uid), for the chapters within
 * RANGE of the current one (+1, -1, +2, -2, ...), one at a time, while a book
 * is open:
 *  - only 30 s after the book was opened (its per-open syncs run first) and
 *    45 s after the reader last moved to another chapter: WeRead typesets on
 *    one thread, so a prefetch must not hold it while the reader is moving;
 *  - only chapters WeRead says it could typeset (isChapterCanReTypeSetting:
 *    real, not paid, already downloaded) and that need it
 *    (WRReaderCursor.isChapterNeedTypeSetting);
 *  - never while WeRead is typesetting (currentRetypeSetting), and the next
 *    one only after the previous one is done (or 120 s);
 *  - each uid is added to WeRead's preloadTypeSettingChapterSet, so its own
 *    next-chapter preload does not repeat it.
 * Checked every 10 s on the main thread (all calls are WeRead's own, as its
 * next-chapter preload makes them). Main process only. Off:
 * fast-v188/chapterprefetch-disabled (data/local/tmp/kobo-chapterprefetch-disabled);
 * range: data/local/tmp/kobo-chapterprefetch-range (1-20, default 3).
 *
 * Whole-book mode (2026-11-11): while the Kobo is on power (the battery's
 * sysfs status, bound by the session at data/local/tmp/kobo-power-status, is
 * not "Discharging") and the reader has stayed on one page for BOOK_IDLE_MS,
 * the rest of the book's chapters are typeset the same way, nearest first.
 * A page turn or the power going away stops it before the next chapter (the
 * one under way finishes: WeRead's typesetting cannot be cancelled). Off:
 * fast-v188/bookprefetch-disabled (data/local/tmp/kobo-bookprefetch-disabled;
 * the status file is then not bound either). While a chapter is being
 * typeset the check runs every second, so DONE ms= is exact to about 1 s.
 * Logs WEREAD_CHAPTER_PREFETCH_*, WEREAD_BOOK_PREFETCH_*. */
final class ChapterPrefetch implements Runnable {
 private static final String BOOK_FRAGMENT = "com.tencent.weread.book.fragment.BookFragment";
 private static final long TICK_MS = 10000, FAST_TICK_MS = 1000, SETTLE_MS = 30000, PENDING_MS = 120000, STAY_MS = 45000, BOOK_IDLE_MS = 120000;
 private static final File POWER = new File("/data/local/tmp/kobo-power-status");
 private static ChapterPrefetch instance;
 private final Handler handler;
 private final int range;
 private final boolean bookEnabled;
 private Class<?> bookFragmentClass;
 private Method pageContainer, firstCharPos, charChapter, charPosition, checkTypeSetting, canReTypeSetting, visibleRealChapter, needTypeSetting, chapters, chapterUid;
 private Field cursorField, preloadSetField, retypingField;
 private Object fragment;          // the BookFragment being watched
 private long fragmentSince;
 private final Set<Integer> tried = new HashSet<Integer>();
 private final Set<Integer> prepared = new HashSet<Integer>(); // typeset by this class in this book
 private int pendingUid = 0, lastAt = 0; private String lastPage; private long pendingSince, movedAt, pageAt;
 private boolean pendingBook, bookActive, bookComplete;
 private int bookDone; private long bookMs;
 private int started, done, polls;
 private boolean failed;
 private ChapterPrefetch(Handler handler, int range, boolean bookEnabled) { this.handler = handler; this.range = range; this.bookEnabled = bookEnabled; }

 static void start() {
  if (new File("/data/local/tmp/kobo-chapterprefetch-disabled").exists()) { System.out.println("WEREAD_CHAPTER_PREFETCH_DISABLED"); return; }
  int r = 3;
  try {
   java.io.BufferedReader in = new java.io.BufferedReader(new java.io.FileReader("/data/local/tmp/kobo-chapterprefetch-range"));
   try { int v = Integer.parseInt(in.readLine().trim()); if (v >= 1 && v <= 20) r = v; } finally { in.close(); }
  } catch (Throwable absent) { }
  final int range = r;
  final boolean book = !new File("/data/local/tmp/kobo-bookprefetch-disabled").exists();
  Thread t = new Thread(new Runnable() { public void run() {
   try {
    for (int i = 0; i < 600 && Looper.getMainLooper() == null; i++) Thread.sleep(100);
    Looper main = Looper.getMainLooper();
    if (main == null) return;
    Handler h = new Handler(main);
    instance = new ChapterPrefetch(h, range, book);
    h.postDelayed(instance, TICK_MS);
   } catch (Throwable error) { System.out.println("WEREAD_CHAPTER_PREFETCH_START_FAILED " + error); }
  } }, "kobo-chapterprefetch");
  t.setDaemon(true); t.start();
 }

 private static Field field(Class<?> c, String name) throws NoSuchFieldException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Field f = k.getDeclaredField(name); f.setAccessible(true); return f; } catch (NoSuchFieldException next) { }
  }
  throw new NoSuchFieldException(c.getName() + "." + name);
 }
 private static Method method(Class<?> c, String name, Class<?>... types) throws NoSuchMethodException {
  for (Class<?> k = c; k != null; k = k.getSuperclass()) {
   try { Method m = k.getDeclaredMethod(name, types); m.setAccessible(true); return m; } catch (NoSuchMethodException next) { }
  }
  throw new NoSuchMethodException(c.getName() + "." + name);
 }

 public void run() {
  if (failed) return;
  try { tick(); }
  catch (Throwable error) {
   failed = true;
   System.out.println("WEREAD_CHAPTER_PREFETCH_ERROR " + error + (error.getCause() != null ? " cause=" + error.getCause() : ""));
   return;
  }
  handler.postDelayed(this, pendingUid != 0 || bookActive ? FAST_TICK_MS : TICK_MS);
 }

 private void tick() throws Exception {
  Class<?> atc = Class.forName("android.app.ActivityThread");
  Object at = atc.getMethod("currentActivityThread").invoke(null);
  Object app = atc.getMethod("currentApplication").invoke(null);
  if (at == null || app == null) return;
  android.content.Context ctx = (android.content.Context) app;
  if (bookFragmentClass == null) {
   String proc = (String) atc.getMethod("currentProcessName").invoke(null);
   if (proc != null && !proc.equals(ctx.getApplicationInfo().processName)) { failed = true; return; } // :seg
   if (++polls > 360) return; // classes appear once WeRead has initialized; keep trying for an hour
   ClassLoader cl = ctx.getClassLoader();
   Class<?> bf = Class.forName(BOOK_FRAGMENT, false, cl);
   checkTypeSetting = method(bf, "checkTypeSetting", int.class);
   canReTypeSetting = method(bf, "isChapterCanReTypeSetting", int.class);
   visibleRealChapter = method(bf, "getVisibleRealChapter");
   cursorField = field(bf, "mReaderCursor");
   preloadSetField = field(bf, "preloadTypeSettingChapterSet");
   retypingField = field(bf, "currentRetypeSetting");
   Class<?> cursor = Class.forName("com.tencent.weread.reader.cursor.WRReaderCursor", false, cl);
   needTypeSetting = cursor.getMethod("isChapterNeedTypeSetting", int.class);
   chapters = cursor.getMethod("chapters");
   // chapters() holds ChapterIndex objects; getId() is the chapter uid (WRReaderCursorImpl.nextChapterUid compares it so).
   chapterUid = Class.forName("com.tencent.weread.storage.ChapterIndexInterface", false, cl).getMethod("getId");
   pageContainer = method(bf, "getPageContainer");
   Class<?> layout = Class.forName("com.tencent.weread.reader.container.pagecontainer.PageLayoutManagerAction", false, cl);
   // WeRead's own (it saves reading progress with it); the container's
   // findFirstCompletelyVisibleCharPos is not implemented for paged reading.
   firstCharPos = method(bf, "getFirstVisibleCharPos");
   Class<?> cp = Class.forName("com.tencent.weread.reader.container.pagecontainer.CharPosition", false, cl);
   charChapter = cp.getMethod("getChapterUid");
   charPosition = cp.getMethod("getPosition");
   bookFragmentClass = bf;
   System.out.println("WEREAD_CHAPTER_PREFETCH_READY range=" + range + " book=" + (bookEnabled ? 1 : 0) + " power=" + power());
  }
  Object f = findBookFragment(at);
  // A book left for a moment (another fragment on top, not resumed) keeps its
  // state; only a different BookFragment (another book, or the reader opened
  // again) starts over.
  if (f == null) return;
  if (f != fragment) {
   if (bookActive) bookStop("book");
   fragment = f; fragmentSince = System.currentTimeMillis(); tried.clear(); prepared.clear(); pendingUid = 0; lastAt = 0;
   lastPage = null; bookComplete = false; bookDone = 0; bookMs = 0;
   System.out.println("WEREAD_CHAPTER_PREFETCH_BOOK id=" + Integer.toHexString(System.identityHashCode(f)) + " started=" + started + " done=" + done);
  }
  long now = System.currentTimeMillis();
  // The reader's place as chapter + character position of the first visible
  // character (BookFragment.getFirstVisibleCharPos): page numbers (also relative to the chapter's start)
  // shift whenever a chapter before the current one is typeset (2026-11-11:
  // that stopped book mode after nearly every such chapter without a turn).
  Object where = pageContainer.invoke(f) == null ? null : firstCharPos.invoke(f);
  if (where != null) {
   String pg = charChapter.invoke(where) + ":" + charPosition.invoke(where);
   if (!pg.equals(lastPage)) {
    if (lastPage != null && bookActive) bookStop("page " + lastPage + "->" + pg);
    lastPage = pg; pageAt = now;
   }
  }
  if (bookActive && !charging()) bookStop("power");
  if (now - fragmentSince < SETTLE_MS) return;
  Object cursor = cursorField.get(f);
  if (cursor == null) return;
  if (pendingUid != 0) {
   boolean need = (Boolean) needTypeSetting.invoke(cursor, pendingUid);
   long age = System.currentTimeMillis() - pendingSince;
   if (need && age < PENDING_MS) return;
   if (!need) { done++; prepared.add(pendingUid); if (pendingBook) { bookDone++; bookMs += age; } }
   System.out.println("WEREAD_CHAPTER_PREFETCH_" + (need ? "TIMEOUT" : "DONE") + " uid=" + pendingUid + " ms=" + age + " mode=" + (pendingBook ? "book" : "near")
    + (pendingBook ? " book_done=" + bookDone + " book_ms=" + bookMs : ""));
   pendingUid = 0;
  }
  if ((Boolean) retypingField.get(f)) return;
  int[] visible = (int[]) visibleRealChapter.invoke(f);
  if (visible == null || visible.length == 0) return;
  if (visible[0] != lastAt) { lastAt = visible[0]; movedAt = System.currentTimeMillis(); System.out.println("WEREAD_CHAPTER_PREFETCH_AT uid=" + lastAt + " prepared_here=" + prepared.contains(lastAt)); }
  // WeRead typesets on one thread: a prefetch started right after the reader
  // moved would make the next jump wait behind it (2026-11-10: 6.8 s instead of
  // ~2 s). Start only once the reader has stayed in a chapter for a while.
  if (System.currentTimeMillis() - movedAt < STAY_MS) return;
  List<?> list = (List<?>) chapters.invoke(cursor);
  if (list == null || list.isEmpty()) return;
  List<Integer> uids = new ArrayList<Integer>(list.size());
  for (Object c : list) uids.add((Integer) chapterUid.invoke(c));
  int at0 = uids.indexOf(visible[0]);
  if (at0 < 0) return;
  @SuppressWarnings("unchecked") Collection<Integer> preload = (Collection<Integer>) preloadSetField.get(f);
  if (startNearest(f, cursor, uids, at0, 1, range, preload, visible[0], false)) return;
  // Whole-book mode: on power and left on one page for a while.
  if (!bookEnabled || bookComplete) return;
  if (now - pageAt < BOOK_IDLE_MS || now - movedAt < BOOK_IDLE_MS || !charging()) return;
  if (!bookActive) { bookActive = true; System.out.println("WEREAD_BOOK_PREFETCH_START chapters=" + uids.size() + " current=" + visible[0] + " power=" + power()); }
  if (startNearest(f, cursor, uids, at0, range + 1, uids.size(), preload, visible[0], true)) return;
  bookActive = false; bookComplete = true;
  System.out.println("WEREAD_BOOK_PREFETCH_COMPLETE book_done=" + bookDone + " book_ms=" + bookMs + " prepared=" + prepared.size() + " chapters=" + uids.size());
 }

 private void bookStop(String why) {
  bookActive = false;
  System.out.println("WEREAD_BOOK_PREFETCH_STOP reason=" + why + " book_done=" + bookDone + " book_ms=" + bookMs + (pendingUid != 0 ? " finishing=" + pendingUid : ""));
 }

 /** On power: the battery status is anything but Discharging (Charging, Not charging, Full). */
 private static String power() {
  try {
   java.io.BufferedReader in = new java.io.BufferedReader(new java.io.FileReader(POWER));
   try { String l = in.readLine(); return l == null ? "empty" : l.trim().replace(' ', '_'); } finally { in.close(); }
  } catch (Throwable absent) { return "none"; }
 }
 private static boolean charging() { String p = power(); return !p.equals("none") && !p.equals("empty") && !p.equals("Discharging") && !p.equals("Unknown"); }

 /** Starts WeRead's typesetting of the nearest untried chapter at distance from..to; true if one was started. */
 private boolean startNearest(Object f, Object cursor, List<Integer> uids, int at0, int from, int to, Collection<Integer> preload, int current, boolean book) throws Exception {
  for (int d = from; d <= to; d++) {
   for (int sign = 1; sign >= -1; sign -= 2) {
    int i = at0 + sign * d;
    if (i < 0 || i >= uids.size()) continue;
    int uid = uids.get(i);
    if (tried.contains(uid)) continue;
    tried.add(uid);
    if (!(Boolean) canReTypeSetting.invoke(f, uid)) continue;
    if (!(Boolean) needTypeSetting.invoke(cursor, uid)) continue;
    if (preload != null) preload.add(uid);
    pendingUid = uid; pendingSince = System.currentTimeMillis(); pendingBook = book; started++;
    System.out.println("WEREAD_CHAPTER_PREFETCH_START uid=" + uid + " current=" + current + " distance=" + (sign * d) + " mode=" + (book ? "book" : "near"));
    checkTypeSetting.invoke(f, uid);
    return true;
   }
  }
  return false;
 }

 /** The resumed BookFragment of any activity, searched through fragment managers. */
 private Object findBookFragment(Object at) throws Exception {
  Map<?, ?> acts = (Map<?, ?>) field(at.getClass(), "mActivities").get(at);
  if (acts == null) return null;
  for (Object record : acts.values()) {
   Object activity = field(record.getClass(), "activity").get(record);
   if (activity == null) continue;
   Method fm;
   try { fm = activity.getClass().getMethod("getSupportFragmentManager"); } catch (NoSuchMethodException none) { continue; }
   Object found = search(fm.invoke(activity), 0);
   if (found != null) return found;
  }
  return null;
 }
 private Object search(Object manager, int depth) throws Exception {
  if (manager == null || depth > 4) return null;
  List<?> list = (List<?>) manager.getClass().getMethod("getFragments").invoke(manager);
  if (list == null) return null;
  for (Object fr : list) {
   if (fr == null) continue;
   if (bookFragmentClass.isInstance(fr)) {
    if ((Boolean) fr.getClass().getMethod("isResumed").invoke(fr)) return fr;
    continue;
   }
   Object child;
   try { child = fr.getClass().getMethod("getChildFragmentManager").invoke(fr); }
   catch (Throwable notAttached) { continue; } // a fragment not attached (yet) has none
   Object found = search(child, depth + 1);
   if (found != null) return found;
  }
  return null;
 }
}
