import java.nio.file.*;
import java.io.*;
import java.util.regex.*;
public class WindowOwnershipTest {
 static void check(String focus,String dump,boolean expected)throws Exception {
  if(WindowOwnership.isVerifiedWeRead(focus,new StringReader(dump))!=expected)throw new AssertionError("Focus ownership mismatch");
 }
 public static void main(String[] args)throws Exception {
  String dump=new String(Files.readAllBytes(Paths.get(args[0])),"UTF-8");
  Matcher m=Pattern.compile("(?m)^  Window #\\d+ (Window\\{[^\\n]+ReaderFragmentActivity\\}):$").matcher(dump);
  if(!m.find())throw new AssertionError("Expected actual reader WindowState fixture");
  String focus="mCurrentFocus="+m.group(1);
  check(focus,dump,true);
  check("mCurrentFocus=null",dump,false);
  check(focus,dump.replace("mOwnerUid=10007 ","mOwnerUid=10000 "),false);
  check(focus,dump.replace("package=com.tencent.weread.eink ","package=com.tencent.weread.eink.evil "),false);
  check("mCurrentFocus=Window{missing u0 com.tencent.weread.eink/com.tencent.weread.ReaderFragmentActivity}",dump,false);
  check(focus,dump.replace("mOwnerUid=10007 ","mOwnerUid=100070 "),false);
  check(focus,dump.replace("package=com.tencent.weread.eink ","package=com.other.app "),false);
  System.out.println("ACTUAL_WINDOW_STATE_OWNERSHIP_FIXTURE_CHECKS_OK cases=7 including_null_focus_and_spoofed_identity");
 }
}
