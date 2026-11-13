import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Process;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import java.io.*;
import java.security.MessageDigest;
/** Root-private metadata cache of a previously full-hashed fixed original APK.
 * Only trusted immutable-to-app package files qualify. No cache for general APKs.
 */
public final class ApkIdentityCache {
 private static final String SOURCE="/lab/weread.apk",CACHE="/lab/weread-identity-cache-v1";
 private static final String CERT="e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c";
 private static String stamp(String path,int owner)throws Exception {
  StructStat s=Os.lstat(path);
  if(!OsConstants.S_ISREG(s.st_mode) || s.st_uid!=owner || (s.st_mode&0022)!=0)throw new SecurityException("APK cache requires trusted regular package files");
  return s.st_dev+":"+s.st_ino+":"+s.st_size+":"+s.st_mtime+":"+s.st_ctime+":"+s.st_uid+":"+s.st_gid+":"+s.st_mode;
 }
 private static String expected(ApplicationInfo app,String sha)throws Exception {
  return "kobo-weread-identity-v1\n"+sha+"\n"+CERT+"\n"+app.uid+"\n"+SOURCE+"\n"+stamp(SOURCE,0)+"\n"+app.sourceDir+"\n"+stamp(app.sourceDir,1000)+"\n";
 }
 private static boolean matches(String cache,String expected)throws Exception {
  try{
   StructStat s=Os.lstat(cache);
   if(!OsConstants.S_ISREG(s.st_mode) || s.st_uid!=0 || (s.st_mode&07777)!=0400 || s.st_size!=expected.getBytes("UTF-8").length)return false;
   try(InputStream in=new FileInputStream(cache)){
    byte[] data=new byte[(int)s.st_size];int at=0,n;while(at<data.length && (n=in.read(data,at,data.length-at))>0)at+=n;
    return at==data.length && in.read()==-1 && expected.equals(new String(data,"UTF-8"));
   }
  }catch(android.system.ErrnoException absent){if(absent.errno==OsConstants.ENOENT)return false;throw absent;}
 }
 private static void publish(String cache,String record)throws Exception {
  File temp=File.createTempFile(".weread-identity-",".tmp",new File("/lab"));
  try{
   try(FileOutputStream out=new FileOutputStream(temp)){out.write(record.getBytes("UTF-8"));out.getFD().sync();}
   Os.chmod(temp.getPath(),0400);Os.rename(temp.getPath(),cache);
  }finally{temp.delete();}
 }
 public static String verifiedRecord(Context context,ApplicationInfo app,String sha)throws Exception {
  if(Process.myUid()!=0 || app.uid!=10007 || !app.sourceDir.startsWith("/data/app/com.tencent.weread.eink-") || !app.sourceDir.endsWith("/base.apk"))throw new SecurityException("Fixed original package only");
  PackageInfo p=context.getPackageManager().getPackageInfo("com.tencent.weread.eink",PackageManager.GET_SIGNATURES);
  if(p.applicationInfo.uid!=10007 || !app.sourceDir.equals(p.applicationInfo.sourceDir) || p.signatures==null || p.signatures.length!=1)throw new SecurityException("Unexpected cached package identity");
  byte[] hash=MessageDigest.getInstance("SHA-256").digest(p.signatures[0].toByteArray());StringBuilder cert=new StringBuilder();
  for(byte b:hash)cert.append(String.format(java.util.Locale.US,"%02x",b&255));
  if(!CERT.equals(cert.toString()))throw new SecurityException("Unexpected package signer");
  return expected(app,sha);
 }
 public static boolean hit(String record)throws Exception {return matches(CACHE,record);}
 public static void save(ApplicationInfo app,String sha,String before)throws Exception {
  if(!before.equals(expected(app,sha)))throw new SecurityException("Package changed during full hash");
  publish(CACHE,before);
 }
 public static void selfTest()throws Exception {
  if(Process.myUid()!=0)throw new SecurityException("Root self test only");
  File dir=new File("/lab/apk-identity-cache-selftest");if(!dir.mkdir())throw new SecurityException("Self test directory exists");
  File source=new File(dir,"source"),cache=new File(dir,"cache");
  try{
   try(FileOutputStream out=new FileOutputStream(source)){out.write(1);}Os.chmod(source.getPath(),0400);
   String first=stamp(source.getPath(),0);publish(cache.getPath(),first);if(!matches(cache.getPath(),first))throw new AssertionError("Cache round trip failed");
   Os.chmod(cache.getPath(),0644);if(matches(cache.getPath(),first))throw new AssertionError("Writable cache accepted");Os.chmod(cache.getPath(),0400);
   Os.chmod(source.getPath(),0600);try(FileOutputStream out=new FileOutputStream(source,true)){out.write(2);}Os.chmod(source.getPath(),0400);
   if(matches(cache.getPath(),stamp(source.getPath(),0)))throw new AssertionError("Changed file accepted");
   if(!cache.delete())throw new IOException("Cannot delete self test cache");Os.symlink(source.getPath(),cache.getPath());if(matches(cache.getPath(),first))throw new AssertionError("Symlink cache accepted");if(!cache.delete())throw new IOException("Cannot delete self test cache");
   publish(cache.getPath(),first+"corrupt");if(matches(cache.getPath(),first))throw new AssertionError("Corrupt cache accepted");
   System.out.println("APK_IDENTITY_CACHE_SELFTEST_OK roundtrip permissions changed_file symlink corrupted_record synthetic_files_only");
  }finally{cache.delete();source.delete();dir.delete();}
 }
}
