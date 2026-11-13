/** Finite launch-only grace for the single verified WeRead DB service. */
public final class StartupServiceGracePolicy {
 public static final long LIMIT_MILLIS=120000;
 public static long delay(long now,long launch,boolean trustedMarker,int uid,String process,
                          boolean foreground,boolean dbOverdue,boolean otherOverdue){
  if(!trustedMarker || uid!=10007 || !"com.tencent.weread.eink".equals(process)
      || !foreground || !dbOverdue || otherOverdue || launch<=0 || now<launch)return 0;
  long age=now-launch;
  return age<LIMIT_MILLIS?LIMIT_MILLIS-age:0;
 }
}
