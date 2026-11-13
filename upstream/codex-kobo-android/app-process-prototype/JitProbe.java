import java.io.*;
/** Fixed arithmetic workload and the real API24 ART compiler's own counters. */
public final class JitProbe {
 private static volatile int sink;
 private static int hot(int n){int z=n;for(int j=0;j<32;j++)z=Integer.rotateLeft(z^(j*0x9e3779b9),7)+n;return z;}
 public static void main(String[] args)throws Exception {
  if(android.os.Process.myUid()!=10000||args.length!=1||!(args[0].equals("jit")||args[0].equals("interpreter")))throw new SecurityException("Fixed UID probe only");
  boolean jit=args[0].equals("jit");String path="/data/anr/traces.txt";
  try(FileOutputStream f=new FileOutputStream(path,false)){}
  for(int i=0;i<12000;i++)sink^=hot(i);
  Thread.sleep(1000);
  int value=0;long start=System.nanoTime();
  for(int i=0;i<60000;i++)value^=hot(i*17+3);
  long duration=System.nanoTime()-start;sink=value;
  int executableCacheMappings=0;
  try(BufferedReader r=new BufferedReader(new FileReader("/proc/self/maps"))){String l;while((l=r.readLine())!=null)if(l.contains("jit-code-cache")&&l.split("\\s+")[1].contains("x"))executableCacheMappings++;}
  android.os.Process.sendSignal(android.os.Process.myPid(),3);
  int compilations=0,entries=0;boolean complete=false;
  for(int attempt=0;attempt<100;attempt++){
   try(BufferedReader r=new BufferedReader(new FileReader(path))){String l;while((l=r.readLine())!=null){
    if(l.startsWith("Total number of JIT compilations: "))compilations=Integer.parseInt(l.substring(l.lastIndexOf(' ')+1));
    if(l.startsWith("Current number of JIT code cache entries: "))entries=Integer.parseInt(l.substring(l.lastIndexOf(' ')+1));
    if(l.equals("----- end "+android.os.Process.myPid()+" -----"))complete=true;
   }}
   if(complete)break;Thread.sleep(100);
  }
  System.out.println("ART_FIXED_WORKLOAD mode="+args[0]+" checksum="+value+" elapsed_ns="+duration+" compilations="+compilations+" code_entries="+entries+" executable_cache_mappings="+executableCacheMappings);
  if(!complete||(jit&&(compilations<=0||entries<=0||executableCacheMappings<=0))||(!jit&&compilations!=0))throw new AssertionError("Actual ART compiler counters did not match requested mode");
  System.out.println("ART_JIT_PROBE_COMPLETE mode="+args[0]);System.exit(0);
 }
}
