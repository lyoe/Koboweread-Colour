import android.os.*;
import android.system.*;
import java.io.*;
/** Root-only, bounded diagnostics for this private development runtime. */
public final class SystemSamplingService extends Binder {
 static final String TOKEN="kobo.lab.SystemSampling.v96";
 static final String DIR="/lab/system-sampling96";
 private volatile int state=0; // 0 idle, 1 starting, 2 running, 3 complete, 4 failed
 private volatile String error="";private volatile long size;
 private int jobs;
 public static void install()throws Exception {
  if(android.os.Process.myUid()!=1000)throw new SecurityException("System UID required");
  Class<?> sm=Class.forName("android.os.ServiceManager");
  if(sm.getMethod("checkService",String.class).invoke(null,"kobo.system_sampling")!=null)throw new IllegalStateException("Duplicate profiler");
  sm.getMethod("addService",String.class,IBinder.class).invoke(null,"kobo.system_sampling",new SystemSamplingService());
  System.out.println("SYSTEM96_IN_PROCESS_SAMPLING_SERVICE_READY");
 }
 private static byte[] ready()throws Exception {
  StructStat s=Os.lstat("/lab/interactive-session-ready");
  if(s.st_uid!=0||(s.st_mode&07777)!=0400||!OsConstants.S_ISREG(s.st_mode)||s.st_size<=0||s.st_size>=4096)throw new SecurityException("Invalid ready metadata");
  return (s.st_dev+":"+s.st_ino+":"+s.st_mtime+":"+s.st_ctime+":"+s.st_size).getBytes("US-ASCII");
 }
 private synchronized void start(final boolean test)throws Exception {
  if(state==1||state==2||jobs>=2||(test&&jobs!=0)||(!test&&(jobs!=1||state!=3)))throw new IllegalStateException("Invalid diagnostic sequence");
  StructStat d=Os.lstat(DIR);if(d.st_uid!=1000||(d.st_mode&0777)!=0700||!OsConstants.S_ISDIR(d.st_mode))throw new SecurityException("Invalid private diagnostic directory");
  if(new StatFs("/data").getAvailableBytes()<138L*1024*1024)throw new IOException("Insufficient diagnostic headroom");
  final byte[] session=ready();final File file=new File(DIR,test?"selftest.trace":"transition.trace");
  if(!file.createNewFile())throw new IOException("Diagnostic file exists");Os.chmod(file.toString(),0600);
  jobs++;state=1;size=0;error="";
  new Thread(new Runnable(){public void run(){boolean started=false;try{
   Debug.startMethodTracingSampling(file.toString(),8*1024*1024,100000);started=true;state=2;
   long end=SystemClock.elapsedRealtime()+(test?1000:120000);
   while(SystemClock.elapsedRealtime()<end){if(!java.util.Arrays.equals(session,ready()))throw new IOException("Reading session changed");Thread.sleep(100);}
   Debug.stopMethodTracing();started=false;size=file.length();state=3;
  }catch(Throwable e){error=e.getClass().getName();state=4;}finally{if(started)try{Debug.stopMethodTracing();}catch(Throwable ignored){}}}},"KoboSysSampler").start();
 }
 @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
  if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString(TOKEN);return true;}
  if(Binder.getCallingUid()!=0)throw new SecurityException("Root only");
  data.enforceInterface(TOKEN);if(data.dataAvail()!=0)throw new IllegalArgumentException("No arguments accepted");
  if(code==1||code==2){try{start(code==1);}catch(Exception e){throw new IllegalStateException(e);}}
  else if(code!=3)return super.onTransact(code,data,reply,flags);
  reply.writeNoException();reply.writeInt(state);reply.writeLong(size);reply.writeString(error);return true;
 }
}
