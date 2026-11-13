import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.system.Os;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.MotionEvent;
import java.io.*;
import java.lang.reflect.Method;
import java.security.MessageDigest;
/** One human gesture to the real, verified WeRead window; creates no overlay. */
public final class WeReadPageTouchProbe {
 private static LocalSocket binding;private static LocalServerSocket server;
 private static String focus()throws Exception {
  Method method=WeReadInputProbe.class.getDeclaredMethod("focus",String.class);method.setAccessible(true);
  return (String)method.invoke(null,"/lab/weread-page-touch-window.txt");
 }
 public static void main(String[] args)throws Exception {
  if(args.length!=0 || android.os.Process.myUid()!=0)throw new SecurityException("Fixed root page input probe only");
  Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  PackageInfo info=context.getPackageManager().getPackageInfo("com.tencent.weread.eink",PackageManager.GET_SIGNATURES);
  if(info.applicationInfo.uid!=10007 || info.signatures==null || info.signatures.length!=1)throw new SecurityException("Unexpected installed app identity");
  StringBuilder cert=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(info.signatures[0].toByteArray()))cert.append(String.format(java.util.Locale.US,"%02x",b&255));
  if(!"e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c".equals(cert.toString()))throw new SecurityException("Unexpected installed signer");
  String initial=focus();
  if(!initial.contains(" com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity}"))throw new SecurityException("Login window is not focused: "+initial);
  String path="/dev/socket/kobo_touch_probe";if(new File(path).exists())throw new SecurityException("Existing input endpoint");
  binding=new LocalSocket();binding.bind(new LocalSocketAddress(path,LocalSocketAddress.Namespace.FILESYSTEM));Os.chmod(path,0600);server=new LocalServerSocket(binding.getFileDescriptor());
  Class<?> type=Class.forName("android.hardware.input.InputManager");final Object input=type.getMethod("getInstance").invoke(null);final Method inject=type.getMethod("injectInputEvent",InputEvent.class,int.class);
  System.out.println("ANDROID_TOUCH_PROBE_WINDOW_READY target=verified-weread uid=10007");
  new Thread(new Runnable(){public void run(){boolean down=false;long downTime=0;int lastX=0,lastY=0;try{
   try(LocalSocket client=server.accept()){
    if(client.getPeerCredentials().getUid()!=0)throw new SecurityException("Host root required");client.setSoTimeout(150000);
    DataInputStream in=new DataInputStream(client.getInputStream());DataOutputStream out=new DataOutputStream(client.getOutputStream());int sequence=0;
    while(true){
     int magic;try{magic=in.readInt();}catch(EOFException closed){break;}
     int action=in.readInt(),x=in.readInt(),y=in.readInt(),serial=in.readInt();
     if(magic!=0x4b545031 || serial!=++sequence || sequence>4096 || x<0 || x>=720 || y<0 || y>=960 || action<0 || action>3 || (action==0?down:!down))throw new SecurityException("Invalid host gesture state");
     if(action==0){String current=focus();if(!current.contains(" com.tencent.weread.eink/"))throw new SecurityException("WeRead lost focus");System.out.println("WEREAD_PHYSICAL_TOUCH_GESTURE_WINDOW x="+x+" y="+y+" tool=FINGER "+current);downTime=SystemClock.uptimeMillis();}
     MotionEvent event=TouchEvents.create(downTime,SystemClock.uptimeMillis(),action,x,y);
     boolean accepted;try{accepted=Boolean.TRUE.equals(inject.invoke(input,event,2));}finally{event.recycle();}
     if(!accepted)throw new AssertionError("Physical app input rejected");down=action==0 || action==2;lastX=x;lastY=y;
     if(action==1){try(PrintWriter record=new PrintWriter("/lab/touch-probe-result.txt")){record.println("ANDROID_WEREAD_PHYSICAL_GESTURE_DISPATCHED");}System.out.println("ANDROID_WEREAD_PHYSICAL_GESTURE_DISPATCHED");}
     out.writeInt(1);out.flush();if(action==1)break;
    }
   }
  }catch(Throwable error){error.printStackTrace();}finally{
   if(down){try{MotionEvent event=TouchEvents.create(downTime,SystemClock.uptimeMillis(),MotionEvent.ACTION_CANCEL,lastX,lastY);try{inject.invoke(input,event,2);}finally{event.recycle();}}catch(Throwable ignored){}}
  }}},"weread-physical-input").start();
  new Handler().postDelayed(new Runnable(){public void run(){System.exit(92);}},180000);Looper.loop();
 }
}
