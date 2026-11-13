import java.net.*;
import java.io.*;
import javax.net.ssl.*;
/** No login, cookies, account or user files. Uses the unmodified platform TLS verifier. */
public final class NetworkProbe {
 public static void main(String[] args)throws Exception {
  if(args.length!=0||android.os.Process.myUid()!=10000)throw new SecurityException("Fixed unprivileged probe only");
  System.out.println("ANDROID_NETWORK_PROBE_IDENTITY uid="+android.os.Process.myUid());
  android.net.Network hostNetwork=null;
  if(new File("/lab/host-default-network-enabled").isFile())hostNetwork=android.net.Network.class.getConstructor(int.class).newInstance(100);
  InetAddress[] addresses=hostNetwork==null?InetAddress.getAllByName("weread.qq.com"):hostNetwork.getAllByName("weread.qq.com");
  if(addresses.length==0)throw new AssertionError("No DNS answers");
  System.out.println("ANDROID_REAL_DNS_OK host=weread.qq.com answers="+addresses.length);
  try{InetAddress.getAllByName("kobo-network-probe.invalid");throw new AssertionError("Unexpected invalid-name success");}catch(UnknownHostException expected){System.out.println("ANDROID_DNS_INVALID_NAME_REJECTED");}
  try(Socket unsupported=new Socket()){
   try{unsupported.connect(new InetSocketAddress(InetAddress.getByName("2001:db8::1"),443),1000);throw new AssertionError("Native IPv6 unexpectedly connected");}
   catch(SocketException expected){System.out.println("ANDROID_NATIVE_IPV6_REJECTED "+expected.getMessage());}
  }
  for(String target:new String[]{"https://weread.qq.com/","https://weread-1258476243.file.myqcloud.com/app/weread_user_agreement_android.html?v=1"}){
  URL url=new URL(target);
  // The fixed private runtime has no proxy. This overload also permits the
  // native Network(100) probe before ConnectivityManager is constructed.
  HttpsURLConnection connection=(HttpsURLConnection)(hostNetwork==null?url.openConnection():hostNetwork.openConnection(url,Proxy.NO_PROXY));
  connection.setConnectTimeout(12000);connection.setReadTimeout(12000);connection.setInstanceFollowRedirects(false);connection.setRequestProperty("User-Agent","Kobo-Android-Lab-Network-Probe/1");
  try{
   int status=connection.getResponseCode();
   if(status!=200)throw new IOException("HTTP status "+status);
   if(connection.getServerCertificates().length==0)throw new AssertionError("No TLS identity");
   int bytes=0;try(InputStream input=connection.getInputStream()){byte[] buffer=new byte[4096];int n;while(bytes<16384&&(n=input.read(buffer,0,Math.min(buffer.length,16384-bytes)))>0)bytes+=n;}
   System.out.println("ANDROID_VERIFIED_HTTPS_OK host="+url.getHost()+" status="+status+" cipher="+connection.getCipherSuite()+" sampled_bytes="+bytes+" netid="+(hostNetwork==null?0:100));
  }finally{connection.disconnect();}
  }
  System.out.println("ANDROID_NETWORK_PROBE_COMPLETE");System.exit(0);
 }
}
