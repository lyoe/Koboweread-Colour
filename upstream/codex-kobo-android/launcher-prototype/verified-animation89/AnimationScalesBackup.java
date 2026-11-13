/** Only the three Android window/transition/animator scale values. */
public final class AnimationScalesBackup {
 public static void validate(float[] s){if(s==null||s.length!=3)throw new IllegalArgumentException("Three scales required");for(float v:s)if(Float.isNaN(v)||Float.isInfinite(v)||v<0||v>20)throw new IllegalArgumentException("Scale out of range");}
 public static String encode(float[] s){validate(s);return s[0]+" "+s[1]+" "+s[2]+"\n";}
 public static float[] decode(String text){if(text==null)throw new IllegalArgumentException("Missing scales");String[] parts=text.trim().split("\\s+");if(parts.length!=3)throw new IllegalArgumentException("Malformed scales");float[] v={Float.parseFloat(parts[0]),Float.parseFloat(parts[1]),Float.parseFloat(parts[2])};validate(v);return v;}
}
