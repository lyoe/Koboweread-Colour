import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.view.WindowManager;
import java.lang.reflect.Field;
/** Root-only live query of the actual WindowState under the original WMS lock.
 * Returns only a fixed classification, never private window titles or a cache. */
public final class LiveFocusService extends Binder {
 public static final String NAME="kobo.lab.live_focus",TOKEN="kobo.lab.LiveFocus.v1";
 private final Object windows,lock;private final Field focus,owner,attrs,inputHandle,paused,visible;
 private Object previousWindow;private int previousKind=-1;private long generation;
 private static Field field(Class<?> c,String name)throws Exception {Field f=c.getDeclaredField(name);f.setAccessible(true);return f;}
 private LiveFocusService(Object manager)throws Exception {
  windows=manager;lock=field(manager.getClass(),"mWindowMap").get(manager);
  focus=field(manager.getClass(),"mCurrentFocus");Class<?> state=Class.forName("com.android.server.wm.WindowState");
  owner=field(state,"mOwnerUid");attrs=field(state,"mAttrs");inputHandle=field(state,"mInputWindowHandle");
  Class<?> handle=Class.forName("com.android.server.input.InputWindowHandle");paused=field(handle,"paused");visible=field(handle,"visible");
  if(lock==null)throw new AssertionError("Missing WMS synchronization object");
  attachInterface(null,TOKEN);
 }
 public static void install(Object windows)throws Exception {
  if(android.os.Process.myUid()!=1000)throw new SecurityException("System server only");
  Class<?> service=Class.forName("android.os.ServiceManager");
  if(service.getMethod("checkService",String.class).invoke(null,NAME)!=null)throw new SecurityException("Duplicate live focus service");
  service.getMethod("addService",String.class,IBinder.class).invoke(null,NAME,new LiveFocusService(windows));
  System.out.println("ANDROID_LIVE_FOCUS_SERVICE_READY real_window_state=true root_only=true");
 }
 @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
  if(Binder.getCallingUid()!=0)throw new SecurityException("Root input adapter only");
  if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString(TOKEN);return true;}
  if(code!=IBinder.FIRST_CALL_TRANSACTION && code!=IBinder.FIRST_CALL_TRANSACTION+1)return super.onTransact(code,data,reply,flags);
  data.enforceInterface(TOKEN);if(data.dataAvail()!=0)throw new IllegalArgumentException("Unexpected focus query arguments");
  int result=0;long stamp;
  try{synchronized(lock){
   Object window=focus.get(windows);
   Object handle=window==null?null:inputHandle.get(window);
   if(window!=null && handle!=null && !paused.getBoolean(handle) && visible.getBoolean(handle) && owner.getInt(window)==10007){
    WindowManager.LayoutParams a=(WindowManager.LayoutParams)attrs.get(window);
    if("com.tencent.weread.eink".equals(a.packageName)){
     result=1;String title=String.valueOf(a.getTitle());
     if("com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity".equals(title))result=2;
     else if("com.tencent.weread.eink/com.tencent.weread.ReaderFragmentActivity".equals(title))result=3;
    }
   }
   // Resident launcher: the fixed private HOME of the lab image (kind 4).
   else if(window!=null && handle!=null && !paused.getBoolean(handle) && visible.getBoolean(handle) && owner.getInt(window)==10000){
    WindowManager.LayoutParams a=(WindowManager.LayoutParams)attrs.get(window);
    if("com.kobo.lab.smoke".equals(a.packageName) && "com.kobo.lab.smoke/com.kobo.lab.smoke.HomeActivity".equals(String.valueOf(a.getTitle())))result=4;
   }
   if(window!=previousWindow || result!=previousKind){
    if(generation==Long.MAX_VALUE)throw new IllegalStateException("Focus generation exhausted");
    generation++;previousWindow=window;previousKind=result;
   }
   stamp=generation;
  }}catch(IllegalAccessException e){throw new IllegalStateException("Live focus field unavailable",e);}
  reply.writeNoException();reply.writeInt(result);if(code==IBinder.FIRST_CALL_TRANSACTION+1)reply.writeLong(stamp);return true;
 }
}
