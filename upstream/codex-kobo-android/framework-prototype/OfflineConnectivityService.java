import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.NetworkCapabilities;
import android.os.*;
import java.lang.reflect.*;
import java.util.*;
/** Explicit offline query endpoint while this lab has no Android network agents.
 * Does not configure the host network, publish a connected network, or accept mutations. */
public final class OfflineConnectivityService {
 private static final Map<Integer,Registration> registrations=new HashMap<Integer,Registration>();
 private static int nextId=1;
 private static int releasedCode;
 private static final class Registration implements IBinder.DeathRecipient {
  final int id,uid;final NetworkRequest request;final IBinder token;final Messenger messenger;
  Registration(int id,int uid,NetworkRequest request,IBinder token,Messenger messenger){this.id=id;this.uid=uid;this.request=request;this.token=token;this.messenger=messenger;}
  public void binderDied(){synchronized(registrations){if(registrations.get(id)==this)registrations.remove(id);}System.out.println("ANDROID_OFFLINE_DEFAULT_LISTENER_DIED id="+id+" uid="+uid);}
 }
 private static void listener(String method,Parcel data,Parcel reply)throws RemoteException {
  int uid=Binder.getCallingUid();
  if(method.equals("releaseNetworkRequest")){
   NetworkRequest request=data.readInt()!=0?NetworkRequest.CREATOR.createFromParcel(data):null;
   if(request==null||data.dataAvail()!=0)throw new IllegalArgumentException("Invalid release request");
   int id;try{id=NetworkRequest.class.getField("requestId").getInt(request);}catch(Exception e){throw new IllegalStateException(e);}
   Registration record;
   synchronized(registrations){record=registrations.get(id);if(record!=null){if(record.uid!=uid&&uid!=0&&uid!=1000)throw new SecurityException("Listener belongs to another UID");if(!record.request.equals(request))throw new IllegalArgumentException("Listener token mismatch");registrations.remove(id);}}
   if(record!=null){
    record.token.unlinkToDeath(record,0);
    Bundle bundle=new Bundle();bundle.putParcelable("NetworkRequest",record.request);Message message=Message.obtain();message.what=releasedCode;message.setData(bundle);
    try{record.messenger.send(message);}catch(RemoteException dead){}
    System.out.println("ANDROID_OFFLINE_DEFAULT_LISTENER_RELEASED id="+id+" uid="+uid);
   }
   reply.writeNoException();return;
  }
  NetworkCapabilities requested=data.readInt()!=0?NetworkCapabilities.CREATOR.createFromParcel(data):null;
  Messenger messenger=data.readInt()!=0?Messenger.CREATOR.createFromParcel(data):null;
  boolean passive=method.equals("listenForNetwork");
  int timeout=passive?0:data.readInt();IBinder token=data.readStrongBinder();int legacy=passive?-1:data.readInt();
  if((!passive&&requested!=null)||timeout!=0||legacy!=-1)throw new UnsupportedOperationException("Only offline network observation is supported");
  if(passive&&requested==null)throw new IllegalArgumentException("Missing listener capabilities");
  if(token==null||messenger==null||data.dataAvail()!=0)throw new IllegalArgumentException("Invalid default listener");
  NetworkRequest request;
  synchronized(registrations){
   int count=0;for(Registration record:registrations.values())if(record.uid==uid)count++;
   if(count>=100||registrations.size()>=128||nextId<=0)throw new IllegalStateException("Offline listener limit");
   int id=nextId++;
   try{NetworkRequest template=new NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build();NetworkCapabilities capabilities=passive?requested:(NetworkCapabilities)NetworkRequest.class.getField("networkCapabilities").get(template);request=NetworkRequest.class.getConstructor(NetworkCapabilities.class,int.class,int.class).newInstance(capabilities,-1,id);}catch(Exception e){throw new IllegalStateException(e);}
   Registration record=new Registration(id,uid,request,token,messenger);token.linkToDeath(record,0);registrations.put(id,record);
   System.out.println("ANDROID_OFFLINE_NETWORK_LISTENER_REGISTERED type="+(passive?"passive":"default")+" id="+id+" uid="+uid+" networks=0");
  }
  reply.writeNoException();reply.writeInt(1);request.writeToParcel(reply,0);
 }
 public static void install(final Context context)throws Exception {
  Class<?> properties=Class.forName("android.os.SystemProperties");
  if(!"true".equals(properties.getMethod("get",String.class).invoke(null,"config.disable_network")))throw new IllegalStateException("Offline lab mode required");
  Class<?> services=Class.forName("android.os.ServiceManager");
  if(services.getMethod("checkService",String.class).invoke(null,"connectivity")!=null)throw new IllegalStateException("Connectivity already published");
  Field released=ConnectivityManager.class.getDeclaredField("CALLBACK_RELEASED");released.setAccessible(true);releasedCode=released.getInt(null);
  final Map<Integer,String> methods=new HashMap<Integer,String>();
  for(Field field:Class.forName("android.net.IConnectivityManager$Stub").getDeclaredFields())if(field.getName().startsWith("TRANSACTION_")){
   field.setAccessible(true);methods.put(field.getInt(null),field.getName().substring(12));
  }
  Binder boundary=new Binder(){
   @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
    if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString("android.net.IConnectivityManager");return true;}
    String method=methods.get(code);if(method==null)return super.onTransact(code,data,reply,flags);
    data.enforceInterface("android.net.IConnectivityManager");
    if(data.dataSize()>8192)throw new IllegalArgumentException("Oversized offline query");
    // ActivityThread asks for a default proxy before any Application runs.
    // The original null-network/global proxy path has no ACCESS_NETWORK_STATE gate.
    if(!method.equals("getProxyForNetwork")&&!method.equals("getGlobalProxy")&&!method.equals("releaseNetworkRequest"))context.enforceCallingOrSelfPermission("android.permission.ACCESS_NETWORK_STATE","Offline connectivity queries");
    if(method.equals("requestNetwork")||method.equals("listenForNetwork")||method.equals("releaseNetworkRequest")){listener(method,data,reply);return true;}
    boolean array=false,booleanValue=false,bool=false;
    if(method.equals("getNetworkInfo")||method.equals("getNetworkForType")||method.equals("getLinkPropertiesForType")||method.equals("isNetworkSupported")){
     data.readInt();if(method.equals("isNetworkSupported")){bool=true;booleanValue=false;}
    }else if(method.equals("getLinkProperties")||method.equals("getNetworkCapabilities")||method.equals("getProxyForNetwork")){
     if(data.readInt()!=0)Network.CREATOR.createFromParcel(data);
    }else if(method.equals("getAllNetworks")||method.equals("getAllNetworkInfo")){array=true;}
    else if(method.equals("isActiveNetworkMetered")){bool=true;booleanValue=true;}
    else if(!method.equals("getActiveNetwork")&&!method.equals("getActiveNetworkInfo")&&!method.equals("getActiveLinkProperties")&&!method.equals("getGlobalProxy")){
     System.out.println("ANDROID_OFFLINE_CONNECTIVITY_UNSUPPORTED method="+method+" caller="+Binder.getCallingUid());
     throw new UnsupportedOperationException("Offline lab connectivity: "+method);
    }
    if(data.dataAvail()!=0)throw new IllegalArgumentException("Unexpected query arguments");
    System.out.println("ANDROID_OFFLINE_CONNECTIVITY_QUERY method="+method+" caller="+Binder.getCallingUid()+" networks=0");
    reply.writeNoException();
    // Nullable parcelable absence and an empty typed array both encode integer 0.
    reply.writeInt(bool?(booleanValue?1:0):0);return true;
   }
  };
  boundary.attachInterface(null,"android.net.IConnectivityManager");
  services.getMethod("addService",String.class,IBinder.class).invoke(null,"connectivity",boundary);
  System.out.println("ANDROID_OFFLINE_CONNECTIVITY_QUERY_SERVICE_READY networks=0 mutations=unsupported");
 }
 public static void verifyQueries(Context context){
  ConnectivityManager manager=(ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
  if(manager==null||manager.getActiveNetworkInfo()!=null||manager.getAllNetworks().length!=0||!manager.isActiveNetworkMetered())throw new AssertionError("Offline query contract failed");
  ConnectivityManager.NetworkCallback callback=new ConnectivityManager.NetworkCallback(){@Override public void onAvailable(Network network){throw new AssertionError("Unexpected connected network");}};
  manager.registerDefaultNetworkCallback(callback);manager.unregisterNetworkCallback(callback);
  ConnectivityManager.NetworkCallback passive=new ConnectivityManager.NetworkCallback(){@Override public void onAvailable(Network network){throw new AssertionError("Unexpected passive connected network");}};
  manager.registerNetworkCallback(new NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build(),passive);manager.unregisterNetworkCallback(passive);
  System.out.println("ANDROID_OFFLINE_CONNECTIVITY_CLIENT_QUERIES_AND_BOTH_LISTENERS_OK networks=0");
 }
}
