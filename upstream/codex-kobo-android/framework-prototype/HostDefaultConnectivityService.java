import android.content.Context;
import android.net.*;
import android.os.*;
import android.system.Os;
import android.system.StructStat;
import java.io.*;
import java.lang.reflect.*;
import java.net.InetAddress;
import java.util.*;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Read-only view of Forma's actual host default IPv4 route.
 * Network 100 is an alias, with no VPN, routing isolation, validation or allocation.
 * The native observer and fwmark adapter must be running before publishing it.
 */
public final class HostDefaultConnectivityService {
 private static final String MARKER="/lab/host-default-network-enabled";
 private static final Map<Integer,Registration> registrations=new HashMap<Integer,Registration>();
 private static final Map<String,Integer> callbackCodes=new HashMap<String,Integer>();
 private static int nextId=1;
 private static Network network;
 private static final class Snapshot {
  final NetworkInfo info;final LinkProperties links;final NetworkCapabilities caps;final String key;
  Snapshot(NetworkInfo i,LinkProperties l,NetworkCapabilities c,String k){info=i;links=l;caps=c;key=k;}
 }
 private static final class Registration implements IBinder.DeathRecipient {
  final int id,uid;final NetworkRequest request;final IBinder token;final Messenger messenger;
  String delivered;
  Registration(int id,int uid,NetworkRequest r,IBinder t,Messenger m){this.id=id;this.uid=uid;request=r;token=t;messenger=m;}
  public void binderDied(){synchronized(registrations){if(registrations.get(id)==this)registrations.remove(id);}}
 }
 public static boolean enabled()throws Exception {
  File marker=new File(MARKER);if(!marker.exists())return false;
  StructStat st=Os.lstat(MARKER),parent=Os.lstat("/lab");
  if(st.st_uid!=0||(st.st_mode&0170000)!=0100000||(st.st_mode&022)!=0||parent.st_uid!=0||(parent.st_mode&022)!=0)throw new SecurityException("Untrusted network marker");
  return true;
 }
 private static Object invoke(Object target,String method,Class<?>[] types,Object... args)throws Exception {
  return target.getClass().getMethod(method,types).invoke(target,args);
 }
 private static Snapshot read(){
  try{
   String path="/lab/network-state/host-default.properties";StructStat st=Os.lstat(path),dir=Os.lstat("/lab/network-state");
   if(st.st_uid!=1051||dir.st_uid!=1051||(st.st_mode&0170000)!=0100000||(dir.st_mode&0170000)!=0040000||((st.st_mode|dir.st_mode)&022)!=0||st.st_size>4096)throw new IOException("Untrusted observation");
   Properties p=new Properties();try(InputStream f=new FileInputStream(path)){p.load(f);}
   long age=SystemClock.uptimeMillis()-Long.parseLong(p.getProperty("observed_uptime_ms"));
   if(!"1".equals(p.getProperty("version"))||!"100".equals(p.getProperty("netid"))||age<0||age>5000||!"1".equals(p.getProperty("connected")))return null;
   String iface=p.getProperty("interface"),ip=p.getProperty("address"),gateway=p.getProperty("gateway");
   int prefix=Integer.parseInt(p.getProperty("prefix")),mtu=Integer.parseInt(p.getProperty("mtu")),count=Integer.parseInt(p.getProperty("dns_count"));
   if(!"eth0".equals(iface)||prefix<1||prefix>32||mtu<576||mtu>9000||count<0||count>4)return null;
   NetworkInfo info=NetworkInfo.class.getConstructor(int.class,int.class,String.class,String.class).newInstance(ConnectivityManager.TYPE_WIFI,0,"WIFI","");
   invoke(info,"setIsAvailable",new Class<?>[]{boolean.class},true);
   invoke(info,"setDetailedState",new Class<?>[]{NetworkInfo.DetailedState.class,String.class,String.class},NetworkInfo.DetailedState.CONNECTED,null,null);
   LinkProperties links=LinkProperties.class.getConstructor().newInstance();invoke(links,"setInterfaceName",new Class<?>[]{String.class},iface);
   Class<?> la=Class.forName("android.net.LinkAddress"),route=Class.forName("android.net.RouteInfo"),ipPrefix=Class.forName("android.net.IpPrefix");
   Object address=la.getConstructor(InetAddress.class,int.class).newInstance(InetAddress.getByName(ip),prefix);
   invoke(links,"addLinkAddress",new Class<?>[]{la},address);
   Constructor<?> rc=route.getConstructor(ipPrefix,InetAddress.class,String.class);
   Object defaultPrefix=ipPrefix.getConstructor(InetAddress.class,int.class).newInstance(InetAddress.getByName("0.0.0.0"),0);
   invoke(links,"addRoute",new Class<?>[]{route},rc.newInstance(defaultPrefix,InetAddress.getByName(gateway),iface));
   // The actual interface prefix also identifies the directly connected route.
   Object localPrefix=ipPrefix.getConstructor(InetAddress.class,int.class).newInstance(InetAddress.getByName(ip),prefix);
   invoke(links,"addRoute",new Class<?>[]{route},rc.newInstance(localPrefix,null,iface));
   for(int i=0;i<count;i++)invoke(links,"addDnsServer",new Class<?>[]{InetAddress.class},InetAddress.getByName(p.getProperty("dns"+i)));
   invoke(links,"setMtu",new Class<?>[]{int.class},mtu);
   NetworkCapabilities caps=NetworkCapabilities.class.getConstructor().newInstance();
   invoke(caps,"addTransportType",new Class<?>[]{int.class},NetworkCapabilities.TRANSPORT_WIFI);
   invoke(caps,"addCapability",new Class<?>[]{int.class},NetworkCapabilities.NET_CAPABILITY_INTERNET);
   // Default capabilities are TRUSTED, NOT_RESTRICTED, NOT_VPN. Never VALIDATED.
   return new Snapshot(info,links,caps,links.toString()+caps.toString());
  }catch(Exception e){System.out.println("HOST_NETWORK_OBSERVATION_UNAVAILABLE reason="+e.getClass().getSimpleName());return null;}
 }
 public static boolean hasObservedLink(){return read()!=null;}
 private static void notify(Registration r,String event,Snapshot s){
  Bundle b=new Bundle();b.putParcelable("NetworkRequest",r.request);
  if(!event.equals("RELEASED"))b.putParcelable("Network",network);
  if(event.equals("CAP_CHANGED"))b.putParcelable("NetworkCapabilities",s.caps);
  if(event.equals("IP_CHANGED"))b.putParcelable("LinkProperties",s.links);
  Message m=Message.obtain();m.what=callbackCodes.get(event);m.setData(b);
  try{r.messenger.send(m);}catch(RemoteException dead){}
 }
 private static void deliver(Registration r,Snapshot s){
  boolean match=false;
  if(s!=null)try{
   NetworkCapabilities requested=(NetworkCapabilities)NetworkRequest.class.getField("networkCapabilities").get(r.request);
   match=(Boolean)invoke(requested,"satisfiedByNetworkCapabilities",new Class<?>[]{NetworkCapabilities.class},s.caps);
  }catch(Exception e){throw new IllegalStateException(e);}
  String key=match?s.key:null;
  if(Objects.equals(r.delivered,key))return;
  if(r.delivered!=null)notify(r,"LOST",null);
  r.delivered=key;
  if(key!=null){notify(r,"AVAILABLE",s);notify(r,"CAP_CHANGED",s);notify(r,"IP_CHANGED",s);}
 }
 private static void listener(String method,Parcel data,Parcel reply)throws RemoteException {
  int uid=Binder.getCallingUid();
  synchronized(registrations){
   if(method.equals("releaseNetworkRequest")){
    NetworkRequest request=data.readInt()!=0?NetworkRequest.CREATOR.createFromParcel(data):null;
    if(request==null||data.dataAvail()!=0)throw new IllegalArgumentException("Invalid release");
    int id;try{id=NetworkRequest.class.getField("requestId").getInt(request);}catch(Exception e){throw new IllegalStateException(e);}
    Registration r=registrations.get(id);
    if(r!=null){if(r.uid!=uid&&uid!=0&&uid!=1000)throw new SecurityException("Listener owner");if(!r.request.equals(request))throw new IllegalArgumentException("Listener mismatch");registrations.remove(id);r.token.unlinkToDeath(r,0);notify(r,"RELEASED",null);}
    reply.writeNoException();return;
   }
   NetworkCapabilities requested=data.readInt()!=0?NetworkCapabilities.CREATOR.createFromParcel(data):null;
   Messenger messenger=data.readInt()!=0?Messenger.CREATOR.createFromParcel(data):null;
   boolean passive=method.equals("listenForNetwork");int timeout=passive?0:data.readInt();IBinder token=data.readStrongBinder();int legacy=passive?-1:data.readInt();
   if((!passive&&requested!=null)||timeout!=0||legacy!=-1)throw new UnsupportedOperationException("Only existing default network observation");
   if((passive&&requested==null)||token==null||messenger==null||data.dataAvail()!=0)throw new IllegalArgumentException("Invalid observer");
   int count=0;for(Registration r:registrations.values())if(r.uid==uid)count++;
   if(count>=100||registrations.size()>=128||nextId<=0)throw new IllegalStateException("Listener limit");
   try{
    if(!passive){NetworkRequest template=new NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build();requested=(NetworkCapabilities)NetworkRequest.class.getField("networkCapabilities").get(template);}
    int id=nextId++;NetworkRequest request=NetworkRequest.class.getConstructor(NetworkCapabilities.class,int.class,int.class).newInstance(requested,-1,id);
    Registration r=new Registration(id,uid,request,token,messenger);token.linkToDeath(r,0);registrations.put(id,r);deliver(r,read());
    reply.writeNoException();reply.writeInt(1);request.writeToParcel(reply,0);
   }catch(RemoteException e){throw e;}catch(Exception e){throw new IllegalStateException(e);}
  }
 }
 public static void install(final Context context)throws Exception {
  if(!enabled())throw new IllegalStateException("Explicit host observer required");
  network=Network.class.getConstructor(int.class).newInstance(100);
  for(String name:new String[]{"AVAILABLE","CAP_CHANGED","IP_CHANGED","LOST","RELEASED"}){Field f=ConnectivityManager.class.getDeclaredField("CALLBACK_"+name);f.setAccessible(true);callbackCodes.put(name,f.getInt(null));}
  final Map<Integer,String> methods=new HashMap<Integer,String>();
  for(Field f:Class.forName("android.net.IConnectivityManager$Stub").getDeclaredFields())if(f.getName().startsWith("TRANSACTION_")){f.setAccessible(true);methods.put(f.getInt(null),f.getName().substring(12));}
  Class<?> services=Class.forName("android.os.ServiceManager");if(services.getMethod("checkService",String.class).invoke(null,"connectivity")!=null)throw new IllegalStateException("Connectivity already published");
  Binder boundary=new Binder(){@Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
   if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString("android.net.IConnectivityManager");return true;}
   String method=methods.get(code);if(method==null)return super.onTransact(code,data,reply,flags);
   data.enforceInterface("android.net.IConnectivityManager");if(data.dataSize()>8192)throw new IllegalArgumentException("Oversized query");
   if(!method.equals("getProxyForNetwork")&&!method.equals("getGlobalProxy")&&!method.equals("releaseNetworkRequest"))context.enforceCallingOrSelfPermission("android.permission.ACCESS_NETWORK_STATE","Host default network observation");
   if(method.equals("requestNetwork")||method.equals("listenForNetwork")||method.equals("releaseNetworkRequest")){listener(method,data,reply);return true;}
   Snapshot s=read();Parcelable result=null;Boolean bool=null;
   boolean type=true,selected=true;
   if(method.equals("getNetworkInfo")||method.equals("getNetworkForType")||method.equals("getLinkPropertiesForType")||method.equals("isNetworkSupported"))type=data.readInt()==ConnectivityManager.TYPE_WIFI;
   else if(method.equals("getLinkProperties")||method.equals("getNetworkCapabilities")||method.equals("getProxyForNetwork")||method.equals("getNetworkInfoForUid")){Network n=data.readInt()!=0?Network.CREATOR.createFromParcel(data):null;selected=network.equals(n);}
   if(method.equals("getActiveNetworkForUid")||method.equals("getActiveNetworkInfoForUid")||method.equals("getNetworkInfoForUid")){
    int uid=data.readInt(),ignoreBlocked=data.readInt(),caller=Binder.getCallingUid();
    if(uid!=caller&&caller!=0&&caller!=1000)throw new SecurityException("Cross-UID network query");
    if(ignoreBlocked!=0&&caller!=0&&caller!=1000)throw new SecurityException("Blocked-state override");
   }
   if(data.dataAvail()!=0)throw new UnsupportedOperationException("Unsupported query arguments: "+method);
   // Keep concrete array types separate: API24 ART cannot verify the merged
   // Object[] type emitted by D8 for a common Parcelable[] temporary.
   if(method.equals("getAllNetworks")){reply.writeNoException();reply.writeTypedArray(s==null?new Network[0]:new Network[]{network},0);return true;}
   if(method.equals("getAllNetworkInfo")){reply.writeNoException();reply.writeTypedArray(s==null?new NetworkInfo[0]:new NetworkInfo[]{s.info},0);return true;}
   if(method.equals("getActiveNetwork")||method.equals("getActiveNetworkForUid")||method.equals("getNetworkForType"))result=s!=null&&type?network:null;
   else if(method.equals("getActiveNetworkInfo")||method.equals("getActiveNetworkInfoForUid")||method.equals("getNetworkInfo")||method.equals("getNetworkInfoForUid"))result=s!=null&&type&&selected?s.info:null;
   else if(method.equals("getActiveLinkProperties")||method.equals("getLinkPropertiesForType")||method.equals("getLinkProperties"))result=s!=null&&type&&selected?s.links:null;
   else if(method.equals("getNetworkCapabilities"))result=s!=null&&selected?s.caps:null;
   else if(method.equals("isNetworkSupported"))bool=type;
   else if(method.equals("isActiveNetworkMetered"))bool=true;
   else if(!method.equals("getProxyForNetwork")&&!method.equals("getGlobalProxy"))throw new UnsupportedOperationException("Host default network: "+method);
   reply.writeNoException();if(bool!=null)reply.writeInt(bool?1:0);else{reply.writeInt(result==null?0:1);if(result!=null)result.writeToParcel(reply,0);}return true;
  }};
  boundary.attachInterface(null,"android.net.IConnectivityManager");services.getMethod("addService",String.class,IBinder.class).invoke(null,"connectivity",boundary);
  Thread watcher=new Thread(new Runnable(){public void run(){for(;;){try{Snapshot s=read();synchronized(registrations){for(Registration r:registrations.values())deliver(r,s);}Thread.sleep(1000);}catch(InterruptedException done){return;}catch(Exception e){System.out.println("HOST_NETWORK_WATCHER_ERROR "+e.getClass().getSimpleName());}}}},"host-network-observer");watcher.setDaemon(true);watcher.start();
  System.out.println("ANDROID_HOST_DEFAULT_CONNECTIVITY_READY netid=100 validation=unknown mutations=unsupported");
 }
 public static void verifyQueries(Context context)throws Exception {
  ConnectivityManager m=(ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
  final Network expected=Network.class.getConstructor(int.class).newInstance(100);
  Network n=m.getActiveNetwork();NetworkInfo info=m.getActiveNetworkInfo();
  if(n==null||!expected.equals(n)||info==null||!info.isConnected()||m.getAllNetworks().length!=1||m.getLinkProperties(n)==null||m.getNetworkCapabilities(n)==null||m.getNetworkCapabilities(n).hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED))throw new AssertionError("Actual host network query contract");
  Network unknown=Network.class.getConstructor(int.class).newInstance(101);
  if(m.getNetworkInfo(n)==null||!m.getNetworkInfo(n).isConnected()||m.getNetworkInfo(unknown)!=null||m.getNetworkCapabilities(unknown)!=null||m.getLinkProperties(unknown)!=null||m.getNetworkInfo(ConnectivityManager.TYPE_MOBILE)!=null)throw new AssertionError("Host network selection or unknown-network rejection");
  final CountDownLatch both=new CountDownLatch(2);
  ConnectivityManager.NetworkCallback a=new ConnectivityManager.NetworkCallback(){@Override public void onAvailable(Network n){if(expected.equals(n))both.countDown();}};
  ConnectivityManager.NetworkCallback b=new ConnectivityManager.NetworkCallback(){@Override public void onAvailable(Network n){if(expected.equals(n))both.countDown();}};
  try{m.registerDefaultNetworkCallback(a);m.registerNetworkCallback(new NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build(),b);if(!both.await(5,TimeUnit.SECONDS))throw new AssertionError("Host callbacks timeout");}finally{m.unregisterNetworkCallback(a);m.unregisterNetworkCallback(b);}
  System.out.println("ANDROID_HOST_DEFAULT_QUERY_AND_BOTH_CALLBACKS_OK netid=100 validated=false");
 }
}
