/** A gesture never crosses an unavailable, paused or replaced window. */
public final class TouchLeasePolicy {
 private TouchLeasePolicy(){}
 public static boolean same(long began,int currentKind,long currentGeneration){
  return began>0 && currentKind>0 && currentKind<=4 && currentGeneration==began;
 }
}
