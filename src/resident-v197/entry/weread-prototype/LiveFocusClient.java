import android.os.IBinder;
import android.os.Parcel;
/** No cached ownership: every call reads WMS through its root-only adapter. */
public final class LiveFocusClient {
 private final IBinder service;
 public LiveFocusClient()throws Exception {
  service=(IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,"kobo.lab.live_focus");
  if(service==null || !service.pingBinder())throw new SecurityException("Live focus service unavailable");
 }
 public int query()throws Exception {
  Parcel request=Parcel.obtain(),reply=Parcel.obtain();try{
   request.writeInterfaceToken("kobo.lab.LiveFocus.v1");
   if(!service.transact(IBinder.FIRST_CALL_TRANSACTION,request,reply,0))throw new SecurityException("Focus query rejected");
   reply.readException();int kind=reply.readInt();if(kind<0 || kind>4 || reply.dataAvail()!=0)throw new SecurityException("Invalid live focus reply");return kind;
  }finally{request.recycle();reply.recycle();}
 }
 public static final class Snapshot {
  public final int kind;public final long generation;
  Snapshot(int k,long g){kind=k;generation=g;}
 }
 public Snapshot snapshot()throws Exception {
  Parcel request=Parcel.obtain(),reply=Parcel.obtain();try{
   request.writeInterfaceToken("kobo.lab.LiveFocus.v1");
   if(!service.transact(IBinder.FIRST_CALL_TRANSACTION+1,request,reply,0))throw new SecurityException("Focus lease unavailable");
   reply.readException();int kind=reply.readInt();long generation=reply.readLong();
   if(kind<0||kind>4||generation<=0||reply.dataAvail()!=0)throw new SecurityException("Invalid focus lease");
   return new Snapshot(kind,generation);
  }finally{request.recycle();reply.recycle();}
 }

}
