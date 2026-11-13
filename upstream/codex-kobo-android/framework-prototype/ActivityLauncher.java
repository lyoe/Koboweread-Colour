import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Process;
import android.util.SparseArray;
import java.io.*;
import java.lang.reflect.*;
/** Fixed-package adapter using AMS addAppLocked, real Process.start and bindApplication. */
public final class ActivityLauncher {
 public static void start(Context context,Object manager,Object installer)throws Exception {
  String name="com.kobo.lab.smoke";
  ApplicationInfo info=context.getPackageManager().getApplicationInfo(name,0x000c0000);
  if(info.uid!=10000 || (info.flags&ApplicationInfo.FLAG_SYSTEM)==0 || (info.flags&ApplicationInfo.FLAG_PERSISTENT)==0)
   throw new AssertionError("Unexpected probe package identity uid="+info.uid+" flags="+info.flags);
  System.out.println("ANDROID_PROBE_PACKAGE_REGISTERED uid="+info.uid+" source="+info.sourceDir+" data="+info.dataDir);
  installer.getClass().getMethod("createAppData",String.class,String.class,int.class,int.class,int.class,String.class,int.class)
   .invoke(installer,null,name,0,3,info.uid,(String)ApplicationInfo.class.getField("seinfo").get(info),info.targetSdkVersion);
  Class<?> service=manager.getClass();
  Object record;
  synchronized(manager){
   Method add=service.getDeclaredMethod("addAppLocked",ApplicationInfo.class,boolean.class,String.class);add.setAccessible(true);
   record=add.invoke(manager,info,false,null);
  }
  Field pidField=record.getClass().getDeclaredField("pid");pidField.setAccessible(true);int pid=pidField.getInt(record);
  if(pid<=0)throw new AssertionError("AMS process backend did not return a live PID");
  System.out.println("ANDROID_AMS_PROCESS_START_RETURNED pid="+pid+" uid="+info.uid);System.out.flush();
  final Object activityManager=manager;
  final Class<?> managerType=service;
  final String packageName=name;
  final int appUid=info.uid;
  final android.os.Handler handler=new android.os.Handler(android.os.Looper.getMainLooper());
  handler.postDelayed(new Runnable(){int attempts;public void run(){try{
   Method find=managerType.getDeclaredMethod("getProcessRecordLocked",String.class,int.class,boolean.class);find.setAccessible(true);
   Object record; synchronized(activityManager){record=find.invoke(activityManager,packageName,appUid,true);}
   Field appThread=record.getClass().getDeclaredField("thread");appThread.setAccessible(true);
   if(appThread.get(record)==null){if(++attempts>80)throw new AssertionError("ApplicationThread did not attach");handler.postDelayed(this,100);return;}
   managerType.getMethod("systemReady",Runnable.class).invoke(activityManager,(Object)null);
   Class.forName("android.os.SystemProperties").getMethod("set",String.class,String.class).invoke(null,"kobo.lab.ams.ready","1");
   System.out.println("ANDROID_AMS_SYSTEM_READY_COMPLETED");System.out.flush();
  }catch(Throwable error){error.printStackTrace();System.exit(31);}}},100);
 }
}
