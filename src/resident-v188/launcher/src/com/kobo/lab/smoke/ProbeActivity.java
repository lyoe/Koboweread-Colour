package com.kobo.lab.smoke;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.graphics.Color;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Button;
import android.view.View;
public final class ProbeActivity extends Activity {
 @Override public void onCreate(Bundle state){
  setTheme(android.R.style.Theme_Material_Light_NoActionBar);super.onCreate(state);
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
  root.setBackgroundColor(Color.WHITE);root.setPadding(32,48,32,32);
  TextView title=new TextView(this);title.setText("APK Activity 本机测试");title.setTextSize(26);title.setTextColor(Color.BLACK);root.addView(title);
  TextView note=new TextView(this);note.setText("应用 UID："+android.os.Process.myUid()+"\n已进入真实 Activity 生命周期\n\n微信读书尚未启动。");note.setTextSize(20);note.setTextColor(Color.BLACK);root.addView(note);
  Button button=new Button(this);button.setText("退出测试 Activity");button.setSoundEffectsEnabled(false);button.setOnClickListener(new View.OnClickListener(){public void onClick(View view){finish();}});root.addView(button);
  setContentView(root);System.out.println("APK_ACTIVITY_ONCREATE");
  new Handler().postDelayed(new Runnable(){public void run(){System.out.println("APK_ACTIVITY_REQUEST_FINISH");finish();}},12000);
 }
 @Override protected void onResume(){super.onResume();System.out.println("APK_ACTIVITY_ONRESUME");}
 @Override protected void onDestroy(){super.onDestroy();System.out.println("APK_ACTIVITY_ONDESTROY");}
}
