package com.kobo.lab.smoke;
import android.app.Application;
import android.content.Intent;
import java.io.FileOutputStream;
public final class ProbeApplication extends Application {
 @Override public void onCreate(){super.onCreate();
  if(android.os.Process.myUid()<10000)throw new AssertionError("Expected application UID");
  try(FileOutputStream out=openFileOutput("application-started.txt",MODE_PRIVATE)){
   out.write(("APPLICATION_ONCREATE uid="+android.os.Process.myUid()+"\n").getBytes("UTF-8"));
  }catch(Exception error){throw new RuntimeException(error);}
  System.out.println("APK_APPLICATION_ONCREATE uid="+android.os.Process.myUid()+" data="+getFilesDir());
  final android.os.Handler handler=new android.os.Handler();
  handler.post(new Runnable(){int attempts;public void run(){try{
   String ready=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.ams.ready");
   if(!"1".equals(ready)){if(++attempts>160)throw new AssertionError("AMS readiness timeout");handler.postDelayed(this,100);return;}
   String profile=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile");
   if("reading".equals(profile)){System.out.println("APK_READING_APPLICATION_READY");return;}
   startActivity(new Intent(ProbeApplication.this,HomeActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
  }catch(Exception error){throw new RuntimeException(error);}}});
 }
}
