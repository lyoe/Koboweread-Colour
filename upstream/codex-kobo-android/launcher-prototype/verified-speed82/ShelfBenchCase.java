import android.os.SystemClock;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListUpdateCallback;
public class ShelfBenchCase {
 static final class Lists extends DiffUtil.Callback {
  final int size,rotation;final boolean replace,changes;
  Lists(int n,int r,boolean p,boolean c){size=n;rotation=r;replace=p;changes=c;}
  public int getOldListSize(){return size;}
  public int getNewListSize(){return size;}
  int after(int j){return replace?size+j:(j+rotation)%size;}
  public boolean areItemsTheSame(int i,int j){return i==after(j);}
  public boolean areContentsTheSame(int i,int j){return !changes||i%17!=0;}
 }
 static final class Count implements ListUpdateCallback {
  int inserted,removed,moved,changed;
  public void onInserted(int p,int n){inserted+=n;}
  public void onRemoved(int p,int n){removed+=n;}
  public void onMoved(int a,int b){moved++;}
  public void onChanged(int p,int n,Object payload){changed+=n;}
 }
 public static void run(){
  for(int round=0;round<3;round++){
   one("unchanged",new Lists(384,0,false,false),round);
   one("rotate",new Lists(384,97,false,true),round);
   one("replace",new Lists(192,0,true,false),round);
  }
  try {
   boolean jit=false;int oat=0;
   java.io.BufferedReader maps=new java.io.BufferedReader(new java.io.FileReader("/proc/self/maps"));String line;
   while((line=maps.readLine())!=null){if(line.contains("jit-code-cache")||line.contains("jit-cache"))jit=true;if(line.contains("r-xp")&&(line.contains("com.tencent.weread.eink-1/oat/arm/base.odex")||line.contains("weread-selected82.odex")))oat++;}maps.close();
   String configured=(String)Class.forName("android.os.SystemProperties").getMethod("get",String.class).invoke(null,"dalvik.vm.usejit");
   System.out.println("SHELF_BENCH82_VM configured_jit="+configured+" jit_mapping="+jit+" oat_exec_mappings="+oat);
  }catch(Exception error){throw new AssertionError(error);}
  System.out.println("SHELF_BENCH82_OK synthetic_only=true real_library_data=false");
 }
 static void one(String name,Lists callback,int round){
  long start=SystemClock.elapsedRealtimeNanos();
  DiffUtil.DiffResult result=DiffUtil.calculateDiff(callback);Count count=new Count();result.dispatchUpdatesTo(count);
  long elapsed=SystemClock.elapsedRealtimeNanos()-start;
  int expectedChanged=callback.changes?(callback.size+16)/17:0;
  if(callback.replace){if(count.inserted!=callback.size||count.removed!=callback.size||count.moved!=0||count.changed!=0)throw new AssertionError("Replace update mismatch");}
  else if(count.inserted!=0||count.removed!=0||count.changed!=expectedChanged||count.moved!=(callback.rotation==0?0:callback.rotation))throw new AssertionError("Move/content update mismatch");
  System.out.println("SHELF_BENCH82 case="+name+" round="+round+" elapsed_ns="+elapsed+" inserted="+count.inserted+" removed="+count.removed+" moved="+count.moved+" changed="+count.changed);
 }
}
