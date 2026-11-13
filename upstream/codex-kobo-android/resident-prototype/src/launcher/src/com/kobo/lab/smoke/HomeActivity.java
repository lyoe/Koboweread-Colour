package com.kobo.lab.smoke;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
/** A real minimal HOME destination for the lifecycle experiment. With the
 * resident marker present it is the resident launcher: it opens WeRead and
 * asks the host to hand the screen back to KOReader or to stop Android. */
public final class HomeActivity extends Activity {
 private static final File RESIDENT=new File("/data/local/tmp/kobo-resident/enabled");
 private static final File VERIFIED=new File("/data/local/tmp/kobo-resident/weread-verified");
 private boolean scheduled,launched,leftHome,resident;
 private TextView status;
 @Override public void onCreate(Bundle state){
  setTheme(android.R.style.Theme_Material_Light_NoActionBar);super.onCreate(state);
  resident=RESIDENT.isFile();
  if(resident){residentLayout();System.out.println("APK_HOME_ONCREATE resident=1");return;}
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.WHITE);root.setPadding(32,48,32,32);
  TextView title=new TextView(this);title.setText("Android 实验主页");title.setTextSize(26);title.setTextColor(Color.BLACK);root.addView(title);
  status=new TextView(this);status.setText("准备启动测试 APK Activity…\n\n微信读书尚未启动。");status.setTextSize(20);status.setTextColor(Color.BLACK);root.addView(status);setContentView(root);
  System.out.println("APK_HOME_ONCREATE");
 }
 private TextView button(LinearLayout parent,String text,View.OnClickListener click){
  TextView b=new TextView(this);b.setText(text);b.setTextSize(24);b.setTextColor(Color.BLACK);b.setGravity(Gravity.CENTER);b.setPadding(24,40,24,40);
  GradientDrawable frame=new GradientDrawable();frame.setColor(Color.WHITE);frame.setStroke(4,Color.BLACK);frame.setCornerRadius(12);b.setBackground(frame);
  b.setClickable(true);b.setOnClickListener(click);
  LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,LinearLayout.LayoutParams.WRAP_CONTENT);p.topMargin=40;parent.addView(b,p);return b;
 }
 private void residentLayout(){
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.WHITE);root.setPadding(48,72,48,48);
  TextView title=new TextView(this);title.setText("Android 启动器");title.setTextSize(30);title.setTextColor(Color.BLACK);root.addView(title);
  status=new TextView(this);status.setTextSize(18);status.setTextColor(Color.BLACK);status.setPadding(0,24,0,0);root.addView(status);
  button(root,"打开微信读书",new View.OnClickListener(){public void onClick(View v){openWeRead();}});
  button(root,"返回 KOReader（Android 后台保留）",new View.OnClickListener(){public void onClick(View v){request("background","正在返回 KOReader…");}});
  button(root,"关闭 Android 并返回 KOReader",new View.OnClickListener(){public void onClick(View v){request("shutdown","正在关闭 Android…");}});
  TextView help=new TextView(this);help.setTextSize(16);help.setTextColor(Color.BLACK);help.setPadding(0,48,0,0);
  help.setText("任何界面按住上翻页键 5 秒后松开：回到这个启动器。\n\n两个翻页键同时按住 2 秒后松开、长按左上角 2 秒后松开、或按电源键：返回 KOReader，Android 保留在后台。\n\n从 KOReader 菜单再次进入时无需重新启动 Android。");
  root.addView(help);setContentView(root);
 }
 private void openWeRead(){
  if(VERIFIED.isFile()){
   Intent launch=getPackageManager().getLaunchIntentForPackage("com.tencent.weread.eink");
   if(launch!=null){launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);status.setText("正在打开微信读书…");System.out.println("APK_RESIDENT_OPEN_WEREAD_DIRECT");startActivity(launch);return;}
  }
  // The first opening in a residency goes through the host's verified launch.
  request("open-weread","正在校验并打开微信读书…");
 }
 private void request(String kind,String message){
  File dir=getFilesDir(),pending=new File(dir,"resident-request.pending"),done=new File(dir,"resident-request");
  try{
   if(done.exists()){status.setText("上一个操作仍在处理，请稍候。");return;}
   try(FileOutputStream out=new FileOutputStream(pending)){out.write((kind+"\n").getBytes("UTF-8"));}
   if(!pending.renameTo(done))throw new java.io.IOException("rename failed");
   status.setText(message);System.out.println("APK_RESIDENT_REQUEST kind="+kind);
  }catch(Exception error){pending.delete();status.setText("无法发送请求："+error.getMessage());}
 }
 @Override protected void onResume(){super.onResume();
  if(resident){status.setText(VERIFIED.isFile()?"微信读书已在后台，可直接打开。":"Android 已就绪。");System.out.println("APK_RESIDENT_HOME_READY");return;}
  try {
   String profile=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile");
   if("reading".equals(profile)){status.setText("正在启动微信读书…");System.out.println("APK_READING_HOME_READY");return;}
  }catch(Exception error){throw new RuntimeException(error);}

  if(launched && leftHome){status.setText("测试 Activity 已退出并返回主页。\n应用 UID："+android.os.Process.myUid()+"\n\n微信读书尚未启动。");System.out.println("APK_ACTIVITY_RETURNED_TO_HOME");}
  if(!scheduled){scheduled=true;new Handler().postDelayed(new Runnable(){public void run(){launched=true;startActivity(new Intent(HomeActivity.this,ProbeActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}},1500);}
 }
 @Override protected void onPause(){if(launched)leftHome=true;super.onPause();}
}
