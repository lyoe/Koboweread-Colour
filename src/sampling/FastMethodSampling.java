import android.os.*;
import android.system.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.Arrays;
/** Bounded, root-only diagnostic, no APK or VM compilation parameter changes.
 * Fast-entry copy of Codex's MethodSamplingProbe: 50 ms sampling, up to 180 s,
 * new output file under /lab/fast/, stops early when /lab/fast/sampling-stop exists. */
public final class FastMethodSampling {
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
   Object profiler=info.getConstructor(String.class,ParcelFileDescriptor.class,int.class,boolean.class).newInstance(filename,fd,50000,false);
   if(!Boolean.TRUE.equals(control.invoke(am,target,0,true,profiler,0)))throw new IllegalStateException("Profile request rejected");
   started=true;long begin=SystemClock.elapsedRealtime();
   System.out.println("FAST_SAMPLING_STARTED target="+target+" interval_us=50000 max_ms="+duration+" uptime_ms="+begin);System.out.flush();
   while(SystemClock.elapsedRealtime()-begin<duration && !new File("/lab/fast/sampling-stop").exists()){
    if(!READY.isFile()||!Arrays.equals(session,ready())){System.out.println("FAST_SAMPLING_SESSION_ENDED");return;}
    Thread.sleep(200);
   }
  }finally{
   // The app is being terminated if readiness disappears; don't keep a dying server busy.
   if(started&&READY.isFile()&&Arrays.equals(session,ready())){
    if(!Boolean.TRUE.equals(control.invoke(am,target,0,false,null,0)))throw new IllegalStateException("Profile stop rejected");
    Thread.sleep(500); // app processes the asynchronous stop command
    System.out.println("FAST_SAMPLING_STOPPED bytes="+file.length()+" uptime_ms="+SystemClock.elapsedRealtime());
   }
  }
 }
 public static void main(String[]args)throws Exception {
  try{
   // args: selftest | capture [TARGET [MAX_MS]]; TARGET is a process name or pid.
   if(android.os.Process.myUid()!=0||args.length<1||args.length>3||!(args[0].equals("selftest")||args[0].equals("capture")))throw new SecurityException("Root fixed diagnostic only");
   if(!READY.isFile())throw new SecurityException("Ready session required");
   if(new android.os.StatFs("/data").getAvailableBytes()<140L*1024*1024)throw new IOException("Diagnostic requires 140 MiB free");
   byte[]session=ready();setup();
   boolean test=args[0].equals("selftest");
   String stamp=Long.toString(System.currentTimeMillis()/1000);
   String target=args.length>=2?args[1]:(test?"com.kobo.lab.smoke":"com.tencent.weread.eink");
   long max=args.length==3?Math.min(180000L,Long.parseLong(args[2])):(test?1000:180000);
   run(target,"/lab/fast/method-"+(test?"selftest":"capture")+"-"+target.replaceAll("[^A-Za-z0-9.]","_")+"-"+stamp+".trace",max,session);
   System.out.println("FAST_SAMPLING_COMPLETE");System.exit(0);
  }catch(Throwable e){e.printStackTrace();System.exit(1);}
 }
}
