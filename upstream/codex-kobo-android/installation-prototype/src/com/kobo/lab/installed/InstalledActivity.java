package com.kobo.lab.installed;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.graphics.Color;
import android.widget.TextView;
import java.io.FileOutputStream;
public final class InstalledActivity extends Activity {
 @Override public void onCreate(Bundle state){
  setTheme(android.R.style.Theme_Material_Light_NoActionBar);super.onCreate(state);
  int uid=android.os.Process.myUid();
  if(uid<10000 || (getApplicationInfo().flags&1)!=0)throw new AssertionError("Expected ordinary installed app");
  try(FileOutputStream file=openFileOutput("installed-app-started.txt",MODE_PRIVATE)){file.write(("INSTALLED_APK uid="+uid+" source="+getApplicationInfo().sourceDir+"\n").getBytes("UTF-8"));}catch(Exception e){throw new RuntimeException(e);}
  android.content.SharedPreferences preferences=getSharedPreferences("lifecycle",MODE_PRIVATE);
  int launches=preferences.getInt("launches",0)+1;
  if(!preferences.edit().putInt("launches",launches).commit())throw new AssertionError("Private state commit failed");
  System.out.println("INSTALLED_APK_PRIVATE_LAUNCH_COUNT="+launches);
  TextView view=new TextView(this);view.setBackgroundColor(Color.WHITE);view.setTextColor(Color.BLACK);view.setTextSize(24);view.setPadding(32,48,32,32);
  view.setText("普通 APK 安装测试\n\n应用 UID："+uid+"\n启动次数："+launches+"\n来源：/data/app\n\n通过 Android 安装接口安装。\n微信读书尚未启动。");setContentView(view);
  System.out.println("INSTALLED_APK_ONCREATE uid="+uid+" source="+getApplicationInfo().sourceDir);
  new Handler().postDelayed(new Runnable(){public void run(){finish();}},12000);
 }
 @Override protected void onResume(){super.onResume();System.out.println("INSTALLED_APK_ONRESUME");}
 @Override protected void onDestroy(){super.onDestroy();System.out.println("INSTALLED_APK_ONDESTROY");}
}
