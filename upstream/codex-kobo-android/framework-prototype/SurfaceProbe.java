import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.Surface;
import java.io.FileOutputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.lang.reflect.*;

/** Real BufferQueue -> SurfaceFlinger composition test. No Activity or input dispatch. */
public final class SurfaceProbe {
    public static void main(String[] args) throws Exception {
        if(args.length!=2)throw new IllegalArgumentException("source.png capture.png");
        Bitmap source=BitmapFactory.decodeFile(args[0]);
        if(source==null)throw new AssertionError("Source decode failed");
        int width=source.getWidth(),height=source.getHeight();
        Class<?> sessionClass=Class.forName("android.view.SurfaceSession");
        Class<?> controlClass=Class.forName("android.view.SurfaceControl");
        Object session=sessionClass.getConstructor().newInstance();
        Object control=null;Constructor<Surface> surfaceConstructor=Surface.class.getDeclaredConstructor();
        surfaceConstructor.setAccessible(true);Surface surface=surfaceConstructor.newInstance();
        try {
            control=controlClass.getConstructor(sessionClass,String.class,int.class,int.class,int.class,int.class)
                .newInstance(session,"Kobo framework probe",width,height,1,4);
            Surface.class.getMethod("copyFrom",controlClass).invoke(surface,control);
            Canvas canvas=surface.lockCanvas(new Rect(0,0,width,height));
            canvas.drawBitmap(source,0,0,null);
            surface.unlockCanvasAndPost(canvas);
            controlClass.getMethod("openTransaction").invoke(null);
            try {
                controlClass.getMethod("setLayerStack",int.class).invoke(control,0);
                controlClass.getMethod("setLayer",int.class).invoke(control,1000);
                controlClass.getMethod("setPosition",float.class,float.class).invoke(control,0f,0f);
                controlClass.getMethod("show").invoke(control);
            } finally { controlClass.getMethod("closeTransaction").invoke(null); }
            System.out.println("ANDROID_SURFACE_BUFFER_POSTED "+width+"x"+height);
            Thread.sleep(1500);
            // GLES1 libagl lacks framebuffer-object screenshot support. Read the actual
            // file-backed scanout instead, with stable sequence/offset validation.
            Bitmap captured=captureVirtualFramebuffer(width,height);
            try(FileOutputStream out=new FileOutputStream(args[1])) {
                if(!captured.compress(Bitmap.CompressFormat.PNG,100,out))throw new AssertionError("Capture PNG failed");
            }
            int[] expected=new int[width*height],actual=new int[width*height];
            source.getPixels(expected,0,width,0,0,width,height);captured.getPixels(actual,0,width,0,0,width,height);
            int matched=0,dark=0,darkMatched=0;
            for(int i=0;i<expected.length;i++) {
                int a=expected[i],b=actual[i];
                boolean near=Math.abs((a&255)-(b&255))<=3 && Math.abs(((a>>>8)&255)-((b>>>8)&255))<=3 && Math.abs(((a>>>16)&255)-((b>>>16)&255))<=3;
                if(near)matched++;
                if(((a>>>16)&255)<100) { dark++;if(near)darkMatched++; }
            }
            System.out.println("SURFACE_CAPTURE_COMPARE matched="+matched+" total="+expected.length+" dark="+dark+" darkMatched="+darkMatched);
            if(dark<100 || matched<expected.length*0.98 || darkMatched<dark*0.9)throw new AssertionError("Compositor pixel comparison failed");
            source.recycle();captured.recycle();
            System.out.println("ANDROID_SURFACEFLINGER_COMPOSITION_OK "+args[1]);
        } finally {
            surface.release();
            if(control!=null)controlClass.getMethod("destroy").invoke(control);
            sessionClass.getMethod("kill").invoke(session);
        }
        System.exit(0);
    }
    private static byte[] readState() throws Exception {
        byte[] state=new byte[24];
        try(RandomAccessFile file=new RandomAccessFile("/lab/virtual-fb.state","r")){file.readFully(state);}
        return state;
    }
    public static Bitmap captureVirtualFramebuffer(int width,int height) throws Exception {
        byte[] rgba=new byte[width*height*4];
        for(int attempt=0;attempt<8;attempt++) {
            byte[] before=readState();ByteBuffer state=ByteBuffer.wrap(before).order(ByteOrder.LITTLE_ENDIAN);
            if(state.getInt()!=0x4b464230 || state.getInt()!=width || state.getInt()!=height || state.getInt()!=width*4)throw new AssertionError("Virtual framebuffer layout mismatch");
            int yoffset=state.getInt(),sequence=state.getInt();
            if(yoffset!=0 && yoffset!=height)throw new AssertionError("Invalid scanout offset");
            try(RandomAccessFile file=new RandomAccessFile("/dev/graphics/fb0","r")) {
                file.seek((long)yoffset*width*4);file.readFully(rgba);
            }
            if(!java.util.Arrays.equals(before,readState())){Thread.sleep(50);continue;}
            int[] colors=new int[width*height];
            for(int i=0;i<colors.length;i++)colors[i]=0xff000000|((rgba[i*4]&255)<<16)|((rgba[i*4+1]&255)<<8)|(rgba[i*4+2]&255);
            System.out.println("ACTUAL_VIRTUAL_SCANOUT_READ sequence="+sequence+" yoffset="+yoffset);
            return Bitmap.createBitmap(colors,width,height,Bitmap.Config.ARGB_8888);
        }
        throw new AssertionError("No stable scanout snapshot");
    }

}
