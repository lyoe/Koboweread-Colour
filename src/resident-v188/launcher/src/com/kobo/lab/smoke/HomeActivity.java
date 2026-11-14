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
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.util.HashMap;
import java.util.Map;
/** A real minimal HOME destination for the lifecycle experiment. With the
 * resident marker present it is the resident launcher: it opens WeRead and
 * asks the host to hand the screen back to KOReader or to stop Android. */
public final class HomeActivity extends Activity {
 private static final File RESIDENT=new File("/data/local/tmp/kobo-resident/enabled");
 private static final File VERIFIED=new File("/data/local/tmp/kobo-resident/weread-verified");
 // Written by the host from the first (verified) WeRead launch of a residency
 // until WeRead is displayed, e.g. the prewarm right after this launcher
 // appears (~20 s).
 private static final File STARTING=new File("/data/local/tmp/kobo-resident/weread-starting");
 private static final String STARTING_TEXT="微信读书正在启动（约 20 秒），完成后自动打开。";
 private static final String STARTING_PAUSED_TEXT="微信读书正在启动（约 20 秒），请稍候。";
 private boolean openPending,homeResumed;private int pendingTicks;
 private final Handler handler=new Handler();
 // Press feedback: a pressed button turns black (white text) until its action
 // finishes, the launcher is shown again, or 3 s pass without a result. A
 // short flash could be missed by the e-ink refresh, so the state is held.
 private final java.util.List<TextView> buttons=new java.util.ArrayList<TextView>();
 private TextView openButton;
 private static void look(TextView b,boolean pressed){
  if(b==null)return;
  GradientDrawable frame=new GradientDrawable();frame.setColor(pressed?Color.BLACK:Color.WHITE);frame.setStroke(4,Color.BLACK);frame.setCornerRadius(12);
  b.setBackground(frame);b.setTextColor(pressed?Color.WHITE:Color.BLACK);
 }
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
  final TextView b=new TextView(this);b.setText(text);b.setTextSize(24);b.setGravity(Gravity.CENTER);b.setPadding(24,40,24,40);
  look(b,false);
  final View.OnClickListener action=click;
  b.setClickable(true);b.setOnClickListener(new View.OnClickListener(){public void onClick(View v){
   look(b,true);action.onClick(v);
   handler.postDelayed(new Runnable(){public void run(){if(b!=openButton||!openPending)look(b,false);}},3000);
  }});
  buttons.add(b);
  LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,LinearLayout.LayoutParams.WRAP_CONTENT);p.topMargin=40;parent.addView(b,p);return b;
 }
 private void residentLayout(){
  tuningShown=false;buttons.clear();
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.WHITE);root.setPadding(48,72,48,48);
  TextView title=new TextView(this);title.setText("Android 启动器");title.setTextSize(30);title.setTextColor(Color.BLACK);root.addView(title);
  status=new TextView(this);status.setTextSize(18);status.setTextColor(Color.BLACK);status.setPadding(0,24,0,0);root.addView(status);
  openButton=button(root,"打开微信读书",new View.OnClickListener(){public void onClick(View v){openWeRead();}});
  button(root,"阅读优化",new View.OnClickListener(){public void onClick(View v){tuningLayout();}});
  button(root,"返回 KOReader（Android 后台保留）",new View.OnClickListener(){public void onClick(View v){request("background","正在返回 KOReader…");}});
  button(root,"关闭 Android 并返回 KOReader",new View.OnClickListener(){public void onClick(View v){request("shutdown","正在关闭 Android…");}});
  TextView help=new TextView(this);help.setTextSize(16);help.setTextColor(Color.BLACK);help.setPadding(0,48,0,0);
  help.setText("任何界面按住上翻页键 5 秒后松开：回到这个启动器。\n\n两个翻页键同时按住 2 秒后松开、长按左上角 2 秒后松开、或按电源键：返回 KOReader，Android 保留在后台。\n\n从 KOReader 菜单再次进入时无需重新启动 Android。");
  root.addView(help);setContentView(root);
 }

 private void residentStatus(){status.setText(openPending||STARTING.isFile()?STARTING_TEXT:VERIFIED.isFile()?"微信读书已在后台，可直接打开。":"Android 已就绪。");}
 // 阅读优化 (2026-11-11, docs/1.9.7版本.md "启动器：阅读优化"): WeRead tuning
 // switches, display profile, restart and odex compile. The host keeps the
 // current settings in TUNING (and what this session started with in
 // TUNING_START); one request "set-<y|n per TUNES>-<c|r|t>" carries the whole
 // wanted state, so only the latest wins. "restart" / "compile" end the
 // session; the host starts it again (compiling first) and WeRead reopens.
 private static final File TUNING=new File("/data/local/tmp/kobo-resident/tuning");
 private static final File TUNING_START=new File("/data/local/tmp/kobo-resident/tuning-start");
 private static final String[][] TUNES={
  {"syncthrottle","同步限频","书友想法、热门划线同一本书 12 小时更新一次"},
  {"markusers","热门划线用户缓存","同一次运行里再开同一本书更省电"},
  {"chapterprefetch","跳章预排版","停留 45 秒后排好前后 3 章，跳章更快"},
  {"bookprefetch","充电时整本预排版","接着电、停在一页 2 分钟后排完全书"},
  {"fontcache","字号缓存","翻页、退书更快"},
  {"skia","字形缓存 16 MB","新页快约 0.2 秒，多用最多约 14 MB 内存"},
  {"segdict","关书后释放分词词典","可用内存多约 20 MB"},
  {"seglow","分词进程低优先级","把 CPU 让给翻页"},
  {"poollow","后台任务低优先级","联网、写库让给翻页"},
  {"zram","内存压缩（zram）","相当于多约 30 MB 内存"},
  {"idle-suspend","超时自动休眠","电池供电时 5 分钟无操作后休眠，唤醒切回微信读书"},
  {"page-wipe","实体键翻页动画","试验功能，实体翻页键触发分区渐显"},
  {"aosp-pinyin","AOSP 离线拼音","启用已安装的拼音键盘，重启微信读书后生效"}};
 private static final String[][] DISPLAYS={{"classic","标准（原显示方式）","c"},{"reagl","REAGL（减少残影）","r"},{"reagl-text","REAGL＋文字增强","t"}};
 private boolean tuningShown,tuneLoaded;
 private final boolean[] want=new boolean[TUNES.length],started=new boolean[TUNES.length];
 private String wantDisplay="reagl-text",startDisplay="reagl-text",sentWord,actionWord,compileInfo="";
 private final TextView[] tuneBoxes=new TextView[TUNES.length],tuneNotes=new TextView[TUNES.length],displayBoxes=new TextView[DISPLAYS.length];
 private TextView tuneStatus,restartButton,compileButton;
 private long armedAt;private TextView armed;
 // KOReader's 阅读优化 menu item: the host writes a new open-tuning marker;
 // each one opens the page once (the last one seen is kept in files/).
 private static final File OPEN_TUNING=new File("/data/local/tmp/kobo-resident/open-tuning");
 private boolean openTuningRequested(){
  String mark=readLine(OPEN_TUNING);if(mark==null)return false;
  File seen=new File(getFilesDir(),"open-tuning.seen");
  if(mark.equals(readLine(seen)))return false;
  try(FileOutputStream out=new FileOutputStream(seen)){out.write((mark+"\n").getBytes("UTF-8"));}catch(Exception error){return false;}
  System.out.println("APK_RESIDENT_OPEN_TUNING");return true;
 }
 private static String readLine(File f){
  try(java.io.BufferedReader r=new java.io.BufferedReader(new java.io.InputStreamReader(new java.io.FileInputStream(f),"UTF-8"))){return r.readLine();}catch(Exception error){return null;}
 }
 private static Map<String,String> readTuning(File f){
  Map<String,String> m=new HashMap<String,String>();
  try(java.io.BufferedReader r=new java.io.BufferedReader(new java.io.InputStreamReader(new java.io.FileInputStream(f),"UTF-8"))){
   for(String line;(line=r.readLine())!=null;){int sp=line.indexOf(' ');if(sp>0)m.put(line.substring(0,sp),line.substring(sp+1).trim());}
  }catch(Exception error){return null;}
  return m;
 }
 private static void box(TextView b,boolean on){
  GradientDrawable frame=new GradientDrawable();frame.setColor(on?Color.BLACK:Color.WHITE);frame.setStroke(3,Color.BLACK);frame.setCornerRadius(10);
  b.setBackground(frame);b.setTextColor(on?Color.WHITE:Color.BLACK);
 }
 private TextView text(LinearLayout parent,String value,float size,int top){
  TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(Color.BLACK);t.setPadding(0,top,0,0);parent.addView(t);return t;
 }
 private void line(LinearLayout parent){
  View v=new View(this);v.setBackgroundColor(Color.GRAY);parent.addView(v,new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,2));
 }
 private TextView row(LinearLayout parent,String title,String note,View.OnClickListener click,TextView[] notes,int index){
  LinearLayout r=new LinearLayout(this);r.setOrientation(LinearLayout.HORIZONTAL);r.setGravity(Gravity.CENTER_VERTICAL);r.setPadding(0,14,0,14);
  LinearLayout left=new LinearLayout(this);left.setOrientation(LinearLayout.VERTICAL);
  text(left,title,19,0);
  if(note!=null){TextView n=text(left,note,14,4);n.setTextColor(0xFF333333);if(notes!=null)notes[index]=n;}
  r.addView(left,new LinearLayout.LayoutParams(0,LinearLayout.LayoutParams.WRAP_CONTENT,1));
  TextView b=new TextView(this);b.setTextSize(18);b.setGravity(Gravity.CENTER);b.setPadding(0,10,0,10);
  LinearLayout.LayoutParams bp=new LinearLayout.LayoutParams(130,LinearLayout.LayoutParams.WRAP_CONTENT);bp.leftMargin=24;r.addView(b,bp);
  r.setClickable(true);r.setOnClickListener(click);
  parent.addView(r);line(parent);return b;
 }
 private void tuningLayout(){
  Map<String,String> cur=readTuning(TUNING),start=readTuning(TUNING_START);
  if(start==null)start=cur;
  if(cur!=null&&!tuneLoaded){
   for(int i=0;i<TUNES.length;i++){want[i]=!"0".equals(cur.get(TUNES[i][0]));started[i]=!"0".equals(start.get(TUNES[i][0]));}
   if(cur.get("display")!=null)wantDisplay=cur.get("display");if(start.get("display")!=null)startDisplay=start.get("display");
   sentWord=tuneWord();tuneLoaded=true;
  }
  if(cur!=null)compileInfo=compileText(cur.get("compile"));
  tuningShown=true;buttons.clear();armed=null;
  ScrollView sv=new ScrollView(this);sv.setBackgroundColor(Color.WHITE);
  LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(48,48,48,48);sv.addView(root);
  LinearLayout head=new LinearLayout(this);head.setOrientation(LinearLayout.HORIZONTAL);head.setGravity(Gravity.CENTER_VERTICAL);
  TextView title=new TextView(this);title.setText("阅读优化");title.setTextSize(28);title.setTextColor(Color.BLACK);
  head.addView(title,new LinearLayout.LayoutParams(0,LinearLayout.LayoutParams.WRAP_CONTENT,1));
  TextView back=new TextView(this);back.setText("返回");back.setTextSize(18);back.setGravity(Gravity.CENTER);back.setPadding(36,12,36,12);look(back,false);
  back.setClickable(true);back.setOnClickListener(new View.OnClickListener(){public void onClick(View v){residentLayout();residentStatus();}});head.addView(back);
  root.addView(head);
  tuneStatus=text(root,cur==null?"暂时读不到当前设置，请重启 Android 后再试。":"点按切换，立即保存；重启微信读书后生效。",15,16);
  text(root,"",6,0);line(root);
  if(cur!=null){
   for(int i=0;i<TUNES.length;i++){final int k=i;
    if(!cur.containsKey(TUNES[i][0]))continue; // not in this WeRead version
    tuneBoxes[i]=row(root,TUNES[i][1],TUNES[i][2],new View.OnClickListener(){public void onClick(View v){want[k]=!want[k];refreshTuning();sendTune();}},tuneNotes,i);
   }
   text(root,"显示效果",19,28);text(root,"",6,0);line(root);
   for(int i=0;i<DISPLAYS.length;i++){final int k=i;
    displayBoxes[i]=row(root,DISPLAYS[i][1],null,new View.OnClickListener(){public void onClick(View v){wantDisplay=DISPLAYS[k][0];refreshTuning();sendTune();}},null,i);
   }
  }
  restartButton=button(root,"重启微信读书（设置随之生效）",new View.OnClickListener(){public void onClick(View v){confirm(restartButton,"再点一次：重启微信读书（约 1 分钟）","restart");}});
  compileButton=button(root,"重新编译加速文件并重启",new View.OnClickListener(){public void onClick(View v){confirm(compileButton,"再点一次：编译并重启（约 3 分钟）","compile");}});
  text(root,"编译用的是阅读时记录的常用代码，读过一段时间后再编译效果更好。"+compileInfo,14,16).setTextColor(0xFF333333);
  setContentView(sv);refreshTuning();
  System.out.println("APK_RESIDENT_TUNING_SHOWN loaded="+(cur!=null));
 }
 private static String compileText(String c){
  if(c==null||c.startsWith("none"))return "";
  String[] p=c.split(" ");
  if(p.length>=4&&"0".equals(p[0]))return "\n上次编译："+p[3].replace('_',' ')+"，成功，"+p[1]+" MB，用时 "+p[2]+" 秒。";
  return "\n上次编译"+(p.length>=4?"（"+p[3].replace('_',' ')+"）":"")+"没有完成，原来的加速文件仍在用。";
 }
 private void refreshTuning(){
  for(int i=0;i<TUNES.length;i++){
   if(tuneBoxes[i]==null)continue;
   tuneBoxes[i].setText(want[i]?"开":"关");box(tuneBoxes[i],want[i]);
   tuneNotes[i].setText(TUNES[i][2]+(want[i]!=started[i]?"（已改，重启后生效）":""));
  }
  for(int i=0;i<DISPLAYS.length;i++){
   if(displayBoxes[i]==null)continue;
   boolean on=DISPLAYS[i][0].equals(wantDisplay);
   displayBoxes[i].setText(on?"✓":(DISPLAYS[i][0].equals(startDisplay)?"当前":""));box(displayBoxes[i],on);
  }
 }
 private String tuneWord(){
  StringBuilder b=new StringBuilder("set-");
  for(boolean w:want)b.append(w?'y':'n');
  String d="t";for(String[] x:DISPLAYS)if(x[0].equals(wantDisplay))d=x[2];
  return b.append('-').append(d).toString();
 }
 // Two taps (within 5 s) for the actions that end the session.
 private void confirm(TextView b,String prompt,String action){
  long now=android.os.SystemClock.uptimeMillis();
  if(armed!=b||now-armedAt>5000){
   if(armed!=null)armed.setText(armed==restartButton?"重启微信读书（设置随之生效）":"重新编译加速文件并重启");
   armed=b;armedAt=now;b.setText(prompt);return;
  }
  armed=null;actionWord=action;b.setText(action.equals("compile")?"正在编译并重启…":"正在重启…");
  tuneStatus.setText(action.equals("compile")?"正在关闭 Android，编译约一两分钟，完成后自动打开微信读书。":"正在重启微信读书，约 1 分钟后自动打开。");
  sendTune();
 }
 private void sendTune(){handler.removeCallbacks(tuneSender);handler.post(tuneSender);}
 private final Runnable tuneSender=new Runnable(){public void run(){
  File dir=getFilesDir(),pending=new File(dir,"resident-request.pending"),done=new File(dir,"resident-request");
  if(done.exists()){handler.postDelayed(this,300);return;}
  String w=tuneWord(),send=null;
  if(!w.equals(sentWord))send=w;else if(actionWord!=null){send=actionWord;actionWord=null;}
  if(send==null){handler.postDelayed(tuneCheck,1000);return;}
  try{
   try(FileOutputStream out=new FileOutputStream(pending)){out.write((send+"\n").getBytes("UTF-8"));}
   if(!pending.renameTo(done))throw new java.io.IOException("rename failed");
   if(send.startsWith("set-"))sentWord=send;
   System.out.println("APK_RESIDENT_REQUEST kind="+send);
  }catch(Exception error){pending.delete();if(tuneStatus!=null)tuneStatus.setText("无法保存设置："+error.getMessage());return;}
  handler.postDelayed(this,300);
 }};
 // After the host took the last request: does its state file match?
 private final Runnable tuneCheck=new Runnable(){public void run(){
  if(!tuningShown||tuneStatus==null||actionWord!=null)return;
  Map<String,String> cur=readTuning(TUNING);boolean ok=cur!=null&&wantDisplay.equals(cur.get("display"));
  for(int i=0;ok&&i<TUNES.length;i++)ok=!cur.containsKey(TUNES[i][0])||want[i]==!"0".equals(cur.get(TUNES[i][0]));
  boolean changed=!wantDisplay.equals(startDisplay);for(int i=0;i<TUNES.length;i++)changed|=tuneBoxes[i]!=null&&want[i]!=started[i];
  if(restartButton!=null&&restartButton.getText().toString().startsWith("正在"))return;
  tuneStatus.setText(!ok?"设置还没保存好，请稍候再看。":changed?"已保存。点下面“重启微信读书”后生效。":"已保存，与当前运行的设置相同。");
 }};
 @Override public void onBackPressed(){if(tuningShown){residentLayout();residentStatus();return;}super.onBackPressed();}
 @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);if(tuningShown){residentLayout();residentStatus();}}
 // A press while WeRead is still starting waits for it: once the host removes
 // STARTING, WeRead is opened directly if the launch was verified; otherwise
 // the press can be repeated.
 private final Runnable pendingCheck=new Runnable(){public void run(){
  if(!openPending)return;
  // WeRead normally comes to the front by itself when it is ready (this
  // launcher is then paused): starting it again from here would relaunch its
  // entry activity on top of the reader. Open it only if the launcher is
  // still what the user sees.
  if(!STARTING.isFile()&&VERIFIED.isFile()){openPending=false;look(openButton,false);if(homeResumed)openWeRead();else System.out.println("APK_RESIDENT_OPEN_PENDING_ALREADY_FRONT");return;}
  if(!STARTING.isFile()||++pendingTicks>150){openPending=false;look(openButton,false);status.setText("微信读书未能启动，请再按一次“打开微信读书”。");System.out.println("APK_RESIDENT_OPEN_PENDING_GAVE_UP");return;}
  handler.postDelayed(this,1000);
 }};
 private void openWeRead(){
  if(STARTING.isFile()){
   status.setText(STARTING_TEXT);System.out.println("APK_RESIDENT_OPEN_PENDING_STARTING");
   if(!openPending){openPending=true;pendingTicks=0;handler.postDelayed(pendingCheck,1000);}
   return;
  }
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
 @Override protected void onResume(){super.onResume();homeResumed=true;
  for(TextView b:buttons)if(b!=openButton||!openPending)look(b,false);
  if(resident){if(openTuningRequested())tuningLayout();else if(!tuningShown)residentStatus();System.out.println("APK_RESIDENT_HOME_READY");return;}
  try {
   String profile=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"kobo.lab.startup.profile");
   if("reading".equals(profile)){status.setText("正在启动微信读书…");System.out.println("APK_READING_HOME_READY");return;}
  }catch(Exception error){throw new RuntimeException(error);}

  if(launched && leftHome){status.setText("测试 Activity 已退出并返回主页。\n应用 UID："+android.os.Process.myUid()+"\n\n微信读书尚未启动。");System.out.println("APK_ACTIVITY_RETURNED_TO_HOME");}
  if(!scheduled){scheduled=true;new Handler().postDelayed(new Runnable(){public void run(){launched=true;startActivity(new Intent(HomeActivity.this,ProbeActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}},1500);}
 }
 // The prewarm launch takes input focus from this launcher at once, while its
 // picture stays on screen until WeRead draws (~20 s): say so on it.
 @Override protected void onPause(){homeResumed=false;if(resident&&STARTING.isFile()){status.setText(STARTING_PAUSED_TEXT);System.out.println("APK_RESIDENT_PAUSED_STARTING");}if(launched)leftHome=true;super.onPause();}
}
