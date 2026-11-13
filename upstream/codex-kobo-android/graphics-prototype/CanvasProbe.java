import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import java.io.FileOutputStream;

/** CPU bitmap drawing probe. Does not start an Activity or install an APK. */
public final class CanvasProbe {
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && "hello".equals(args[0])) {
            System.out.println("ANDROID_FRAMEWORK_MAIN_OK");
            return;
        }
        final String output = args.length > 0 ? args[0] : "/lab/canvas-test.png";
        final int width = 720, height = 960;
        Bitmap image = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(image);
        canvas.drawColor(Color.WHITE);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(Color.BLACK);
        paint.setStyle(Paint.Style.FILL);
        canvas.drawRect(40, 40, width - 40, 160, paint);
        paint.setColor(Color.WHITE);
        paint.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
        paint.setTextSize(42);
        canvas.drawText("Android on Kobo", 65, 115, paint);
        paint.setColor(Color.BLACK);
        paint.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
        paint.setTextSize(29);
        canvas.drawText("Real Android Canvas / CPU rendering", 40, 225, paint);
        canvas.drawText("No Activity or APK installer yet", 40, 270, paint);
        for (int i = 0; i < 8; i++) {
            int level = i * 255 / 7;
            paint.setColor(Color.rgb(level, level, level));
            canvas.drawRect(40 + i * 80, 330, 120 + i * 80, 490, paint);
        }
        paint.setColor(Color.BLACK);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(3);
        canvas.drawCircle(160, 650, 95, paint);
        canvas.drawRect(330, 560, 660, 740, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setTextSize(25);
        canvas.drawText("Canvas + Skia + PNG", 40, 840, paint);
        canvas.drawText("Rendering verification", 40, 885, paint);
        if (image.getPixel(50, 50) != Color.BLACK || image.getPixel(0, 0) != Color.WHITE) {
            throw new AssertionError("Canvas pixel check failed");
        }
        try (FileOutputStream file = new FileOutputStream(output)) {
            if (!image.compress(Bitmap.CompressFormat.PNG, 100, file)) {
                throw new AssertionError("PNG encoding failed");
            }
        }
        image.recycle();
        System.out.println("ANDROID_CANVAS_PNG_OK " + width + "x" + height + " " + output);
    }
}
