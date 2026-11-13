import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInstaller;
import android.os.*;
import java.io.*;
import java.lang.reflect.*;
/** Uses the actual PackageInstaller Session API and receives its real status Intent. */
public final class InstallProbe {
 private static String packageName="com.kobo.lab.installed", sourcePath="/lab/kobo-installed-probe.apk";
 private static final String WEREAD_SHA="04d86bcdc1bbd77cab4ecc4d1c04e869d76dbabc79c60c52a707c64fc3e4b75f";
 private static final String WEREAD_CERT="e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c";
 private static String hex(byte[] bytes)throws Exception {
  byte[] hash=java.security.MessageDigest.getInstance("SHA-256").digest(bytes);StringBuilder out=new StringBuilder();
  for(byte b:hash)out.append(String.format(java.util.Locale.US,"%02x",b&255));return out.toString();
 }

 public static void run(final String path)throws Exception {
  if(android.os.Process.myUid()!=0)throw new SecurityException("Root probe required");
  if("/lab/weread.apk".equals(path)){packageName="com.tencent.weread.eink";sourcePath=path;if(!WEREAD_SHA.equals(digest(path)))throw new SecurityException("WeRead source digest mismatch");}
  else if(!sourcePath.equals(path))throw new SecurityException("Fixed local APKs only");
  if(Looper.myLooper()==null)Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  final Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  if("com.tencent.weread.eink".equals(packageName)){
   android.content.pm.PackageInfo archive=context.getPackageManager().getPackageArchiveInfo(path,android.content.pm.PackageManager.GET_SIGNATURES);
   if(archive==null || !packageName.equals(archive.packageName) || archive.signatures==null || archive.signatures.length!=1 || !WEREAD_CERT.equals(hex(archive.signatures[0].toByteArray())))throw new SecurityException("WeRead archive identity mismatch");
   System.out.println("WEREAD_ORIGINAL_APK_IDENTITY_VERIFIED sha256="+WEREAD_SHA+" signer="+WEREAD_CERT);
  }
  final PackageInstaller installer=context.getPackageManager().getPackageInstaller();
  final Handler handler=new Handler(Looper.getMainLooper());
  for(PackageInstaller.SessionInfo previous:installer.getMySessions()){
   if(packageName.equals(previous.getAppPackageName()) && !previous.isActive()){
    installer.abandonSession(previous.getSessionId());System.out.println("ANDROID_ABANDONED_OLD_LAB_SESSION id="+previous.getSessionId());
   }
  }
  Binder callback=new Binder(){
   @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
    if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString("android.content.IIntentSender");return true;}
    if(code!=1)return super.onTransact(code,data,reply,flags);
    if(Binder.getCallingUid()!=1000)throw new SecurityException("Unexpected installer callback caller");
    data.enforceInterface("android.content.IIntentSender");data.readInt();
    final Intent result=data.readInt()!=0?Intent.CREATOR.createFromParcel(data):null;
    if(reply!=null)reply.writeNoException();
    handler.post(new Runnable(){public void run(){try{
     if(result==null)throw new AssertionError("Missing installer result");
     int status=result.getIntExtra(PackageInstaller.EXTRA_STATUS,Integer.MIN_VALUE);
     String message=result.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
     System.out.println("ANDROID_INSTALL_SESSION_RESULT status="+status+" message="+message);
     try(PrintWriter record=new PrintWriter("/lab/install-result.txt")){record.println("status="+status+" message="+message);}
     if(status!=PackageInstaller.STATUS_SUCCESS){System.exit(51);return;}
     verifyAndLaunch(context,-1);System.exit(0);
    }catch(Throwable error){error.printStackTrace();System.exit(52);}}});
    return true;
   }
  };
  callback.attachInterface(null,"android.content.IIntentSender");
  Parcel parcel=Parcel.obtain();parcel.writeStrongBinder(callback);parcel.setDataPosition(0);IntentSender sender=IntentSender.CREATOR.createFromParcel(parcel);parcel.recycle();
  PackageInstaller.SessionParams params=new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
  params.setAppPackageName(packageName);params.setSize(new File(path).length());
  // Internal storage only; no device storage formatting or external volume management.
  Field installFlags=PackageInstaller.SessionParams.class.getField("installFlags");installFlags.setInt(params,installFlags.getInt(params)|16|2);
  int id=installer.createSession(params);System.out.println("ANDROID_INSTALL_SESSION_CREATED id="+id);
  boolean submitted=false;
  try(PackageInstaller.Session session=installer.openSession(id)){
   // -1 is the API's supported streaming mode; ext2 lacks posix_fallocate.
   try(InputStream source=new FileInputStream(path);OutputStream target=session.openWrite("base.apk",0,-1)){
    byte[] buffer=new byte[16384];int count;while((count=source.read(buffer))!=-1)target.write(buffer,0,count);session.fsync(target);
   }
   System.out.println("ANDROID_INSTALL_SESSION_BYTES_WRITTEN size="+new File(path).length());
   session.commit(sender);submitted=true;
  } finally {if(!submitted)installer.abandonSession(id);}
  handler.postDelayed(new Runnable(){public void run(){System.err.println("ANDROID_INSTALL_CALLBACK_TIMEOUT");System.exit(53);}},240000);
  Looper.loop();
 }
 public static void launchWeReadExisting(String expected)throws Exception {
  packageName="com.tencent.weread.eink";sourcePath="/lab/weread.apk";
  launchExisting(expected);
 }
 public static void launchExisting(String expected)throws Exception {
  if(android.os.Process.myUid()!=0)throw new SecurityException("Root probe required");
  int uid=Integer.parseInt(expected);if(uid<10000||uid>=20000)throw new AssertionError("Missing previous identity");
  if(Looper.myLooper()==null)Looper.prepareMainLooper();
  Class<?> thread=Class.forName("android.app.ActivityThread");Object main=thread.getMethod("systemMain").invoke(null);
  Context context=(Context)thread.getMethod("getSystemContext").invoke(main);
  verifyAndLaunch(context,uid);System.exit(0);
 }
 private static String digest(String path)throws Exception {
  java.security.MessageDigest hash=java.security.MessageDigest.getInstance("SHA-256");
  try(InputStream file=new FileInputStream(path)){byte[] buffer=new byte[16384];int n;while((n=file.read(buffer))!=-1)hash.update(buffer,0,n);}
  StringBuilder out=new StringBuilder();for(byte b:hash.digest())out.append(String.format(java.util.Locale.US,"%02x",b&255));return out.toString();
 }
 private static void verifyAndLaunch(Context context,int previousUid)throws Exception {
  if(packageName.equals("com.tencent.weread.eink")){if(HostDefaultConnectivityService.enabled())HostDefaultConnectivityService.verifyQueries(context);else OfflineConnectivityService.verifyQueries(context);NoOutputAudioQueries.verifyQueries(context);}
  ApplicationInfo info=context.getPackageManager().getApplicationInfo(packageName,0);
  if((info.flags&(ApplicationInfo.FLAG_SYSTEM|ApplicationInfo.FLAG_PERSISTENT))!=0 || !info.sourceDir.startsWith("/data/app/"))throw new AssertionError("Not an ordinary installation");
  if(previousUid>=0 && info.uid!=previousUid)throw new AssertionError("Installed UID changed");
  System.out.println("ANDROID_APK_IDENTITY_HASH_BEGIN uptime_ms="+SystemClock.uptimeMillis());
  boolean cacheAllowed=packageName.equals("com.tencent.weread.eink") && previousUid==10007 && "reading".equals(Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile"));
  String identity=cacheAllowed?ApkIdentityCache.verifiedRecord(context,info,WEREAD_SHA):null;
  boolean cached=cacheAllowed && ApkIdentityCache.hit(identity);String installed;
  if(cached){installed=WEREAD_SHA;System.out.println("ANDROID_APK_IDENTITY_CACHE_HIT full_hash_previously_verified=true signer_rechecked=true");}
  else{
   String source=digest(sourcePath);installed=digest(info.sourceDir);
   if(packageName.equals("com.tencent.weread.eink") && (!"/lab/weread.apk".equals(sourcePath) || !WEREAD_SHA.equals(source)))throw new SecurityException("WeRead source digest mismatch");
   if(!source.equals(installed))throw new AssertionError("Installed APK bytes differ");
   if(cacheAllowed)ApkIdentityCache.save(info,WEREAD_SHA,identity);
   System.out.println("ANDROID_APK_IDENTITY_FULL_HASH_VERIFIED cache_saved="+cacheAllowed);
  }
  System.out.println("ANDROID_APK_IDENTITY_HASH_END uptime_ms="+SystemClock.uptimeMillis());
  try(PrintWriter record=new PrintWriter(previousUid<0?"/lab/installed-identity.txt":"/lab/reopen-result.txt")){record.println("uid="+info.uid+" source="+info.sourceDir+" sha256="+installed);}
  PackageProcessRegistry.write(context);
  System.out.println("ANDROID_ORDINARY_PACKAGE_"+(previousUid<0?"INSTALLED":"PERSISTED")+" uid="+info.uid+" source="+info.sourceDir+" sha256="+installed);
  Intent launch=context.getPackageManager().getLaunchIntentForPackage(packageName);
  if(launch==null)throw new AssertionError("No launcher activity for "+packageName);
  launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
  System.out.println("ANDROID_INSTALLED_LAUNCH_COMPONENT="+launch.getComponent());
  Object manager=Class.forName("android.app.ActivityManagerNative").getMethod("getDefault").invoke(null);
  Method start=null;for(Method candidate:Class.forName("android.app.IActivityManager").getMethods())if(candidate.getName().equals("startActivityAsUser") && candidate.getParameterTypes().length==11){start=candidate;break;}
  if(start==null)throw new NoSuchMethodException("startActivityAsUser");
  if("com.tencent.weread.eink".equals(packageName)){
   Class<?> properties=Class.forName("android.os.SystemProperties");
   if("reading".equals(properties.getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile"))){
    String timestamp=Long.toString(SystemClock.uptimeMillis());
    properties.getMethod("set",String.class,String.class).invoke(null,"kobo.lab.weread.start_uptime",timestamp);
    if(!timestamp.equals(properties.getMethod("get",String.class).invoke(null,"kobo.lab.weread.start_uptime")))throw new AssertionError("Startup timestamp not published");
   }
  }
  Object code=start.invoke(manager,null,"android",launch,null,null,null,0,0,null,null,0);
  if(((Integer)code)<0)throw new AssertionError("Activity start failed: "+code);
  System.out.println("ANDROID_INSTALLED_ACTIVITY_START_RESULT code="+code);
  System.out.println("ANDROID_INSTALLED_ACTIVITY_LAUNCH_REQUESTED");
 }

}
