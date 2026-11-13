package com.kobo.lab.smoke;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.content.Intent;
import android.graphics.Color;
import android.widget.LinearLayout;
import android.widget.TextView;
/** A real minimal HOME destination for the lifecycle experiment. */
public final class HomeActivity extends Activity {
 private boolean scheduled,launched,leftHome;
 private TextView status;
 @Override public void onCreate(Bundle state){
  setTheme(android.R.style.Theme_Material_Light_NoActionBar);super.onCreate(state);
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.WHITE);root.setPadding(32,48,32,32);
  TextView title=new TextView(this);title.setText("Android 实验主页");title.setTextSize(26);title.setTextColor(Color.BLACK);root.addView(title);
  status=new TextView(this);status.setText("准备启动测试 APK Activity…\n\n微信读书尚未启动。");status.setTextSize(20);status.setTextColor(Color.BLACK);root.addView(status);setContentView(root);
  System.out.println("APK_HOME_ONCREATE");
 }
 @Override protected void onResume(){super.onResume();
  try {
   String profile=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile");
   if("reading".equals(profile)){status.setText("正在启动微信读书…");System.out.println("APK_READING_HOME_READY");return;}
  }catch(Exception error){throw new RuntimeException(error);}

  if(launched && leftHome){status.setText("测试 Activity 已退出并返回主页。\n应用 UID："+android.os.Process.myUid()+"\n\n微信读书尚未启动。");System.out.println("APK_ACTIVITY_RETURNED_TO_HOME");}
  if(!scheduled){scheduled=true;new Handler().postDelayed(new Runnable(){public void run(){launched=true;startActivity(new Intent(HomeActivity.this,ProbeActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}},1500);}
 }
 @Override protected void onPause(){if(launched)leftHome=true;super.onPause();}
}
