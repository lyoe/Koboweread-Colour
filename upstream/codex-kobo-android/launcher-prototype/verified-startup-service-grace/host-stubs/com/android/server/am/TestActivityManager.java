// Isolated host fixture; never included in the Android runtime.
package com.android.server.am;import java.util.*;import android.os.Handler;public class TestActivityManager {public static int SERVICE_TIMEOUT_MSG=12;public final Handler mHandler;public final Collection<ProcessRecord> mLruProcesses=new ArrayList<>();public TestActivityManager(Handler h){mHandler=h;}}
