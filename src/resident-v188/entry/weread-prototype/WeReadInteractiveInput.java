import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.system.Os;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.MotionEvent;
import android.view.KeyEvent;
import java.io.*;
import java.lang.reflect.Method;
import java.security.MessageDigest;
/** Bounded human-only input to the verified real WeRead window; no generated clicks. */
public final class WeReadInteractiveInput {
 private static LocalSocket binding;private static LocalServerSocket server;
 /* Requests from resident-loop.sh, so that the first WeRead launch and the
  * :seg service start need no JVM of their own (each was ~1.5 s of CPU on
  * the single core, the launch on the way to the home page). A request is a
  * file moved into /lab/resident (inotify, no polling): helper-launch (the
  * persisted UID) runs InstallProbe's own verifyAndLaunch (identity cache or
  * full digest, PMS UID, process registry, startActivityAsUser) exactly as
  * FrameworkProbe launch-weread does; helper-seg starts the segmentation
  * service as SegPrestart does. The answer is NAME.result: "ok" or
  * "fail CLASS: MESSAGE". The loop falls back to the separate JVM on failure. */
 private static android.os.FileObserver requests;
 private static void answer(String name,String text){
  try{File tmp=new File("/lab/resident/"+name+".result.tmp");try(PrintWriter out=new PrintWriter(tmp)){out.println(text);}
   if(!tmp.renameTo(new File("/lab/resident/"+name+".result")))System.out.println("WEREAD_HELPER_REQUEST_ANSWER_FAILED "+name);}catch(Throwable error){error.printStackTrace();}
 }
 private static void startHome() throws Exception {
  Intent home=new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME).setClassName("com.kobo.lab.smoke","com.kobo.lab.smoke.HomeActivity").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
  // Same call as the verified WeRead launch: no caller application record.
  Object manager=Class.forName("android.app.ActivityManagerNative").getMethod("getDefault").invoke(null);
  Method start=null;for(Method candidate:Class.forName("android.app.IActivityManager").getMethods())if(candidate.getName().equals("startActivityAsUser") && candidate.getParameterTypes().length==11){start=candidate;break;}
  if(start==null)throw new NoSuchMethodException("startActivityAsUser");
  Object code=start.invoke(manager,null,"android",home,null,null,null,0,0,null,null,0);
  System.out.println("WEREAD_RESIDENT_HOME_STARTED code="+code);
 }
 private static void handleRequest(Context context,String name){
  File request=new File("/lab/resident/"+name);String text=null;
  try{try(BufferedReader in=new BufferedReader(new FileReader(request))){text=in.readLine();}}catch(IOException error){}
  request.delete();
  try{
   if("helper-launch".equals(name)){
    int uid=Integer.parseInt(text==null?"":text.trim());
    Class<?> probe=Class.forName("InstallProbe");
    java.lang.reflect.Field pkg=probe.getDeclaredField("packageName");pkg.setAccessible(true);pkg.set(null,"com.tencent.weread.eink");
    java.lang.reflect.Field src=probe.getDeclaredField("sourcePath");src.setAccessible(true);src.set(null,"/lab/weread.apk");
    Method launch=probe.getDeclaredMethod("verifyAndLaunch",Context.class,int.class);launch.setAccessible(true);
    long begin=SystemClock.uptimeMillis();launch.invoke(null,context,uid);
    System.out.println("WEREAD_HELPER_LAUNCH_DONE ms="+(SystemClock.uptimeMillis()-begin));
   }else if("helper-home".equals(name)){
    // 阅读优化 from the KOReader menu (2026-11-11): the launcher on top; it
    // opens its settings page from the host's open-tuning marker.
    startHome();
   }else{
    Intent intent=new Intent().setComponent(new android.content.ComponentName("com.tencent.weread.eink","com.tencent.weread.reader.segment.aidl.SegmentService"));
    Object am=Class.forName("android.app.ActivityManagerNative").getMethod("getDefault").invoke(null);Method start=null;
    for(Method m:am.getClass().getMethods())if(m.getName().equals("startService") && m.getParameterTypes().length==5)start=m;
    if(start==null)throw new NoSuchMethodException("startService");
    Object started=start.invoke(am,null,intent,null,"com.android.shell",0);
    if(!(started instanceof android.content.ComponentName) || ((android.content.ComponentName)started).getPackageName().startsWith("!"))throw new IllegalStateException("startService returned "+started);
    System.out.println("WEREAD_HELPER_SEG_STARTED "+started);
   }
   answer(name,"ok");
  }catch(Throwable error){
   Throwable cause=error instanceof java.lang.reflect.InvocationTargetException && error.getCause()!=null?error.getCause():error;
   cause.printStackTrace();answer(name,"fail "+cause.getClass().getName()+": "+cause.getMessage());
  }
 }
 // A page key is injected with WAIT_FOR_FINISH on its own thread. While it is
 // unfinished (the app's UI thread is busy), further keys and new gestures are
 // dropped instead of queued: a queued key behind an unfinished one is what
 // makes InputDispatcher declare an ANR after 5 s on this slow device.
 // The same holds for a key queued behind an unfinished tap (measured: a
 // tap, then a page key while WeRead was stopped -> ANR, ActivityManager
 // kills WeRead without a dialog). So a gesture's ACTION_UP is injected with
 // WAIT_FOR_FINISH on its own thread too, and until it is finished keys and
 // new gestures are dropped. Off: /data/local/tmp/kobo-gesturewait-disabled.
 private static final java.util.concurrent.atomic.AtomicBoolean keyPending=new java.util.concurrent.atomic.AtomicBoolean();
 private static final java.util.concurrent.atomic.AtomicBoolean gesturePending=new java.util.concurrent.atomic.AtomicBoolean();
 private static final boolean gestureWait=!new java.io.File("/data/local/tmp/kobo-gesturewait-disabled").exists();
 private static String focus()throws Exception {
  Method method=WeReadInputProbe.class.getDeclaredMethod("focus",String.class);method.setAccessible(true);
  return (String)method.invoke(null,"/lab/interactive-window.txt");
 }
 private static void cancelOwned(Object input,Method inject,LiveFocusClient live,long lease,long began,int x,int y)throws Exception {
  LiveFocusClient.Snapshot current=live.snapshot();
  if(!TouchLeasePolicy.same(lease,current.kind,current.generation)){
   System.out.println("WEREAD_CANCEL_SKIPPED_WINDOW_PAUSED_OR_REPLACED");return;
  }
  MotionEvent event=TouchEvents.create(began,SystemClock.uptimeMillis(),MotionEvent.ACTION_CANCEL,x,y);
  try{inject.invoke(input,event,0);}finally{event.recycle();}
 }
 /** Resident: rotate the display upright (0) or upside down (2), as a locked
  * user rotation; true once the window manager reports it. Never throws: an
  * uncaught exception here would end the resident session. */
 private static boolean rotate(int target){
  try{
   IBinder binder=(IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,"window");
   Object wm=Class.forName("android.view.IWindowManager$Stub").getMethod("asInterface",IBinder.class).invoke(null,binder);
   Method rotation=wm.getClass().getMethod("getRotation");long began=SystemClock.uptimeMillis();
   wm.getClass().getMethod("freezeRotation",int.class).invoke(wm,target);
   int now=(Integer)rotation.invoke(wm);
   while(now!=target && SystemClock.uptimeMillis()-began<2000){Thread.sleep(20);now=(Integer)rotation.invoke(wm);}
   System.out.println("WEREAD_RESIDENT_ROTATION target="+target+" rotation="+now+" ms="+(SystemClock.uptimeMillis()-began));
   return now==target;
  }catch(Throwable error){System.out.println("WEREAD_RESIDENT_ROTATION_FAILED "+error);return false;}
 }
 public static void main(String[] args)throws Exception {
  // "resident": long-lived helper of the resident launcher session. It accepts
  // one host forwarder at a time, again after each background period.
  final boolean resident=args.length==1 && "resident".equals(args[0]);
  final int sessionSeconds=resident?43200:(args.length==0?180:(args.length==1 && "1800".equals(args[0])?1800:(args.length==1 && "180".equals(args[0])?180:0)));
  if(sessionSeconds==0 || android.os.Process.myUid()!=0)throw new SecurityException("Fixed root page input probe only");
  Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  final android.graphics.Point bounds=new android.graphics.Point();
  ((android.view.WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealSize(bounds);
  final int width=bounds.x,height=bounds.y;
  if(!((width==720 && height==960)||(width==632 && height==840)||(width==1264 && height==1680)))throw new SecurityException("Unsupported runtime display");
  PackageInfo info=context.getPackageManager().getPackageInfo("com.tencent.weread.eink",PackageManager.GET_SIGNATURES);
  if(info.applicationInfo.uid!=10007 || info.signatures==null || info.signatures.length!=1)throw new SecurityException("Unexpected installed app identity");
  StringBuilder cert=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(info.signatures[0].toByteArray()))cert.append(String.format(java.util.Locale.US,"%02x",b&255));
  if(!"e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c".equals(cert.toString()))throw new SecurityException("Unexpected installed signer");
  long legacyBegin=SystemClock.uptimeMillis();String initial=focus();
  long legacyMs=SystemClock.uptimeMillis()-legacyBegin;
  final LiveFocusClient live=new LiveFocusClient();
  LiveFocusClient.Snapshot leaseReady=live.snapshot();
  System.out.println("WEREAD_FOCUS_LEASE_READY kind="+leaseReady.kind+" generation="+leaseReady.generation);
  long liveBegin=SystemClock.uptimeMillis();int liveKind=0;for(int i=0;i<20;i++)liveKind=live.query();
  System.out.println("WEREAD_FOCUS_QUERY_BENCHMARK legacy_single_ms="+legacyMs+" live_20_total_ms="+(SystemClock.uptimeMillis()-liveBegin)+" kind="+liveKind);
  if(!resident && !initial.contains(" com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity}") && !initial.contains(" com.tencent.weread.eink/com.tencent.weread.ReaderFragmentActivity}"))throw new SecurityException("Supported WeRead window is not focused: "+initial);
  String path="/dev/socket/kobo_touch_probe";if(new File(path).exists())throw new SecurityException("Existing input endpoint");
  binding=new LocalSocket();binding.bind(new LocalSocketAddress(path,LocalSocketAddress.Namespace.FILESYSTEM));Os.chmod(path,0600);server=new LocalServerSocket(binding.getFileDescriptor());
  Class<?> type=Class.forName("android.hardware.input.InputManager");final Object input=type.getMethod("getInstance").invoke(null);final Method inject=type.getMethod("injectInputEvent",InputEvent.class,int.class);
  if(resident){
   final Context requestContext=context;
   requests=new android.os.FileObserver("/lab/resident",android.os.FileObserver.MOVED_TO){public void onEvent(int event,String name){
    if("helper-launch".equals(name) || "helper-seg".equals(name) || "helper-home".equals(name))handleRequest(requestContext,name);}};
   requests.startWatching();
   System.out.println("WEREAD_HELPER_REQUESTS_READY");
  }
  System.out.println("ANDROID_TOUCH_PROBE_WINDOW_READY target=verified-weread uid=10007"+(resident?" resident=1 launcher_kind=4":""));
  new Thread(new Runnable(){public void run(){do{boolean down=false,dropping=false;long downTime=0,gestureLease=0;int lastX=0,lastY=0;try{
   try(LocalSocket client=server.accept()){
    if(client.getPeerCredentials().getUid()!=0)throw new SecurityException("Host root required");client.setSoTimeout(resident?0:(sessionSeconds+10)*1000);
    if(resident)System.out.println("WEREAD_RESIDENT_INPUT_CONNECTED");
    DataInputStream in=new DataInputStream(client.getInputStream());DataOutputStream out=new DataOutputStream(client.getOutputStream());int sequence=0;
    while(true){
     int magic;try{magic=in.readInt();}catch(EOFException closed){break;}
     int action=in.readInt(),x=in.readInt(),y=in.readInt(),serial=in.readInt();
     if(magic!=0x4b545031 || serial!=++sequence || sequence>(resident?8388608:(sessionSeconds==1800?524288:16384)))throw new SecurityException("Invalid host input serial");
     if(action==6){
      // Resident launcher only: bring the lab HOME (launcher) to the front.
      // The HOME key itself cannot be used: this runtime has no keyguard
      // service, and PhoneWindowManager's HOME handling needs one.
      if(!resident || x!=0 || y!=0 || down)throw new SecurityException("Invalid home packet");
      new Thread(new Runnable(){public void run(){try{startHome();}catch(Throwable error){error.printStackTrace();}}},"resident-home").start();
      out.writeInt(1);out.flush();continue;
     }
     if(action==7){
      // Resident only: follow the device upright or upside down. Touches
      // and page keys arrive already in the acknowledged orientation.
      if(!resident || (x!=0 && x!=2) || y!=0 || down)throw new SecurityException("Invalid rotation packet");
      out.writeInt(rotate(x)?1:2);out.flush();continue;
     }
     if(action==4 || action==5){
      if(x!=0 || y!=0)throw new SecurityException("Invalid physical page key packet");
      long keyBegin=SystemClock.uptimeMillis();int kind=live.query();long keyFocusMs=SystemClock.uptimeMillis()-keyBegin;
      if(down || kind!=3){
       System.out.println("WEREAD_PHYSICAL_PAGE_KEY_SKIPPED_NO_READER_FOCUS");out.writeInt(2);out.flush();continue;
      }
      if(gesturePending.get()){
       System.out.println("WEREAD_PHYSICAL_PAGE_KEY_SKIPPED_GESTURE_PENDING");out.writeInt(2);out.flush();continue;
      }
      if(!keyPending.compareAndSet(false,true)){
       System.out.println("WEREAD_PHYSICAL_PAGE_KEY_SKIPPED_APP_BUSY");out.writeInt(2);out.flush();continue;
      }
      final int code=action==4?KeyEvent.KEYCODE_PAGE_UP:KeyEvent.KEYCODE_PAGE_DOWN;
      final String direction=action==4?"previous":"next";final long focusMs=keyFocusMs,queuedAt=keyBegin;
      new Thread(new Runnable(){public void run(){try{
       long began=SystemClock.uptimeMillis();
       KeyEvent press=new KeyEvent(began,began,KeyEvent.ACTION_DOWN,code,0,0,-1,0,0,InputDevice.SOURCE_KEYBOARD);
       boolean accepted=Boolean.TRUE.equals(inject.invoke(input,press,2));
       KeyEvent release=new KeyEvent(began,SystemClock.uptimeMillis(),KeyEvent.ACTION_UP,code,0,0,-1,0,accepted?0:KeyEvent.FLAG_CANCELED,InputDevice.SOURCE_KEYBOARD);
       boolean released=Boolean.TRUE.equals(inject.invoke(input,release,2));
       if(accepted && released)System.out.println("WEREAD_PHYSICAL_PAGE_KEY_DISPATCHED direction="+direction);
       else System.out.println("WEREAD_PHYSICAL_PAGE_KEY_REJECTED");
       System.out.println("WEREAD_KEY_LATENCY focus_ms="+focusMs+" total_ms="+(SystemClock.uptimeMillis()-queuedAt));
      }catch(Throwable error){error.printStackTrace();}finally{keyPending.set(false);}}},"weread-page-key").start();
      out.writeInt(1);out.flush();continue;
     }
     if(x<0 || x>=width || y<0 || y>=height || action<0 || action>3 || (action==0?down:!down))throw new SecurityException("Invalid host gesture state");
     if(action==0){
      boolean owned=false;long deadline=SystemClock.uptimeMillis()+1200;
      do{LiveFocusClient.Snapshot target=live.snapshot();if(target.kind>0){owned=true;gestureLease=target.generation;break;}Thread.sleep(150);}while(SystemClock.uptimeMillis()<deadline);
      dropping=!owned;
      if(dropping)System.out.println("WEREAD_INTERACTIVE_GESTURE_SKIPPED_NO_VERIFIED_FOCUS");
      else if(keyPending.get()){dropping=true;System.out.println("WEREAD_INTERACTIVE_GESTURE_SKIPPED_KEY_PENDING");}
      else if(gesturePending.get()){dropping=true;System.out.println("WEREAD_INTERACTIVE_GESTURE_SKIPPED_GESTURE_PENDING");}
      else{System.out.println("WEREAD_INTERACTIVE_HUMAN_GESTURE_STARTED");downTime=SystemClock.uptimeMillis();}
     }
     if(action!=0 && !dropping){
      LiveFocusClient.Snapshot target=live.snapshot();
      if(!TouchLeasePolicy.same(gestureLease,target.kind,target.generation)){
       dropping=true;System.out.println("WEREAD_GESTURE_SKIPPED_WINDOW_PAUSED_OR_REPLACED");
      }
     }
     if(dropping){
      down=action==0 || action==2;
      if(!down)dropping=false;
      // ACK 2 means deliberately not injected, never application acceptance.
      out.writeInt(2);out.flush();continue;
     }
     if(action==1 && gestureWait){
      // Policy acceptance is not waited for here: the forwarder gets ACK 1
      // at once, as before; a rejected or timed-out UP is only logged.
      final MotionEvent up=TouchEvents.create(downTime,SystemClock.uptimeMillis(),action,x,y);
      gesturePending.set(true);
      new Thread(new Runnable(){public void run(){long began=SystemClock.uptimeMillis();try{
       boolean finished=Boolean.TRUE.equals(inject.invoke(input,up,2));
       long ms=SystemClock.uptimeMillis()-began;
       if(!finished)System.out.println("WEREAD_INTERACTIVE_UP_NOT_FINISHED ms="+ms);else if(ms>=1000)System.out.println("WEREAD_INTERACTIVE_UP_FINISHED_SLOW ms="+ms);
      }catch(Throwable error){error.printStackTrace();}finally{up.recycle();gesturePending.set(false);}}},"weread-gesture-up").start();
      down=false;lastX=x;lastY=y;
      try(PrintWriter record=new PrintWriter("/lab/interactive-input-result.txt")){record.println("ANDROID_WEREAD_INTERACTIVE_GESTURE_DISPATCHED events="+sequence);}System.out.println("ANDROID_WEREAD_INTERACTIVE_GESTURE_DISPATCHED");
      out.writeInt(1);out.flush();continue;
     }
     MotionEvent event=TouchEvents.create(downTime,SystemClock.uptimeMillis(),action,x,y);
     // Confirm target/policy acceptance without waiting for a busy UI thread
     // to finish dispatch. Real touch events may remain queued normally.
     boolean accepted;try{accepted=Boolean.TRUE.equals(inject.invoke(input,event,1));}finally{event.recycle();}
     if(!accepted){
      // A disappearing/replaced window may reject one event. End that gesture
      // without claiming acceptance or terminating the app's whole session.
      System.out.println("WEREAD_INTERACTIVE_INPUT_REJECTED_GESTURE_CANCELLED");
      if(down){try{cancelOwned(input,inject,live,gestureLease,downTime,lastX,lastY);}catch(Throwable ignored){}}
      down=action==0 || action==2;dropping=down;
      out.writeInt(3);out.flush();continue;
     }
     down=action==0 || action==2;lastX=x;lastY=y;
     if(action==1){try(PrintWriter record=new PrintWriter("/lab/interactive-input-result.txt")){record.println("ANDROID_WEREAD_INTERACTIVE_GESTURE_DISPATCHED events="+sequence);}System.out.println("ANDROID_WEREAD_INTERACTIVE_GESTURE_DISPATCHED");}
     out.writeInt(1);out.flush();
    }
   }
  }catch(Throwable error){error.printStackTrace();if(resident)System.exit(1);}finally{
   if(down && !dropping){try{cancelOwned(input,inject,live,gestureLease,downTime,lastX,lastY);}catch(Throwable ignored){}}
  }if(resident)System.out.println("WEREAD_RESIDENT_INPUT_DISCONNECTED");}while(resident);}},"weread-physical-input").start();
  if(!resident)new Handler().postDelayed(new Runnable(){public void run(){System.exit(0);}},(sessionSeconds+30)*1000L);Looper.loop();
 }
}
