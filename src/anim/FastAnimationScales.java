import android.os.IBinder;
import java.lang.reflect.Method;
import java.util.Arrays;
/** Root-only: set the window, transition and animator scales for the fast
 * entry. "off" sets 0,0,0 (e-ink cannot show the frames, but each animation
 * frame holds the window lock while waiting for SurfaceFlinger); "window" sets
 * 0,0,1 (window and activity-transition animations off, the app's own
 * animators unchanged); "on" restores 1,1,1 so other entries keep Android's
 * defaults. */
public final class FastAnimationScales {
 public static void main(String[] args)throws Exception {
  if(android.os.Process.myUid()!=0||args.length!=1||!(args[0].equals("off")||args[0].equals("window")||args[0].equals("on")))throw new SecurityException("Fixed root animation control");
  IBinder window=(IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,"window");
  if(window==null)throw new IllegalStateException("No window manager");
  Object wm=Class.forName("android.view.IWindowManager$Stub").getMethod("asInterface",IBinder.class).invoke(null,window);
  Class<?> type=Class.forName("android.view.IWindowManager");
  Method get=type.getMethod("getAnimationScales"),set=type.getMethod("setAnimationScales",float[].class);
  float[] before=(float[])get.invoke(wm);
  float v=args[0].equals("on")?1f:0f;float[] target={v,v,args[0].equals("window")?1f:v};
  set.invoke(wm,(Object)target);
  float[] after=(float[])get.invoke(wm);
  if(!Arrays.equals(after,target))throw new AssertionError("Animation scales not applied: "+Arrays.toString(after));
  System.out.println("FAST_ANIMATION_SCALES before="+Arrays.toString(before)+" after="+Arrays.toString(after));
 }
}
