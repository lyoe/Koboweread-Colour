import android.content.Context;
import android.os.IBinder;
import android.view.accessibility.AccessibilityManager;
import java.lang.reflect.*;
/** Start the platform's real service; retain settings and ordinary permissions. */
public final class AccessibilityBootstrap {
 public static void install(Context context)throws Exception {
  if(android.os.Process.myUid()!=1000)throw new SecurityException("System-server UID required");
  Class<?> sm=Class.forName("android.os.ServiceManager");
  if(sm.getMethod("checkService",String.class).invoke(null,Context.ACCESSIBILITY_SERVICE)!=null)throw new IllegalStateException("Accessibility service already exists");
  if(Class.forName("com.android.server.LocalServices").getMethod("getService",Class.class).invoke(null,Class.forName("android.view.WindowManagerInternal"))==null)throw new IllegalStateException("Window manager required");
  IBinder service=(IBinder)Class.forName("com.android.server.accessibility.AccessibilityManagerService").getConstructor(Context.class).newInstance(context);
  sm.getMethod("addService",String.class,IBinder.class).invoke(null,Context.ACCESSIBILITY_SERVICE,service);
  if(!service.pingBinder())throw new AssertionError("Accessibility service not alive");
  System.out.println("ANDROID_REAL_ACCESSIBILITY_SERVICE_PUBLISHED");
 }
 public static void verifyClient(Context context)throws Exception {
  AccessibilityManager client=(AccessibilityManager)context.getSystemService(Context.ACCESSIBILITY_SERVICE);
  if(client==null)throw new AssertionError("No accessibility manager client");
  boolean enabled=client.isEnabled();Field field=AccessibilityManager.class.getDeclaredField("mService");field.setAccessible(true);Object service=field.get(client);
  if(service==null)throw new AssertionError("Accessibility client did not connect");
  long begin=android.os.SystemClock.elapsedRealtimeNanos();
  for(int n=0;n<10000;n++)if(client.isEnabled()!=enabled)throw new AssertionError("State changed during client check");
  if(field.get(client)!=service)throw new AssertionError("Client lost cached service");
  System.out.println("ANDROID_ACCESSIBILITY_CLIENT_CACHE_OK uid="+android.os.Process.myUid()+" queries=10000 enabled="+enabled+" elapsed_ns="+(android.os.SystemClock.elapsedRealtimeNanos()-begin));
 }
}
