import android.app.Activity;
import android.graphics.*;
import android.os.*;
import android.view.*;
import android.webkit.WebView;
import java.io.*;
import java.lang.reflect.*;
import java.util.Map;
/** Opt-in public-agreement-only software draw comparison; changes no View properties. */
public final class WebViewRenderingDiagnostic {
 private static final String URL="https://weread-1258476243.file.myqcloud.com/app/weread_user_agreement_android.html";
 private static final File DIR=new File("/data/local/tmp/kobo-webview-diagnostic");
 private static WebView find(View view){
  if(view instanceof WebView){WebView w=(WebView)view;String url=w.getUrl();if(url!=null&&(url.equals(URL)||url.startsWith(URL+"?"))&&w.getProgress()==100&&w.getWidth()>0&&w.getHeight()>0)return w;}
  if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++){WebView w=find(group.getChildAt(i));if(w!=null)return w;}}
  return null;
 }
 private static WebView current()throws Exception {
  Class<?> t=Class.forName("android.app.ActivityThread");Object thread=t.getMethod("currentActivityThread").invoke(null);if(thread==null)return null;
  Field activities=t.getDeclaredField("mActivities");activities.setAccessible(true);
  for(Object record:((Map<?,?>)activities.get(thread)).values()){
   Field field=record.getClass().getDeclaredField("activity");field.setAccessible(true);Activity activity=(Activity)field.get(record);
   if(activity!=null&&!activity.isFinishing()&&"com.tencent.weread.eink".equals(activity.getPackageName())){WebView w=find(activity.getWindow().getDecorView());if(w!=null)return w;}
  }
  return null;
 }
 private static int save(Bitmap image,String name)throws Exception {
  try(FileOutputStream out=new FileOutputStream(new File(DIR,name))){if(!image.compress(Bitmap.CompressFormat.PNG,100,out))throw new IOException("PNG compression failed");}
  int[] pixels=new int[image.getWidth()*image.getHeight()];image.getPixels(pixels,0,image.getWidth(),0,0,image.getWidth(),image.getHeight());int dark=0;
  for(int p:pixels)if(((p>>>16)&255)<224||((p>>>8)&255)<224||(p&255)<224)dark++;
  return dark;
 }
 private static Object awContents(WebView web)throws Exception {
  Field providerField=WebView.class.getDeclaredField("mProvider");providerField.setAccessible(true);Object provider=providerField.get(web);
  Field contents=provider.getClass().getDeclaredField("mAwContents");contents.setAccessible(true);return contents.get(provider);
 }
 private static void nativeState(WebView web,StringBuilder report){try{
  Object aw=awContents(web);report.append("PROVIDER_AW_CLASS=").append(aw.getClass().getName()).append("\n");
  for(String name:new String[]{"mIsPaused","mViewVisibility","mWindowVisibility","mIsAttachedToWindow","mAwViewMethods","mScrollOffsetManager"}){
   try{Field f=aw.getClass().getDeclaredField(name);f.setAccessible(true);Object value=f.get(aw);report.append("AW_FIELD ").append(name).append("=").append(value==null?"null":value instanceof Number||value instanceof Boolean?value.toString():value.getClass().getName()).append("\n");}catch(NoSuchFieldException missing){report.append("AW_FIELD_ABSENT ").append(name).append("\n");}
  }
 }catch(Exception error){report.append("AW_STATE_UNAVAILABLE ").append(error.getClass().getName()).append("\n");}}
 private static void boundedNativePicture(WebView web,Bitmap image,StringBuilder report){try{
  Object aw=awContents(web);Field f=aw.getClass().getDeclaredField("mNativeAwContents");f.setAccessible(true);long contents=f.getLong(aw);if(contents==0)throw new IllegalStateException("Native contents destroyed");
  Method capture=aw.getClass().getDeclaredMethod("nativeCapturePicture",long.class,int.class,int.class);capture.setAccessible(true);
  long handle=((Long)capture.invoke(aw,contents,web.getWidth(),web.getHeight())).longValue();if(handle==0)throw new IllegalStateException("No native picture");
  Class<?> type=Class.forName("org.chromium.android_webview.AwPicture",true,aw.getClass().getClassLoader());Constructor<?> constructor=type.getDeclaredConstructor(long.class);constructor.setAccessible(true);Picture picture=(Picture)constructor.newInstance(handle);
  Canvas canvas=new Canvas(image);canvas.drawColor(Color.WHITE,PorterDuff.Mode.SRC);picture.draw(canvas);
  report.append("BOUNDED_NATIVE_PICTURE width=").append(picture.getWidth()).append(" height=").append(picture.getHeight()).append(" darkPixels=").append(save(image,"webview-bounded-native-picture.png")).append("\n");
 }catch(Exception error){report.append("BOUNDED_NATIVE_PICTURE_UNAVAILABLE ").append(error.getClass().getName()).append(": ").append(error.getMessage()).append("\n");}}
 private static void tree(View view,StringBuilder report,int depth,int[] count){
  if(depth>32||count[0]++>=256)return;int[] xy=new int[2];view.getLocationOnScreen(xy);
  report.append("TREE depth=").append(depth).append(" class=").append(view.getClass().getName()).append(" x=").append(xy[0]).append(" y=").append(xy[1]).append(" width=").append(view.getWidth()).append(" height=").append(view.getHeight()).append(" visibility=").append(view.getVisibility()).append(" alpha=").append(view.getAlpha());
  android.graphics.drawable.Drawable background=view.getBackground();if(background instanceof android.graphics.drawable.ColorDrawable)report.append(" backgroundColor=").append(Integer.toHexString(((android.graphics.drawable.ColorDrawable)background).getColor()));
  if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;report.append(" clipChildren=").append(group.getClipChildren()).append(" clipToPadding=").append(group.getClipToPadding());}
  report.append("\n");if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++)tree(group.getChildAt(i),report,depth+1,count);}
 }
 private static void inspect(WebView web)throws Exception {
  int width=web.getWidth(),height=web.getHeight();if(width>720||height>960||android.os.Process.myUid()!=10007||!DIR.isDirectory())throw new SecurityException("Unexpected diagnostic bounds or identity");
  StringBuilder report=new StringBuilder("ACTUAL_WEREAD_SOFTWARE_DRAW uid=10007 progress="+web.getProgress()+" contentHeight="+web.getContentHeight()+"\n");
  nativeState(web,report);
  for(View view=web;view!=null;){
   int[] location=new int[2];view.getLocationOnScreen(location);
   report.append(view.getClass().getName()).append(" width=").append(view.getWidth()).append(" height=").append(view.getHeight()).append(" x=").append(location[0]).append(" y=").append(location[1]).append(" visibility=").append(view.getVisibility()).append(" shown=").append(view.isShown()).append(" alpha=").append(view.getAlpha()).append(" scaleX=").append(view.getScaleX()).append(" scaleY=").append(view.getScaleY()).append(" clip=").append(view.getClipBounds()).append(" layer=").append(view.getLayerType()).append(" hardware=").append(view.isHardwareAccelerated()).append(" windowVisibility=").append(view.getWindowVisibility()).append(" focus=").append(view.hasWindowFocus()).append("\n");
   ViewParent parent=view.getParent();view=parent instanceof View?(View)parent:null;
  }
  View decor=web;while(decor.getParent() instanceof View)decor=(View)decor.getParent();tree(decor,report,0,new int[]{0});
  Bitmap image=Bitmap.createBitmap(width,height,Bitmap.Config.ARGB_8888);
  try{
   Canvas canvas=new Canvas(image);canvas.drawColor(Color.WHITE);report.append("BITMAP_CANVAS_CLIP_BEFORE=").append(canvas.getClipBounds()).append("\n");web.draw(canvas);report.append("BITMAP_CANVAS_CLIP_AFTER=").append(canvas.getClipBounds()).append("\n");int direct=save(image,"webview-direct-draw.png");report.append("DIRECT_DRAW_DARK_PIXELS=").append(direct).append("\n");
   canvas=new Canvas(image);canvas.drawColor(Color.WHITE,PorterDuff.Mode.SRC);Picture picture=web.capturePicture();report.append("PICTURE width=").append(picture.getWidth()).append(" height=").append(picture.getHeight()).append("\n");picture.draw(canvas);report.append("CAPTURE_PICTURE_DARK_PIXELS=").append(save(image,"webview-capture-picture.png")).append("\n");
   boundedNativePicture(web,image,report);
  }finally{image.recycle();}
  try(FileOutputStream out=new FileOutputStream(new File(DIR,"rendering-report.txt"))){out.write(report.toString().getBytes("UTF-8"));}
  System.out.println(report.toString());System.out.println("WEREAD_SOFTWARE_DRAW_DIAGNOSTIC_COMPLETE");
 }
 public static void start(){
  if(!"1".equals(System.getenv("KOBO_WEREAD_DRAW_DIAGNOSTIC")))return;
  final Handler handler=new Handler(Looper.getMainLooper());handler.postDelayed(new Runnable(){int attempts;public void run(){try{
   WebView web=current();if(web==null){if(++attempts<100)handler.postDelayed(this,2000);return;}inspect(web);
  }catch(Throwable error){System.out.println("WEREAD_SOFTWARE_DRAW_DIAGNOSTIC_FAILED");error.printStackTrace();}}},2000);
 }
}
