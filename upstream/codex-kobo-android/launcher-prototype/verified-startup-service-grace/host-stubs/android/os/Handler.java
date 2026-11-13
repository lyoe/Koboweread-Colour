// Isolated host fixture; never included in the Android runtime.
package android.os;
import java.util.ArrayList;import java.util.List;
public class Handler {
 public interface Callback {boolean handleMessage(Message msg);}
 private final Callback mCallback;
 public final List<Message> queued=new ArrayList<>();public int originalCalls;
 public Handler(Callback callback){mCallback=callback;}
 public Message obtainMessage(int what,Object obj){Message m=new Message();m.what=what;m.obj=obj;return m;}
 public boolean sendMessage(Message m){return sendMessageAtTime(m,SystemClock.uptimeMillis());}
 public boolean sendMessageAtTime(Message m,long when){m.when=when;queued.add(m);return true;}
 public boolean sendMessageDelayed(Message m,long delay){throw new AssertionError("Use the fixed absolute deadline");}
 public void dispatchMessage(Message m){if(mCallback==null || !mCallback.handleMessage(m))originalCalls++;}
 public void removeMessages(int what,Object obj){queued.removeIf(m->m.what==what && m.obj==obj);}
}
