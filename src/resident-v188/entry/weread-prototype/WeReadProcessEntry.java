import java.lang.reflect.InvocationTargetException;
/** Process-local compatibility settings before the real, unmodified ActivityThread. */
public final class WeReadProcessEntry {
 public static void main(String[] args)throws Exception {
  if(android.os.Process.myUid()<10000 || android.os.Process.myUid()>=20000)throw new SecurityException("Application UID required");
  Class<?> vm=Class.forName("dalvik.system.VMRuntime");
  Object runtime=vm.getMethod("getRuntime").invoke(null);
  vm.getMethod("setTargetSdkVersion",int.class).invoke(runtime,31);
  // The available display backend is a software Canvas backend, without GLES 2.
  Class.forName("android.view.ThreadedRenderer").getMethod("disable",boolean.class).invoke(null,true);
  Class<?> web=Class.forName("android.webkit.WebViewFactory");web.getMethod("prepareWebViewInZygote").invoke(null);
  java.lang.reflect.Field reserved=web.getDeclaredField("sAddressSpaceReserved");reserved.setAccessible(true);
  if(!reserved.getBoolean(null))throw new AssertionError("WebView address reservation failed");
  System.out.println("WEREAD_WEBVIEW_ADDRESS_SPACE_RESERVED");
  System.out.println("WEREAD_PROCESS_COMPAT_READY targetSdk=31 software_ui=true uid="+android.os.Process.myUid());
  WebViewReadDiagnostics.start();
  ReaderPageRelease.start();
  MemoryTrim.start();
  HeapDumpRequest.start();
  SegDictRelease.start();
  FontSizeCache.start();
  PoolPriority.start();
  SyncThrottle.start();
  ChapterPrefetch.start();
  MarkUsersCache.start();
  try{Class.forName("android.app.ActivityThread").getMethod("main",String[].class).invoke(null,(Object)args);}
  catch(InvocationTargetException error){Throwable cause=error.getCause();if(cause instanceof Error)throw (Error)cause;if(cause instanceof Exception)throw (Exception)cause;throw new RuntimeException(cause);}
 }
}
