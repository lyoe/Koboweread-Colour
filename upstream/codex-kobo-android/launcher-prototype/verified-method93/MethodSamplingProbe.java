import android.os.*;
import android.system.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.Arrays;
/** Bounded, root-only diagnostic, no APK or VM compilation parameter changes. */
public final class MethodSamplingProbe {
 static final File READY=new File("/lab/interactive-session-ready");
 static byte[] ready()throws IOException{try(FileInputStream in=new FileInputStream(READY)){byte[] out=new byte[4096];int n=in.read(out);if(n<0||n==out.length)throw new IOException("Invalid ready metadata");return Arrays.copyOf(out,n);}}
 static Object am;static Method control;static Class<?> info;
 static void setup()throws Exception {
  IBinder b=(IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,"activity");
  if(b==null)throw new IllegalStateException("No activity manager");
  am=Class.forName("android.app.ActivityManagerNative").getMethod("asInterface",IBinder.class).invoke(null,b);
  info=Class.forName("android.app.ProfilerInfo");
  control=Class.forName("android.app.IActivityManager").getMethod("profileControl",String.class,int.class,boolean.class,info,int.class);
 }
 static void run(String target,String filename,long duration,byte[] session)throws Exception {
  File file=new File(filename);if(!file.createNewFile())throw new SecurityException("Existing diagnostic file");
  Os.chmod(filename,0600);
  boolean started=false;
  try(ParcelFileDescriptor fd=ParcelFileDescriptor.open(file,ParcelFileDescriptor.MODE_READ_WRITE)){
   Object profiler=info.getConstructor(String.class,ParcelFileDescriptor.class,int.class,boolean.class).newInstance(filename,fd,100000,false);
   if(!Boolean.TRUE.equals(control.invoke(am,target,0,true,profiler,0)))throw new IllegalStateException("Profile request rejected");
   started=true;long begin=SystemClock.elapsedRealtime();
   System.out.println("METHOD93_PROFILE_REQUEST_ACCEPTED target="+target+" interval_us=100000 max_ms="+duration+" uptime_ms="+begin);System.out.flush();
   while(SystemClock.elapsedRealtime()-begin<duration){
    if(!READY.isFile()||!Arrays.equals(session,ready())){System.out.println("METHOD93_SESSION_ENDED");return;}
    Thread.sleep(200);
   }
  }finally{
   // The app is being terminated if readiness disappears; don't keep a dying server busy.
   if(started&&READY.isFile()&&Arrays.equals(session,ready())){
    if(!Boolean.TRUE.equals(control.invoke(am,target,0,false,null,0)))throw new IllegalStateException("Profile stop rejected");
    Thread.sleep(500); // app processes the asynchronous stop command
    System.out.println("METHOD93_STOP_REQUEST_ACCEPTED bytes="+file.length()+" uptime_ms="+SystemClock.elapsedRealtime());
   }
  }
 }
 public static void main(String[]args)throws Exception {
  try{
   if(android.os.Process.myUid()!=0||args.length!=1||!(args[0].equals("selftest")||args[0].equals("capture")))throw new SecurityException("Root fixed diagnostic only");
   if(!READY.isFile()||new File("/lab/weread-heap-headroom-enabled").exists()||new File("/lab/display-cadence15-enabled").exists())throw new SecurityException("Baseline ready session required");
   if(new android.os.StatFs("/data").getAvailableBytes()<140L*1024*1024)throw new IOException("Diagnostic requires 140 MiB free");
   byte[]session=ready();setup();
   boolean test=args[0].equals("selftest");
   run(test?"com.kobo.lab.smoke":"com.tencent.weread.eink",test?"/lab/method93-selftest.trace":"/lab/method93-main.trace",test?1000:120000,session);
   System.out.println("METHOD93_HELPER_COMPLETE");System.exit(0);
  }catch(Throwable e){e.printStackTrace();System.exit(1);}
 }
}
