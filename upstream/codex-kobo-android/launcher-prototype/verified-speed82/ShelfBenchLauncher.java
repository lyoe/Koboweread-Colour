import dalvik.system.PathClassLoader;
public class ShelfBenchLauncher {
 public static void main(String[] args)throws Exception {
  if(android.os.Process.myUid()!=0)throw new SecurityException("Private root benchmark only");
  if(args.length!=0)throw new IllegalArgumentException("No user data arguments");
  PathClassLoader loader=new PathClassLoader("/data/app/com.tencent.weread.eink-1/base.apk:/lab/shelf-bench-case82.dex",ShelfBenchLauncher.class.getClassLoader());
  Class.forName("ShelfBenchCase",true,loader).getMethod("run").invoke(null);
 }
}
