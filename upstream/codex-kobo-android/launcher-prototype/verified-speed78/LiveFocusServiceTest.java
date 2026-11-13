import android.os.*;
import com.android.server.wm.WindowState;
public class LiveFocusServiceTest {
 public static class Windows {private final Object mWindowMap=new Object();private WindowState mCurrentFocus;}
 static int query(LiveFocusService service)throws Exception {Parcel d=new Parcel(),r=new Parcel();d.token=LiveFocusService.TOKEN;if(!service.onTransact(1,d,r,0))throw new AssertionError();return r.result;}
 static void expect(int a,int b){if(a!=b)throw new AssertionError(a+" != "+b);}
 public static void main(String[] args)throws Exception {
  Windows w=new Windows();LiveFocusService.install(w);LiveFocusService b=(LiveFocusService)ServiceManager.service;
  expect(query(b),0);WindowState s=new WindowState();w.mCurrentFocus=s;s.mAttrs.packageName="com.tencent.weread.eink";s.mAttrs.title="com.tencent.weread.eink/com.tencent.weread.ReaderFragmentActivity";
  expect(query(b),3);s.mOwnerUid=10000;expect(query(b),0);s.mOwnerUid=10007;s.mAttrs.packageName="com.tencent.weread.eink.evil";expect(query(b),0);
  s.mAttrs.packageName="com.tencent.weread.eink";s.mAttrs.title="PopupWindow";expect(query(b),1);s.mAttrs.title="com.tencent.weread.eink/com.tencent.weread.WeReadFragmentActivity";expect(query(b),2);
  Binder.caller=10007;try{query(b);throw new AssertionError();}catch(SecurityException expected){}Binder.caller=0;
  Parcel d=new Parcel(),r=new Parcel();d.token="wrong";try{b.onTransact(1,d,r,0);throw new AssertionError();}catch(SecurityException expected){}
  d.token=LiveFocusService.TOKEN;d.extra=1;try{b.onTransact(1,d,r,0);throw new AssertionError();}catch(IllegalArgumentException expected){}
  w.mCurrentFocus=null;expect(query(b),0);
  System.out.println("LIVE_FOCUS_BOUNDARIES_OK actual_service null_focus changed_owner wrong_package popup root_only token trailing_args uncached");
 }
}
