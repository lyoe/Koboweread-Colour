import android.os.*;
import com.android.server.wm.WindowState;
public class FocusTransitionTest {
 public static class Windows {private final Object mWindowMap=new Object();private WindowState mCurrentFocus;}
 static Parcel query(LiveFocusService service,int code)throws Exception{Parcel d=new Parcel(),r=new Parcel();d.token=LiveFocusService.TOKEN;if(!service.onTransact(code,d,r,0))throw new AssertionError();return r;}
 static void check(boolean value){if(!value)throw new AssertionError();}
 static WindowState reader(){WindowState w=new WindowState();w.mAttrs.packageName="com.tencent.weread.eink";w.mAttrs.title="com.tencent.weread.eink/com.tencent.weread.ReaderFragmentActivity";return w;}
 public static void main(String[] args)throws Exception{
  Windows w=new Windows();w.mCurrentFocus=reader();LiveFocusService.install(w);LiveFocusService service=(LiveFocusService)ServiceManager.service;
  Parcel a=query(service,2);check(a.result==3&&a.stamp>0&&query(service,1).result==3&&query(service,2).stamp==a.stamp);
  w.mCurrentFocus.mInputWindowHandle.paused=true;Parcel b=query(service,2);check(b.result==0&&b.stamp>a.stamp);check(!TouchLeasePolicy.same(a.stamp,b.result,b.stamp));
  w.mCurrentFocus.mInputWindowHandle.paused=false;Parcel c=query(service,2);check(c.result==3&&c.stamp>b.stamp&&!TouchLeasePolicy.same(a.stamp,c.result,c.stamp));
  w.mCurrentFocus=reader();Parcel d=query(service,2);check(d.result==3&&d.stamp>c.stamp&&!TouchLeasePolicy.same(c.stamp,d.result,d.stamp));
  w.mCurrentFocus.mInputWindowHandle.visible=false;check(query(service,2).result==0);
  w.mCurrentFocus.mInputWindowHandle=null;check(query(service,2).result==0);
  w.mCurrentFocus=null;check(query(service,2).result==0);
  check(TouchLeasePolicy.same(d.stamp,3,d.stamp));check(!TouchLeasePolicy.same(0,3,0));check(!TouchLeasePolicy.same(d.stamp,0,d.stamp));check(!TouchLeasePolicy.same(d.stamp,4,d.stamp));
  Binder.caller=10007;try{query(service,2);throw new AssertionError();}catch(SecurityException expected){}Binder.caller=0;
  System.out.println("FOCUS88_REAL_SERVICE_PAUSED_HIDDEN_MISSING_HANDLE_NEW_WINDOW_AND_GENERATION_INVALIDATION_OK");
 }
}
