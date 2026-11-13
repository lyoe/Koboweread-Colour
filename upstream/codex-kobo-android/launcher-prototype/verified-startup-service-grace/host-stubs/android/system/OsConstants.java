// Isolated host fixture; never included in the Android runtime.
package android.system;public final class OsConstants {public static boolean S_ISREG(int mode){return (mode&0170000)==0100000;}}
