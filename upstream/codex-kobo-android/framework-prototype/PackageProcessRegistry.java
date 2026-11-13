import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import java.io.*;
import java.lang.reflect.*;
/** Root-only registry populated from actual PMS identities, never APK supplied UIDs. */
public final class PackageProcessRegistry {
 public static void write(Context context)throws Exception {
  if(android.os.Process.myUid()!=0 && android.os.Process.myUid()!=1000)throw new SecurityException("Root registry required");
  File pending=new File("/data/system/kobo-install-process-allowlist.tmp");
  try(PrintWriter out=new PrintWriter(pending)){
   for(String name:new String[]{"com.kobo.lab.smoke","com.android.defcontainer","com.kobo.lab.installed","com.tencent.weread.eink"}){
    ApplicationInfo info;
    try{info=context.getPackageManager().getApplicationInfo(name,0x000c0000);}catch(PackageManager.NameNotFoundException absent){continue;}
    if(info.uid<10000 || info.uid>=20000 || info.targetSdkVersion!=(name.equals("com.tencent.weread.eink")?31:24) || !name.equals(info.processName))throw new SecurityException("Unexpected identity: "+name);
    if(name.equals("com.tencent.weread.eink")){
     int[] gids=context.getPackageManager().getPackageGids(name);
     if(gids!=null)for(int gid:gids)if(gid!=3002 && gid!=3003)throw new SecurityException("Unexpected WeRead permission group: "+gid);
     System.out.println("WEREAD_PMS_PERMISSION_GROUPS="+java.util.Arrays.toString(gids));
    }
    String seinfo=(String)ApplicationInfo.class.getField("seinfo").get(info);
    out.println(name+" "+info.uid+" "+seinfo+" "+info.dataDir+" "+info.targetSdkVersion);
    System.out.println("ANDROID_REGISTERED_PROCESS package="+name+" uid="+info.uid+" seinfo="+seinfo+" data="+info.dataDir);
   }
  }
  pending.setReadable(false,false);pending.setWritable(false,false);pending.setReadable(true,true);pending.setWritable(true,true);
  if(!pending.renameTo(new File("/data/system/kobo-install-process-allowlist.txt")))throw new IOException("Registry rename failed");
 }
}
