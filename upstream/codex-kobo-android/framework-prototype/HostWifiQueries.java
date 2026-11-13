import android.content.Context;
import android.os.*;
import java.lang.reflect.*;
import java.util.*;
/** Narrow read-only Wi-Fi metadata endpoint. The runtime has no supplicant
 * bridge, so connection/link-speed/DHCP metadata are explicitly unavailable.
 * Does not expose identifiers or accept Wi-Fi controls. Real connectivity
 * remains the separately observed host default route.
 */
public final class HostWifiQueries {
 public static void install(final Context context)throws Exception {
  if(!HostDefaultConnectivityService.enabled())throw new IllegalStateException("Host observer required");
  final Map<Integer,String> methods=new HashMap<Integer,String>();
  for(Field f:Class.forName("android.net.wifi.IWifiManager$Stub").getDeclaredFields())if(f.getName().startsWith("TRANSACTION_")){f.setAccessible(true);methods.put(f.getInt(null),f.getName().substring(12));}
  Class<?> services=Class.forName("android.os.ServiceManager");if(services.getMethod("checkService",String.class).invoke(null,"wifi")!=null)throw new IllegalStateException("Wi-Fi service already published");
  Binder b=new Binder(){@Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
   if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString("android.net.wifi.IWifiManager");return true;}
   String method=methods.get(code);if(method==null)return super.onTransact(code,data,reply,flags);
   data.enforceInterface("android.net.wifi.IWifiManager");
   if(data.dataSize()>1024)throw new IllegalArgumentException("Oversized Wi-Fi query data");
   context.enforceCallingOrSelfPermission("android.permission.ACCESS_WIFI_STATE","Unavailable host supplicant metadata");
   if(method.equals("requestActivityInfo")){
    if(data.dataAvail()<4)throw new IllegalArgumentException("Missing activity receiver marker");
    int present=data.readInt();if(present!=0 && present!=1)throw new IllegalArgumentException("Invalid activity receiver marker");
    ResultReceiver receiver=present==1?ResultReceiver.CREATOR.createFromParcel(data):null;
    if(data.dataAvail()!=0)throw new IllegalArgumentException("Unexpected activity query arguments");
    // There is no controller telemetry bridge. Reply promptly with no record,
    // rather than inventing zero-valued energy or leaving BatteryStats waiting.
    if(receiver!=null)receiver.send(0,new Bundle());
    System.out.println("ANDROID_HOST_WIFI_ACTIVITY_UNAVAILABLE callback="+(receiver!=null)+" uid="+Binder.getCallingUid());
    return true; // oneway: reply is null for a remote Binder call.
   }
   if(data.dataAvail()!=0)throw new IllegalArgumentException("Unexpected Wi-Fi query data");
   if(method.equals("reportActivityInfo")){reply.writeNoException();reply.writeInt(0);return true;}
   if(method.equals("getSupportedFeatures")){reply.writeNoException();reply.writeInt(0);return true;}
   if(method.equals("getConnectionInfo")||method.equals("getDhcpInfo")){reply.writeNoException();reply.writeInt(0);System.out.println("ANDROID_HOST_WIFI_METADATA_UNAVAILABLE query="+method+" uid="+Binder.getCallingUid());return true;}
   if(method.equals("getWifiEnabledState")){reply.writeNoException();reply.writeInt(HostDefaultConnectivityService.hasObservedLink()?android.net.wifi.WifiManager.WIFI_STATE_ENABLED:android.net.wifi.WifiManager.WIFI_STATE_UNKNOWN);return true;}
   throw new UnsupportedOperationException("No host Wi-Fi control or supplicant bridge: "+method);
  }};
  b.attachInterface(null,"android.net.wifi.IWifiManager");services.getMethod("addService",String.class,IBinder.class).invoke(null,"wifi",b);
  System.out.println("ANDROID_HOST_WIFI_READ_ONLY_QUERY_READY connection_metadata=unavailable");
  verifyActivityQueries();
 }
 public static void verifyActivityQueries()throws Exception {
  Class<?> services=Class.forName("android.os.ServiceManager");
  IBinder binder=(IBinder)services.getMethod("getService",String.class).invoke(null,"wifi");
  Object wifi=Class.forName("android.net.wifi.IWifiManager$Stub").getMethod("asInterface",IBinder.class).invoke(null,binder);
  final java.util.concurrent.CountDownLatch received=new java.util.concurrent.CountDownLatch(1);
  final java.util.concurrent.atomic.AtomicBoolean valid=new java.util.concurrent.atomic.AtomicBoolean(false);
  ResultReceiver receiver=new ResultReceiver(null){@Override protected void onReceiveResult(int code,Bundle data){valid.set(code==0 && data!=null && data.isEmpty());received.countDown();}};
  Class<?> api=Class.forName("android.net.wifi.IWifiManager");
  api.getMethod("requestActivityInfo",ResultReceiver.class).invoke(wifi,receiver);
  if(!received.await(5,java.util.concurrent.TimeUnit.SECONDS) || !valid.get())throw new AssertionError("Unavailable activity callback did not complete");
  api.getMethod("requestActivityInfo",ResultReceiver.class).invoke(wifi,(Object)null);
  if(api.getMethod("reportActivityInfo").invoke(wifi)!=null || !Integer.valueOf(0).equals(api.getMethod("getSupportedFeatures").invoke(wifi)))throw new AssertionError("Unexpected controller telemetry capability");
  System.out.println("ANDROID_HOST_WIFI_ACTIVITY_QUERY_CLIENT_OK uid="+android.os.Process.myUid()+" metadata=unavailable callback=true null_receiver=true");
 }

}
