import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.system.Os;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.io.*;
import java.lang.reflect.Method;

/** Opaque four-target test window. Only physical host input, no synthetic taps. */
public final class WeReadTouchProbe {
 private static LocalSocket binding;
 private static LocalServerSocket server;
 private static volatile boolean ready;
 private static final String PATH="/dev/socket/kobo_touch_probe";
 public static void main(String[] args)throws Exception {
  if(args.length!=0 || android.os.Process.myUid()!=0)throw new SecurityException("Fixed root touch probe only");
  if(new File(PATH).exists())throw new SecurityException("Existing touch endpoint");
  Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  final android.graphics.Point bounds=new android.graphics.Point();
  ((android.view.WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealSize(bounds);
  final int width=bounds.x,height=bounds.y;
  if(!((width==720 && height==960)||(width==632 && height==840)))throw new SecurityException("Unsupported runtime display");
  context.setTheme(android.R.style.Theme_DeviceDefault_Light_NoActionBar);
  final FrameLayout root=new FrameLayout(context);root.setBackgroundColor(Color.WHITE);
  TextView title=new TextView(context);title.setText("Kobo 真实触控测试");title.setTextColor(Color.BLACK);title.setTextSize(26);
  FrameLayout.LayoutParams titleParams=new FrameLayout.LayoutParams(width-80,90);titleParams.leftMargin=40;titleParams.topMargin=70;root.addView(title,titleParams);
  final TextView status=new TextView(context);status.setText("请依次点击 1、2、3、4\n测试不会确认协议或登录。");status.setTextColor(Color.BLACK);status.setTextSize(20);
  FrameLayout.LayoutParams statusParams=new FrameLayout.LayoutParams(width-80,150);statusParams.leftMargin=40;statusParams.topMargin=height-170;root.addView(status,statusParams);
  final int[] expected={1};
  for(int i=1;i<=4;i++){
   final int number=i;final Button button=new Button(context);button.setText(Integer.toString(i));button.setTextSize(34);button.setSoundEffectsEnabled(false);button.setHapticFeedbackEnabled(false);
   FrameLayout.LayoutParams params=new FrameLayout.LayoutParams((width-180)/2,140);params.leftMargin=(i%2==1)?60:width/2+30;params.topMargin=i<=2?height/4:height*3/5;root.addView(button,params);
   button.setOnClickListener(new View.OnClickListener(){public void onClick(View view){
    if(number!=expected[0]){System.out.println("ANDROID_TOUCH_TARGET_OUT_OF_ORDER expected="+expected[0]+" actual="+number);return;}
    button.setText(number+" ✓");expected[0]++;
    System.out.println("ANDROID_PHYSICAL_TOUCH_TARGET_CLICKED target="+number);
    status.setText(expected[0]<=4?"已确认 "+number+"，请点击 "+expected[0]:"四个目标均已通过真实触屏点击。\n请等待自动恢复 KOReader。");
    if(expected[0]==5){try(PrintWriter out=new PrintWriter("/lab/touch-probe-result.txt")){out.println("ANDROID_PHYSICAL_FOUR_TARGETS_CLICKED_IN_ORDER");}catch(Exception error){throw new RuntimeException(error);}}
   }});
  }
  final WindowManager manager=(WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
  WindowManager.LayoutParams window=new WindowManager.LayoutParams(width,height,2008,WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,PixelFormat.OPAQUE);
  window.gravity=Gravity.TOP|Gravity.LEFT;window.setTitle("Kobo physical touch test");manager.addView(root,window);
  binding=new LocalSocket();binding.bind(new LocalSocketAddress(PATH,LocalSocketAddress.Namespace.FILESYSTEM));Os.chmod(PATH,0600);
  server=new LocalServerSocket(binding.getFileDescriptor());
  final Class<?> type=Class.forName("android.hardware.input.InputManager");final Object input=type.getMethod("getInstance").invoke(null);final Method inject=type.getMethod("injectInputEvent",InputEvent.class,int.class);
  new Handler().postDelayed(new Runnable(){public void run(){
   if(!root.isAttachedToWindow() || root.getWidth()!=width || root.getHeight()!=height)throw new AssertionError("No full test window");
   ready=true;System.out.println("ANDROID_TOUCH_PROBE_WINDOW_READY size="+width+"x"+height);
  }},1500);
  new Thread(new Runnable(){public void run(){boolean down=false;long downTime=0;int lastX=0,lastY=0;try{
   try(LocalSocket client=server.accept()){
    if(client.getPeerCredentials().getUid()!=0)throw new SecurityException("Host root peer required");
    client.setSoTimeout(180000);
    DataInputStream in=new DataInputStream(client.getInputStream());DataOutputStream out=new DataOutputStream(client.getOutputStream());
    int sequence=0;
    while(true){
     int magic;try{magic=in.readInt();}catch(EOFException closed){break;}
     int action=in.readInt(),x=in.readInt(),y=in.readInt(),serial=in.readInt();
     if(!ready || magic!=0x4b545031 || serial!=++sequence || sequence>4096 || x<0 || x>=width || y<0 || y>=height || action<0 || action>3 || (action==0?down:!down))throw new SecurityException("Invalid host input state");
     long time=SystemClock.uptimeMillis();if(action==0)downTime=time;
     MotionEvent event=TouchEvents.create(downTime,time,action,x,y);
     boolean accepted;try{accepted=Boolean.TRUE.equals(inject.invoke(input,event,2));}finally{event.recycle();}
     if(!accepted)throw new AssertionError("Real InputManager rejected physical event");
     down=action==0 || action==2;lastX=x;lastY=y;
     out.writeInt(1);out.flush();
    }
    System.out.println("ANDROID_HOST_TOUCH_CLIENT_CLOSED");
   }
  }catch(Throwable error){error.printStackTrace();}finally{
   if(down){try{MotionEvent cancel=TouchEvents.create(downTime,SystemClock.uptimeMillis(),MotionEvent.ACTION_CANCEL,lastX,lastY);try{inject.invoke(input,cancel,2);}finally{cancel.recycle();}}catch(Throwable ignored){}}
  }}},"physical-touch-input").start();
  new Handler().postDelayed(new Runnable(){public void run(){System.err.println("ANDROID_TOUCH_PROBE_TIMEOUT");manager.removeViewImmediate(root);System.exit(91);}},180000);
  Looper.loop();
 }
}
