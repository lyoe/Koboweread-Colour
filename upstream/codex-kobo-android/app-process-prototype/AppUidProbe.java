import android.os.Process;
import java.io.*;
/** Ordinary UID ART test; does not install a package or create an Activity. */
public final class AppUidProbe {
 public static void main(String[] args)throws Exception {
  if(args.length==1 && "server".equals(args[0])){
   if(Process.myUid()!=0)throw new AssertionError("Setup requires root");
   android.os.Looper.prepareMainLooper();
   Class<?> props=Class.forName("android.os.SystemProperties");
   java.lang.reflect.Method set=props.getMethod("set",String.class,String.class);
   set.invoke(null,"dalvik.vm.heapsize","64m");set.invoke(null,"dalvik.vm.heapgrowthlimit","48m");
   android.os.Binder service=new android.os.Binder(){
    @Override protected boolean onTransact(int code,android.os.Parcel data,android.os.Parcel reply,int flags)throws android.os.RemoteException{
     if(code!=1)return super.onTransact(code,data,reply,flags);
     data.enforceInterface("kobo.lab.IUidProbe");int uid=android.os.Binder.getCallingUid();
     if(uid!=10000)throw new SecurityException("Unexpected UID");
     reply.writeNoException();reply.writeInt(uid);return true;
    }
   };
   Class.forName("android.os.ServiceManager").getMethod("addService",String.class,android.os.IBinder.class).invoke(null,"kobo.uid.test",service);
   System.out.println("ANDROID_UID_TEST_SERVICE_READY");System.out.flush();android.os.Looper.loop();return;
  }
  if(Process.myUid()!=10000)throw new AssertionError("Wrong application UID");
  StringBuilder status=new StringBuilder();
  try(BufferedReader reader=new BufferedReader(new FileReader("/proc/self/status"))){
   for(String line;(line=reader.readLine())!=null;){
    if(line.startsWith("Uid:")||line.startsWith("Gid:")||line.startsWith("Groups:")||line.startsWith("CapEff:")||line.startsWith("NoNewPrivs:")){System.out.println(line);status.append(line).append('\n');}
    if(line.startsWith("CapEff:")&&!line.trim().endsWith("0000000000000000"))throw new AssertionError("Unexpected capabilities");
   }
  }
  long maximum=Runtime.getRuntime().maxMemory();System.out.println("ANDROID_APP_HEAP_MAX_BYTES "+maximum);
  if(maximum!=48L*1024*1024)throw new AssertionError("Heap property configuration not applied");
  android.os.IBinder remote=(android.os.IBinder)Class.forName("android.os.ServiceManager").getMethod("getService",String.class).invoke(null,"kobo.uid.test");
  if(remote==null)throw new AssertionError("UID test service missing");
  android.os.Parcel data=android.os.Parcel.obtain(),reply=android.os.Parcel.obtain();
  try{data.writeInterfaceToken("kobo.lab.IUidProbe");if(!remote.transact(1,data,reply,0))throw new AssertionError("UID Binder transaction failed");reply.readException();if(reply.readInt()!=10000)throw new AssertionError("Binder caller UID mismatch");}
  finally{data.recycle();reply.recycle();}
  System.out.println("ANDROID_ORDINARY_UID_BINDER_ROUNDTRIP_OK");
  File directory=new File("/data/local/tmp/kobo-uid-probe");
  File file=new File(directory,"probe.txt");
  try(FileOutputStream out=new FileOutputStream(file)){out.write("ANDROID_UID_PRIVATE_IO_OK\n".getBytes("UTF-8"));}
  try(BufferedReader in=new BufferedReader(new FileReader(file))){if(!"ANDROID_UID_PRIVATE_IO_OK".equals(in.readLine()))throw new AssertionError("Private readback");}
  if(!file.delete())throw new AssertionError("Private cleanup");
  boolean denied=false;
  try(FileOutputStream out=new FileOutputStream("/lab/uid-write-must-fail")){out.write(1);}catch(IOException expected){denied=true;}
  if(!denied)throw new AssertionError("Application UID wrote root-owned lab directory");
  for(int round=0;round<8;round++){
   byte[][] objects=new byte[8][];
   for(int i=0;i<objects.length;i++){objects[i]=new byte[2*1024*1024];objects[i][0]=(byte)(round+i);}
   for(int i=0;i<objects.length;i++)if(objects[i][0]!=(byte)(round+i))throw new AssertionError("Heap data mismatch");
   objects=null;System.gc();
  }
  System.out.println("ANDROID_ORDINARY_UID_ART_PRIVATE_IO_GC_OK uid="+Process.myUid());
 }
}
