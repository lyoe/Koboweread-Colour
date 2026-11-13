import android.os.*;
public final class SystemSamplingClient {
 public static void main(String[]args){try{
 run(args);System.exit(0);
 }catch(Throwable e){System.err.println("SYSTEM96_CLIENT_FAILED "+e.getClass().getName()+": "+e.getMessage());System.exit(1);}
 }
 private static void run(String[]args)throws Exception {
  if(android.os.Process.myUid()!=0||args.length!=1)throw new SecurityException("Root fixed client");
  int code=args[0].equals("selftest")?1:args[0].equals("capture")?2:args[0].equals("status")?3:0;if(code==0)throw new IllegalArgumentException();
  IBinder service=(IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,"kobo.system_sampling212");if(service==null)throw new IllegalStateException("Service unavailable");
  Parcel in=Parcel.obtain(),out=Parcel.obtain();try{
   in.writeInterfaceToken("kobo.lab.SystemSampling.v96");if(!service.transact(code,in,out,0))throw new IllegalStateException("Rejected");out.readException();
   System.out.println("SYSTEM96_STATE state="+out.readInt()+" bytes="+out.readLong()+" error="+out.readString());
  }finally{in.recycle();out.recycle();}
 }
}
