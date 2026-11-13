import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.MotionEvent;
import android.view.InputEvent;
import android.view.InputDevice;
import android.os.SystemClock;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Button;
import java.io.FileOutputStream;
/** Privileged system-dialog window probe; not an Activity or installed APK. */
public final class WindowClient {
    public static void main(final String[] args) throws Exception {
        if(args.length!=1)throw new IllegalArgumentException("scanout.png");
        Looper.prepareMainLooper();
        Class<?> thread=Class.forName("android.app.ActivityThread");
        Object system=thread.getMethod("systemMain").invoke(null);
        Context context=(Context)thread.getMethod("getSystemContext").invoke(system);
        context.setTheme(android.R.style.Theme_DeviceDefault_Light_NoActionBar);
        final WindowManager manager=(WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
        final boolean[] drawn={false};
        final LinearLayout root=new LinearLayout(context){
            @Override protected void dispatchDraw(android.graphics.Canvas canvas){
                super.dispatchDraw(canvas);
                if(!drawn[0]){drawn[0]=true;System.out.println("ANDROID_WINDOW_VIEW_DRAWN attached="+isAttachedToWindow()+" size="+getWidth()+"x"+getHeight()+" token="+(getWindowToken()!=null));}
            }
        };root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32,48,32,32);root.setBackgroundColor(Color.WHITE);
        TextView title=new TextView(context);title.setText("Android 真实窗口测试");title.setTextColor(Color.BLACK);title.setTextSize(26);root.addView(title);
        TextView note=new TextView(context);note.setText("ViewRoot + WindowManager\n窗口已接入系统服务\n\n尚未启动微信读书 Activity。");note.setTextColor(Color.BLACK);note.setTextSize(20);root.addView(note);
        final boolean[] clicked={false};
        final Button button=new Button(context);button.setSoundEffectsEnabled(false);button.setHapticFeedbackEnabled(false);button.setText("Android 标准按钮");root.addView(button);
        button.setOnClickListener(new View.OnClickListener(){public void onClick(View view){
            clicked[0]=true;button.setText("点击已通过 Android 输入服务");
            System.out.println("ANDROID_WINDOW_BUTTON_CLICKED");
        }});
        WindowManager.LayoutParams params=new WindowManager.LayoutParams(720,960,2008,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,PixelFormat.OPAQUE);
        params.gravity=Gravity.TOP|Gravity.LEFT;params.setTitle("Kobo window probe");
        manager.addView(root,params);
        new Handler().postDelayed(new Runnable(){public void run(){
            int[] location=new int[2];button.getLocationOnScreen(location);
            final float x=location[0]+button.getWidth()/2f,y=location[1]+button.getHeight()/2f;
            new Thread(new Runnable(){public void run(){try{
                Class<?> inputClass=Class.forName("android.hardware.input.InputManager");
                Object input=inputClass.getMethod("getInstance").invoke(null);
                java.lang.reflect.Method inject=inputClass.getMethod("injectInputEvent",InputEvent.class,int.class);
                long downTime=SystemClock.uptimeMillis();
                for(int action:new int[]{MotionEvent.ACTION_DOWN,MotionEvent.ACTION_UP}){
                    MotionEvent event=MotionEvent.obtain(downTime,SystemClock.uptimeMillis(),action,x,y,0);
                    event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
                    try{if(!Boolean.TRUE.equals(inject.invoke(input,event,2)))throw new AssertionError("Input injection failed action="+action);}
                    finally{event.recycle();}
                    if(action==MotionEvent.ACTION_DOWN)Thread.sleep(80);
                }
                System.out.println("ANDROID_INPUT_DISPATCH_INJECTION_OK x="+x+" y="+y);
            }catch(Throwable error){error.printStackTrace();}}},"probe-input-injection").start();
        }},1500);
        new Handler().postDelayed(new Runnable(){public void run(){int result=1;try{
            System.out.println("WINDOW_CLIENT_STATE attached="+root.isAttachedToWindow()+" size="+root.getWidth()+"x"+root.getHeight()+" shown="+root.isShown()+" displayState="+root.getDisplay().getState());
            android.os.IBinder windowBinder=(android.os.IBinder)Class.forName("android.os.ServiceManager").getMethod("getService",String.class).invoke(null,"window");
            try(FileOutputStream dump=new FileOutputStream("/lab/window-state.txt")){windowBinder.dump(dump.getFD(),new String[]{"-a"});}
            if(!drawn[0]||!root.isAttachedToWindow()||root.getWindowToken()==null)throw new AssertionError("No attached window draw");
            if(!clicked[0])throw new AssertionError("Button did not receive dispatched touch");
            Bitmap image=SurfaceProbe.captureVirtualFramebuffer(720,960);
            int[] pixels=new int[720*960];image.getPixels(pixels,0,720,0,0,720,960);int white=0,dark=0;
            for(int pixel:pixels){int value=(pixel>>>16)&255;if(value>245)white++;if(value<100)dark++;}
            if(white<400000||dark<200)throw new AssertionError("Unexpected scanout white="+white+" dark="+dark);
            try(FileOutputStream out=new FileOutputStream(args[0])){if(!image.compress(Bitmap.CompressFormat.PNG,100,out))throw new AssertionError("PNG failed");}
            image.recycle();System.out.println("ANDROID_ATTACHED_WINDOW_SCANOUT_OK white="+white+" dark="+dark);result=0;
        }catch(Throwable error){error.printStackTrace();}finally{manager.removeViewImmediate(root);System.exit(result);}}},7000);
        Looper.loop();
    }
}
