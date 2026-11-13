// Isolated host fixture; never included in the Android runtime.
package android.os; public final class Message {public int what;public Object obj;public long when;public static Message obtain(Message original){Message copy=new Message();copy.what=original.what;copy.obj=original.obj;return copy;}}
