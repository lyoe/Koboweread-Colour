import android.content.ComponentName;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Collection;
/** Defers only overdue DBDataService during a root-marked initial launch.
 * The original timeout message and ProcessRecord are retained, with one fixed
 * absolute deadline. Completion still uses the unmodified ActiveServices path.
 */
public final class StartupServiceGrace implements Handler.Callback {
 private static final int CALLBACK_PROBE=0x4b475250;
 private final Object probeToken=new Object();
 private static final String MARKER="/lab/weread-startup-service-grace";
 private final Object manager;private final Handler handler;private final Handler.Callback previous;
 private final Field uid,process,pid,thread,foreground,services,name,executingStart,lru;
 private final int timeoutMessage,serviceTimeout;
 private final Method propertyGet;
 private StartupServiceGrace(Object am,Handler h,Handler.Callback old)throws Exception {
  manager=am;handler=h;previous=old;
  ClassLoader loader=am.getClass().getClassLoader();
  Class<?> pr=Class.forName("com.android.server.am.ProcessRecord",false,loader);
  Class<?> sr=Class.forName("com.android.server.am.ServiceRecord",false,loader);
  Class<?> active=Class.forName("com.android.server.am.ActiveServices",false,loader);
  uid=field(pr,"uid");process=field(pr,"processName");pid=field(pr,"pid");thread=field(pr,"thread");
  foreground=field(pr,"execServicesFg");services=field(pr,"executingServices");
  name=field(sr,"name");executingStart=field(sr,"executingStart");
  lru=field(am.getClass(),"mLruProcesses");
  timeoutMessage=field(am.getClass(),"SERVICE_TIMEOUT_MSG").getInt(null);
  serviceTimeout=field(active,"SERVICE_TIMEOUT").getInt(null);
  if(timeoutMessage!=12 || serviceTimeout!=20000)throw new SecurityException("Unmatched API24 service timeout layout");
  propertyGet=Class.forName("android.os.SystemProperties").getMethod("get",String.class);
 }
 private static Field field(Class<?> type,String name)throws Exception {
  Field result=type.getDeclaredField(name);result.setAccessible(true);return result;
 }
 public static void install(Object am)throws Exception {
  if(android.os.Process.myUid()!=1000)throw new SecurityException("System server only");
  Handler h=(Handler)field(am.getClass(),"mHandler").get(am);
  Field callback=field(Handler.class,"mCallback");Handler.Callback old=(Handler.Callback)callback.get(h);
  if(old instanceof StartupServiceGrace)throw new SecurityException("Duplicate startup grace handler");
  StartupServiceGrace wrapper=new StartupServiceGrace(am,h,old);callback.set(h,wrapper);
  if(callback.get(h)!=wrapper)throw new AssertionError("Startup handler install failed");
  System.out.println("WEREAD_DB_STARTUP_GRACE_INSTALLED limit_ms=120000 original_service_timeout_ms=20000");
  if(!h.sendMessage(h.obtainMessage(CALLBACK_PROBE,wrapper.probeToken)))throw new AssertionError("Startup callback probe not queued");
 }
 private boolean trustedMarker(){
  try{StructStat s=Os.lstat(MARKER);return s.st_uid==0 && OsConstants.S_ISREG(s.st_mode) && (s.st_mode&07777)==0400;}
  catch(Exception absent){return false;}
 }
 @Override public boolean handleMessage(Message message){
  if(message.what==CALLBACK_PROBE && message.obj==probeToken){System.out.println("WEREAD_DB_STARTUP_CALLBACK_PROBE_OK");return true;}
  if(message.what==timeoutMessage && message.obj!=null){
   try{
    long deadline=0;int appPid=0;
    synchronized(manager){
     Object record=message.obj;
     if(uid.getInt(record)==10007 && "com.tencent.weread.eink".equals(process.get(record))
         && thread.get(record)!=null && ((Collection<?>)lru.get(manager)).contains(record)){
      long now=SystemClock.uptimeMillis();boolean db=false,other=false;
      for(Object service:(Collection<?>)services.get(record)){
       if(executingStart.getLong(service)>=now-serviceTimeout)continue;
       ComponentName component=(ComponentName)name.get(service);
       if("com.tencent.weread.eink".equals(component.getPackageName())
           && "com.tencent.weread.aidl.DBDataService".equals(component.getClassName()))db=true;
       else other=true;
      }
      String raw=(String)propertyGet.invoke(null,"kobo.lab.weread.start_uptime");
      long launch=raw.matches("[0-9]{1,18}")?Long.parseLong(raw):0;
      long delay=StartupServiceGracePolicy.delay(now,launch,trustedMarker(),uid.getInt(record),
          (String)process.get(record),foreground.getBoolean(record),db,other);
      if(delay>0)deadline=now+delay;
      appPid=pid.getInt(record);
     }
    }
    if(deadline>0){
     // Same what/obj lets the original serviceDoneExecutingLocked remove it.
     Message copy=Message.obtain(message);
     if(handler.sendMessageAtTime(copy,deadline)){
      System.out.println("WEREAD_DB_STARTUP_TIMEOUT_DEFERRED pid="+appPid+" deadline_uptime_ms="+deadline);
      return true;
     }
    }
   }catch(Exception error){System.out.println("WEREAD_DB_STARTUP_GRACE_FALLBACK cause="+error.getClass().getName());}
  }
  return previous!=null && previous.handleMessage(message);
 }
}
