import android.os.IBinder;
import android.system.Os;
import java.io.*;
import java.lang.reflect.*;
import java.util.Arrays;
public final class ReadingPerfProbe {
 public static void main(String[] args)throws Exception {
  if(android.os.Process.myUid()!=0||args.length!=1||!(args[0].equals("inspect")||args[0].equals("off")||args[0].equals("restore")))throw new SecurityException("Fixed root animation control");
  Class<?> sm=Class.forName("android.os.ServiceManager");
  IBinder window=(IBinder)sm.getMethod("checkService",String.class).invoke(null,"window");
  Object wm=Class.forName("android.view.IWindowManager$Stub").getMethod("asInterface",IBinder.class).invoke(null,window);
  float[] scales=(float[])Class.forName("android.view.IWindowManager").getMethod("getAnimationScales").invoke(wm);
  System.out.println("READING89_ANIMATION_SCALES "+Arrays.toString(scales));
  AnimationScalesBackup.validate(scales);
  File backup=new File("/lab/reading-animation-scales-before89.txt");
  if(!args[0].equals("inspect")){
   float[] target;
   if(args[0].equals("off")){
    if(!backup.createNewFile())throw new SecurityException("Existing animation backup");
    Os.chmod(backup.toString(),0600);
    try(FileOutputStream out=new FileOutputStream(backup)){out.write(AnimationScalesBackup.encode(scales).getBytes("US-ASCII"));out.getFD().sync();}
    target=new float[]{0,0,0};
   }else{
    android.system.StructStat st=Os.lstat(backup.toString());
    if(st.st_uid!=0||(st.st_mode&0777)!=0600||!android.system.OsConstants.S_ISREG(st.st_mode)||st.st_size>96)throw new SecurityException("Invalid backup identity");
    String line;try(BufferedReader r=new BufferedReader(new FileReader(backup))){line=r.readLine();if(r.readLine()!=null)throw new SecurityException("Trailing backup data");}
    target=AnimationScalesBackup.decode(line);
   }
   Class.forName("android.view.IWindowManager").getMethod("setAnimationScales",float[].class).invoke(wm,(Object)target);
   float[] actual=(float[])Class.forName("android.view.IWindowManager").getMethod("getAnimationScales").invoke(wm);
   if(!Arrays.equals(actual,target))throw new AssertionError("Animation scales not applied");
   System.out.println("READING89_ANIMATION_SCALES_VERIFIED "+Arrays.toString(actual));
   if(args[0].equals("restore")&&!backup.delete())throw new IOException("Backup cleanup failed");
  }
  IBinder sf=(IBinder)sm.getMethod("checkService",String.class).invoke(null,"SurfaceFlinger");
  File f=new File("/lab/reading89-surface-latency.txt");
  try(FileOutputStream out=new FileOutputStream(f)){Os.chmod(f.toString(),0600);sf.dump(out.getFD(),new String[]{"--latency"});}
  try(BufferedReader r=new BufferedReader(new FileReader(f))){String line=r.readLine();if(line!=null&&line.matches("[0-9]+"))System.out.println("READING89_SF_REFRESH_PERIOD_NS "+line);else System.out.println("READING89_SF_REFRESH_PERIOD_UNAVAILABLE");}
 }
}
