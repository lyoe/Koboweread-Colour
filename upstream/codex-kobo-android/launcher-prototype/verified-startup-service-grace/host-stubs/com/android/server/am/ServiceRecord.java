// Isolated host fixture; never included in the Android runtime.
package com.android.server.am;import android.content.ComponentName;public class ServiceRecord {public ComponentName name;public long executingStart;public ServiceRecord(String cls,long start){name=new ComponentName("com.tencent.weread.eink",cls);executingStart=start;}}
