import android.system.Os;
import android.system.ErrnoException;
import java.io.*;
/** Fixed harmless file under the diagnostic directory; never opens application data. */
public final class SyncWaitSelfTest {
 public static void main(String[] args)throws Exception {
  if(args.length!=0||android.os.Process.myUid()!=10007)throw new SecurityException("Fixed UID self-test only");
  File file=new File("/lab/sync-wait/testdata/probe.bin");
  if(!file.createNewFile())throw new IOException("Existing test file");
  try(FileOutputStream stream=new FileOutputStream(file)){
   stream.write(new byte[4096]);
   stream.getFD().sync();
   Os.fdatasync(stream.getFD());
  }finally{if(!file.delete())throw new IOException("Cannot remove test file");}
  boolean rejected=false;
  try{Os.fsync(new FileDescriptor());}catch(ErrnoException expected){if(expected.errno!=9)throw expected;rejected=true;}
  if(!rejected)throw new AssertionError("Invalid descriptor accepted");
  System.out.println("SYNC_WAIT_UID_FILE_SYNC_FDATASYNC_EBADF_SELFTEST_PASS");
  System.exit(0);
 }
}
