import android.view.InputDevice;
import android.view.MotionEvent;
/** Complete single-finger metadata for the fixed physical-input probes. */
public final class TouchEvents {
 public static MotionEvent create(long down,long time,int action,int x,int y){
  MotionEvent.PointerProperties properties=new MotionEvent.PointerProperties();properties.id=0;properties.toolType=MotionEvent.TOOL_TYPE_FINGER;
  MotionEvent.PointerCoords coords=new MotionEvent.PointerCoords();coords.x=x;coords.y=y;coords.pressure=action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_CANCEL?0f:1f;coords.size=1f;
  MotionEvent event=MotionEvent.obtain(down,time,action,1,new MotionEvent.PointerProperties[]{properties},new MotionEvent.PointerCoords[]{coords},0,0,1f,1f,0,0,InputDevice.SOURCE_TOUCHSCREEN,0);
  if(event.getToolType(0)!=MotionEvent.TOOL_TYPE_FINGER || !event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN))throw new AssertionError("Invalid finger event metadata");
  return event;
 }
}
