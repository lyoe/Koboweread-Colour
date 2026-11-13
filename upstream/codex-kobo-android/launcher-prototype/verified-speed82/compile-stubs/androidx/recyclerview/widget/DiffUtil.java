package androidx.recyclerview.widget;
public class DiffUtil {
 public abstract static class Callback {
  public abstract int getOldListSize(); public abstract int getNewListSize();
  public abstract boolean areItemsTheSame(int oldItemPosition,int newItemPosition);
  public abstract boolean areContentsTheSame(int oldItemPosition,int newItemPosition);
  public Object getChangePayload(int oldItemPosition,int newItemPosition){return null;}
 }
 public static class DiffResult {public void dispatchUpdatesTo(ListUpdateCallback c){throw new UnsupportedOperationException();}}
 public static DiffResult calculateDiff(Callback c){throw new UnsupportedOperationException();}
}
