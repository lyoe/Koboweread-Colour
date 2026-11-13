import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
/** Match the actual focused WindowState, not a package-looking window title. */
public final class WindowOwnership {
 public static boolean isVerifiedWeRead(String current,Reader snapshot)throws IOException {
  String prefix="mCurrentFocus=";
  try(BufferedReader in=new BufferedReader(snapshot)){
   if(current==null || !current.startsWith(prefix+"Window{"))return false;
   String window=current.substring(prefix.length());boolean selected=false;String line;
   while((line=in.readLine())!=null){String item=line.trim();
    if(item.startsWith("Window #")){selected=item.endsWith(" "+window+":");continue;}
    if(selected && item.startsWith("mOwnerUid="))return item.startsWith("mOwnerUid=10007 ") && (" "+item+" ").contains(" package=com.tencent.weread.eink ");
   }
   return false;
  }
 }
}
