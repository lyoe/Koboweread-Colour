import android.content.Context;
import android.media.AudioManager;
import android.os.*;
import java.lang.reflect.*;
import java.util.*;
/** Query-only silent virtual volume policy. No audio hardware or playback is provided. */
public final class NoOutputAudioQueries {
 public static void install(Context context)throws Exception {
  if(android.os.Process.myUid()!=1000)throw new SecurityException("System service UID required");
  Class<?> services=Class.forName("android.os.ServiceManager");
  if(services.getMethod("checkService",String.class).invoke(null,"audio")!=null)throw new IllegalStateException("Audio already published");
  final Map<Integer,String> methods=new HashMap<Integer,String>();
  for(Field field:Class.forName("android.media.IAudioService$Stub").getDeclaredFields())if(field.getName().startsWith("TRANSACTION_")){field.setAccessible(true);methods.put(field.getInt(null),field.getName().substring(12));}
  Binder boundary=new Binder(){
   @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException {
    if(code==IBinder.INTERFACE_TRANSACTION){reply.writeString("android.media.IAudioService");return true;}
    String method=methods.get(code);if(method==null)return super.onTransact(code,data,reply,flags);
    data.enforceInterface("android.media.IAudioService");int value=0;boolean oneWay=false;
    if(method.equals("getStreamMaxVolume")||method.equals("getStreamMinVolume")||method.equals("getStreamVolume")||method.equals("getLastAudibleStreamVolume")||method.equals("isStreamMute")||method.equals("isStreamAffectedByMute")||method.equals("isStreamAffectedByRingerMode")){
     int stream=data.readInt();if(stream<0||stream>10)throw new IllegalArgumentException("Unknown audio stream");
     value=method.equals("getStreamMaxVolume")?15:method.startsWith("is")?1:0;
    }else if(method.equals("isMasterMute")){value=1;}
    else if(method.equals("getUiSoundsStreamType")){value=AudioManager.STREAM_SYSTEM;}
    else if(method.equals("getVibrateSetting")||method.equals("shouldVibrate")){data.readInt();}
    else if(method.equals("isValidRingerMode")){int mode=data.readInt();value=mode>=0&&mode<=2?1:0;}
    else if(method.equals("playSoundEffect")||method.equals("playSoundEffectVolume")){
     data.readInt();if(method.equals("playSoundEffectVolume"))data.readFloat();oneWay=true;
    }else if(method.equals("unloadSoundEffects")){oneWay=true;}
    else if(!method.equals("getMode")&&!method.equals("getCurrentAudioFocus")&&!method.equals("getRingerModeExternal")&&!method.equals("getRingerModeInternal")&&!method.equals("isSpeakerphoneOn")&&!method.equals("isBluetoothScoOn")&&!method.equals("isBluetoothA2dpOn")&&!method.equals("isHdmiSystemAudioSupported")&&!method.equals("isCameraSoundForced")&&!method.equals("loadSoundEffects")){
     System.out.println("ANDROID_NO_OUTPUT_AUDIO_UNSUPPORTED method="+method);throw new UnsupportedOperationException("No-output audio query policy: "+method);
    }
    if(data.dataAvail()!=0)throw new IllegalArgumentException("Unexpected audio query arguments");
    System.out.println("ANDROID_NO_OUTPUT_AUDIO_QUERY method="+method+" value="+value+" caller="+Binder.getCallingUid());
    if(!oneWay){reply.writeNoException();reply.writeInt(value);}return true;
   }
  };
  boundary.attachInterface(null,"android.media.IAudioService");
  services.getMethod("addService",String.class,IBinder.class).invoke(null,"audio",boundary);
  System.out.println("ANDROID_NO_OUTPUT_AUDIO_QUERIES_READY virtual_range=0..15 index=0 muted=true playback=false");
 }
 public static void verifyQueries(Context context){
  AudioManager manager=(AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
  if(manager==null||manager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)!=15||manager.getStreamVolume(AudioManager.STREAM_MUSIC)!=0||!manager.isStreamMute(AudioManager.STREAM_MUSIC))throw new AssertionError("Silent audio query contract failed");
  System.out.println("ANDROID_NO_OUTPUT_AUDIO_CLIENT_QUERIES_OK playback=false");
 }
}
