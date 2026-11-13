import android.content.Context;
import android.os.*;
/** Starts the actual matching WebView service and its normal provider preparation. */
public final class WebViewBootstrap {
 public static void start(Context context)throws Exception {
  final Class<?> type=Class.forName("com.android.server.webkit.WebViewUpdateService");
  final Object service=type.getConstructor(Context.class).newInstance(context);
  type.getMethod("onStart").invoke(service);
  final Handler handler=new Handler(Looper.getMainLooper());
  handler.postDelayed(new Runnable(){int attempts;public void run(){try{
   Object ready=Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.ams.ready");
   if(!"1".equals(ready)){if(++attempts>100)throw new AssertionError("AMS not ready for WebView");handler.postDelayed(this,100);return;}
   type.getMethod("prepareWebViewInSystemServer").invoke(service);
   System.out.println("ANDROID_ORIGINAL_WEBVIEW_PROVIDER_PREPARATION_RETURNED");
  }catch(Throwable failure){failure.printStackTrace();System.exit(32);}}},100);
  System.out.println("ANDROID_ORIGINAL_WEBVIEW_UPDATE_SERVICE_STARTED");
 }
}
