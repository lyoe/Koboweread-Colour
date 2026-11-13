// Isolated host fixture; never included in the Android runtime.
package android.content; public final class ComponentName {private final String pkg,cls;public ComponentName(String p,String c){pkg=p;cls=c;}public String getPackageName(){return pkg;}public String getClassName(){return cls;}}
