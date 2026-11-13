import android.content.Context;
import android.graphics.Point;
import android.os.Looper;
import android.view.WindowManager;
/** Read-only display and network checks; never injects input or starts WeRead. */
public final class Libra2RuntimeCheck {
 public static void main(String[] args)throws Exception {
  if(args.length!=0 || android.os.Process.myUid()!=0)throw new SecurityException("Root diagnostic only");
  Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  Point bounds=new Point();((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealSize(bounds);
  if(bounds.x!=632 || bounds.y!=840)throw new AssertionError("Libra2 display geometry mismatch");
  System.out.println("LIBRA2_ANDROID_DISPLAY_GEOMETRY_OK 632x840");
  HostDefaultConnectivityService.verifyQueries(context);
  HostWifiQueries.verifyActivityQueries();
  System.out.println("LIBRA2_ANDROID_RUNTIME_QUERY_CHECKS_PASSED");
  System.exit(0);
 }
}
