import android.content.Context;
import android.graphics.Point;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.WindowManager;
/** Verify physical-size-preserving native display before launching the app. */
public final class NativeResolutionCheck {
 public static void main(String[] args)throws Exception {
  if(args.length!=0 || android.os.Process.myUid()!=0)throw new SecurityException("Root diagnostic only");
  Looper.prepareMainLooper();Class<?> type=Class.forName("android.app.ActivityThread");
  Object thread=type.getMethod("systemMain").invoke(null);
  Context context=(Context)type.getMethod("getSystemContext").invoke(thread);
  android.view.Display display=((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
  Point size=new Point();display.getRealSize(size);DisplayMetrics metrics=new DisplayMetrics();display.getRealMetrics(metrics);
  String density=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"ro.sf.lcd_density");
  if(size.x!=1264 || size.y!=1680 || metrics.densityDpi!=320 || Math.abs(metrics.density-2.0f)>.001 || !"320".equals(density))throw new AssertionError("Native display or density mismatch: "+size.x+"x"+size.y+" dpi="+metrics.densityDpi+" density="+density);
  System.out.println("LIBRA2_NATIVE_DISPLAY_CHECK_PASSED size=1264x1680 dpi=320 equivalent_dp=632x840");
  System.exit(0);
 }
}
