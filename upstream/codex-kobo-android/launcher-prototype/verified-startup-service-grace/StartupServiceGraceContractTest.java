import android.os.*;
import android.system.Os;
import com.android.server.am.*;
import java.io.*;
public final class StartupServiceGraceContractTest {
 public static void main(String[] args)throws Exception {
  int[] forwarded={0};Handler h=new Handler(m->{if(m.what==999){forwarded[0]++;return true;}return false;});
  TestActivityManager am=new TestActivityManager(h);ProcessRecord proc=new ProcessRecord();am.mLruProcesses.add(proc);
  ServiceRecord db=new ServiceRecord("com.tencent.weread.aidl.DBDataService",1000);proc.executingServices.add(db);
  ByteArrayOutputStream log=new ByteArrayOutputStream();PrintStream old=System.out;System.setOut(new PrintStream(log));
  try {
   StartupServiceGrace.install(am);if(h.queued.size()!=1)throw new AssertionError("probe missing");
   h.dispatchMessage(h.queued.remove(0));
   if(!log.toString("UTF-8").contains("CALLBACK_PROBE_OK") || h.originalCalls!=0)throw new AssertionError("dispatch hook not active");
   SystemClock.now=61000;Os.metadataCost=5000;
   Message timeout=h.obtainMessage(12,proc);h.dispatchMessage(timeout);
   if(h.queued.size()!=1 || h.queued.get(0).when!=121000 || h.queued.get(0).obj!=proc || h.queued.get(0).what!=12)throw new AssertionError("absolute deadline or original identity lost");
   if(proc.executingServices.size()!=1 || db.executingStart!=1000)throw new AssertionError("service completion was fabricated");
   h.removeMessages(12,proc);if(!h.queued.isEmpty())throw new AssertionError("original cancellation mismatch");
   Os.metadataCost=0;SystemClock.now=121000;h.dispatchMessage(timeout);
   if(h.originalCalls!=1 || !h.queued.isEmpty())throw new AssertionError("original timeout not restored at deadline");
   h.dispatchMessage(h.obtainMessage(999,new Object()));if(forwarded[0]!=1 || h.originalCalls!=1)throw new AssertionError("previous callback not preserved");
   SystemClock.now=61000;proc.executingServices.add(new ServiceRecord("com.tencent.weread.OtherService",1000));h.dispatchMessage(timeout);
   if(h.originalCalls!=2 || !h.queued.isEmpty())throw new AssertionError("other overdue service delayed");
   proc.executingServices.removeIf(s->s!=db);Os.marker=false;h.dispatchMessage(timeout);
   if(h.originalCalls!=3 || !h.queued.isEmpty())throw new AssertionError("grace without trusted marker");
   Os.marker=true;proc.uid=10006;h.dispatchMessage(timeout);
   if(h.originalCalls!=4 || !h.queued.isEmpty())throw new AssertionError("wrong uid delayed");
  }finally{System.setOut(old);}
  System.out.println("STARTUP_SERVICE_GRACE_CONTRACT_OK actual_dispatch_probe absolute_deadline_under_metadata_delay original_message_identity cancellation original_timeout other_messages_and_services_no_fake_completion");
 }
}
