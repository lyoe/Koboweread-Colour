import android.os.IBinder;
import android.os.SystemClock;
import android.view.InputEvent;
import android.view.InputDevice;
import android.view.KeyEvent;
import java.io.*;
import java.lang.reflect.Method;

/** One Back key to the already focused login page; no touch, consent or login. */
public final class WeReadInputProbe {
 private static String focus(String file)throws Exception {
  IBinder window=(IBinder)Class.forName("android.os.ServiceManager").getMethod("getService",String.class).invoke(null,"window");
  if(window==null)throw new AssertionError("No window service");
  try(FileOutputStream out=new FileOutputStream(file)){window.dump(out.getFD(),new String[]{"windows"});}
  try(BufferedReader in=new BufferedReader(new FileReader(file))){
   String line;while((line=in.readLine())!=null)if(line.trim().startsWith("mCurrentFocus="))return line.trim();
  }
  throw new AssertionError("No current focus in window dump");
 }
 public static void main(String[] args)throws Exception {
  if(args.length!=1 || !("back-to-home".equals(args[0]) || "verify-login".equals(args[0])) || android.os.Process.myUid()!=0)throw new SecurityException("Fixed root input probe only");
  if("verify-login".equals(args[0])){
   for(int i=0;i<30;i++){
    String current=focus("/lab/weread-input-reopened-window.txt");
    if(current.contains(" com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity}")){
     System.out.println("WEREAD_REAL_MAIN_WINDOW_FOCUSED_AFTER_REOPEN");System.exit(0);
    }
    Thread.sleep(500);
   }
   throw new AssertionError("WeRead main window did not regain focus");
  }
  String before=focus("/lab/weread-input-before-window.txt");
  if(!before.contains(" com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity}"))throw new AssertionError("Unexpected focus: "+before);
  Class<?> type=Class.forName("android.hardware.input.InputManager");
  Object input=type.getMethod("getInstance").invoke(null);
  Method inject=type.getMethod("injectInputEvent",InputEvent.class,int.class);
  long down=SystemClock.uptimeMillis();
  for(int action:new int[]{KeyEvent.ACTION_DOWN,KeyEvent.ACTION_UP}){
   KeyEvent event=new KeyEvent(down,SystemClock.uptimeMillis(),action,KeyEvent.KEYCODE_BACK,0,0,-1,0,0,InputDevice.SOURCE_KEYBOARD);
   if(!Boolean.TRUE.equals(inject.invoke(input,event,2)))throw new AssertionError("Back injection rejected action="+action);
   if(action==KeyEvent.ACTION_DOWN)Thread.sleep(80);
  }
  System.out.println("WEREAD_BACK_KEY_DISPATCHED");
  for(int i=0;i<20;i++){
   Thread.sleep(500);String after=focus("/lab/weread-input-after-window.txt");
   if(after.contains(" com.kobo.lab.smoke/com.kobo.lab.smoke.HomeActivity}")){
    System.out.println("WEREAD_BACK_KEY_RETURNED_TO_REAL_HOME");System.exit(0);
   }
  }
  throw new AssertionError("Back did not focus the real HOME; no further keys sent");
 }
}
