import android.content.ComponentName;
import android.content.Intent;
import java.lang.reflect.Method;

/** Starts WeRead 1.9.7's word-segmentation service (process :seg) ahead of
 * the first book open; run as root by resident-loop.sh (this runtime has no
 * am.jar). Root may start the unexported service. Prints the started
 * component, exits 1 when the activity manager refuses it. */
public final class SegPrestart {
    public static void main(String[] args) throws Exception {
        Intent intent = new Intent().setComponent(new ComponentName(
                "com.tencent.weread.eink", "com.tencent.weread.reader.segment.aidl.SegmentService"));
        Object am = Class.forName("android.app.ActivityManagerNative").getMethod("getDefault").invoke(null);
        Method start = null;
        for (Method m : am.getClass().getMethods())
            if (m.getName().equals("startService") && m.getParameterTypes().length == 5) start = m;
        if (start == null) throw new IllegalStateException("startService not found");
        Object started = start.invoke(am, null, intent, null, "com.android.shell", 0);
        System.out.println("SEG_PRESTART_RESULT " + started);
        System.exit(started instanceof ComponentName
                && !((ComponentName) started).getPackageName().startsWith("!") ? 0 : 1);
    }
}
