// Isolated host fixture; never included in the Android runtime.
package android.system;public final class Os {public static boolean marker=true;public static long metadataCost;public static StructStat lstat(String path)throws java.io.IOException {android.os.SystemClock.now+=metadataCost;if(!marker)throw new java.io.FileNotFoundException();return new StructStat(0,0100400);}}
