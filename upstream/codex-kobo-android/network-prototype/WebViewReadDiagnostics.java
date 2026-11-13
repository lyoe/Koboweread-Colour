import android.os.*;
/** Opt-in, process-local debugging for the known public agreement page only.
 * No app callbacks, WebView clients or APK code are replaced. */
public final class WebViewReadDiagnostics {
 public static void start(){
  if(!"1".equals(System.getenv("KOBO_WEREAD_WEB_DIAGNOSTICS")))return;
  Thread observer=new Thread(new Runnable(){public void run(){try{
   for(int attempt=0;attempt<600;attempt++){
    final Looper main=Looper.getMainLooper();
    if(main!=null&&Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null)!=null){
     new Handler(main).post(new Runnable(){public void run(){android.webkit.WebView.setWebContentsDebuggingEnabled(true);System.out.println("WEREAD_LOCAL_WEBVIEW_READ_DIAGNOSTICS_ENABLED pid="+android.os.Process.myPid());WebViewRenderingDiagnostic.start();}});return;
    }
    Thread.sleep(100);
   }
   System.out.println("WEREAD_LOCAL_WEBVIEW_DIAGNOSTICS_NOT_READY");
  }catch(Throwable error){error.printStackTrace();}}},"WebViewReadDiagnostics");observer.setDaemon(true);observer.start();
 }
}
