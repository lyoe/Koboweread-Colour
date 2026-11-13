import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.InputEvent;
import android.view.MotionEvent;
import java.io.FileOutputStream;
import java.lang.reflect.Method;
/** Fixed non-consenting button diagnostic, guarded by known login-page pixels. */
public final class LoginTapDiagnostic {
 private static Bitmap capture()throws Exception {
  return (Bitmap)Class.forName("SurfaceProbe").getMethod("captureVirtualFramebuffer",int.class,int.class).invoke(null,720,960);
 }
 private static void save(Bitmap bitmap,String path)throws Exception {
  try(FileOutputStream out=new FileOutputStream(path)){if(!bitmap.compress(Bitmap.CompressFormat.PNG,100,out))throw new AssertionError("Capture failed");}
 }
 private static int fingerprint(Bitmap bitmap){
  int[] pixels=new int[720*960];bitmap.getPixels(pixels,0,720,0,0,720,960);int hash=0x811c9dc5;
  for(int pixel:pixels){int gray=(77*((pixel>>>16)&255)+150*((pixel>>>8)&255)+29*(pixel&255)+128)>>>8;hash=(hash^gray)*16777619;}
  return hash;
 }
 public static void main(String[] args)throws Exception {
  if(args.length!=0 || android.os.Process.myUid()!=0)throw new SecurityException("Fixed root diagnostic only");
  Method focus=WeReadInputProbe.class.getDeclaredMethod("focus",String.class);focus.setAccessible(true);
  String beforeFocus=(String)focus.invoke(null,"/lab/login-tap-before-window.txt");
  if(!beforeFocus.contains(" com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity}"))throw new SecurityException("Unexpected focus");
  Bitmap before=capture();int hash=fingerprint(before);save(before,"/lab/login-tap-before.png");before.recycle();
  System.out.printf("LOGIN_TAP_BASELINE_GRAY_FNV=%08x%n",hash);
  if(hash!=0xc90cadf4 && hash!=0x0c9d0dc5)throw new SecurityException("Not the known unchecked login page; no click sent");
  Class<?> type=Class.forName("android.hardware.input.InputManager");Object input=type.getMethod("getInstance").invoke(null);Method inject=type.getMethod("injectInputEvent",InputEvent.class,int.class);
  long down=SystemClock.uptimeMillis();
  for(int action:new int[]{MotionEvent.ACTION_DOWN,MotionEvent.ACTION_UP}){
   MotionEvent event=TouchEvents.create(down,SystemClock.uptimeMillis(),action,360,505);
   try{if(!Boolean.TRUE.equals(inject.invoke(input,event,2)))throw new AssertionError("Button event rejected");}finally{event.recycle();}
   if(action==MotionEvent.ACTION_DOWN)Thread.sleep(120);
  }
  System.out.println("LOGIN_TAP_GET_QR_BUTTON_DISPATCHED x=360 y=505 tool=FINGER consent-actions=0");
  Thread.sleep(500);Bitmap immediate=capture();save(immediate,"/lab/login-tap-immediate.png");immediate.recycle();
  Thread.sleep(2000);Bitmap later=capture();save(later,"/lab/login-tap-after.png");later.recycle();
  focus.invoke(null,"/lab/login-tap-after-window.txt");System.out.println("LOGIN_TAP_DIAGNOSTIC_CAPTURED");System.exit(0);
 }
}
