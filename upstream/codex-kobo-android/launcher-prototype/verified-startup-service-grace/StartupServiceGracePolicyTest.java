public final class StartupServiceGracePolicyTest {
 static long check(long now,long start,boolean marker,int uid,String process,boolean fg,boolean db,boolean other){return StartupServiceGracePolicy.delay(now,start,marker,uid,process,fg,db,other);}
 public static void main(String[] args){
  String app="com.tencent.weread.eink";
  if(check(61000,1000,true,10007,app,true,true,false)!=60000)throw new AssertionError("remaining deadline");
  if(check(120999,1000,true,10007,app,true,true,false)!=1)throw new AssertionError("last millisecond");
  if(check(121000,1000,true,10007,app,true,true,false)!=0)throw new AssertionError("deadline not restored");
  if(check(181000,1000,true,10007,app,true,true,false)!=0)throw new AssertionError("expired");
  if(check(61000,1000,false,10007,app,true,true,false)!=0)throw new AssertionError("no marker");
  if(check(61000,1000,true,10006,app,true,true,false)!=0)throw new AssertionError("wrong uid");
  if(check(61000,1000,true,10007,app+":seg",true,true,false)!=0)throw new AssertionError("other process");
  if(check(61000,1000,true,10007,app,false,true,false)!=0)throw new AssertionError("background");
  if(check(61000,1000,true,10007,app,true,false,false)!=0)throw new AssertionError("no overdue DB");
  if(check(61000,1000,true,10007,app,true,true,true)!=0)throw new AssertionError("other overdue service");
  if(check(1,1000,true,10007,app,true,true,false)!=0 || check(61000,0,true,10007,app,true,true,false)!=0)throw new AssertionError("invalid clock");
  System.out.println("STARTUP_SERVICE_GRACE_POLICY_OK fixed_uid_process_service finite_deadline no_background_or_other_service_grace");
 }
}
