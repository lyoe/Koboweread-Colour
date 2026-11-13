// Isolated host fixture; never included in the Android runtime.
package com.android.server.am;import java.util.*;public class ProcessRecord {public int uid=10007,pid=42;public String processName="com.tencent.weread.eink";public Object thread=new Object();public boolean execServicesFg=true;public final Collection<ServiceRecord> executingServices=new ArrayList<>();}
