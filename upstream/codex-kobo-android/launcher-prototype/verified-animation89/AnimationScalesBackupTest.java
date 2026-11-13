import java.util.Arrays;
public class AnimationScalesBackupTest {
 static void bad(String text){try{AnimationScalesBackup.decode(text);throw new AssertionError("Invalid backup accepted");}catch(IllegalArgumentException expected){}}
 public static void main(String[] args){float[] values={1f,.5f,2f};if(!Arrays.equals(values,AnimationScalesBackup.decode(AnimationScalesBackup.encode(values))))throw new AssertionError();bad(null);bad("1 2");bad("1 2 3 4");bad("NaN 1 1");bad("Infinity 1 1");bad("-1 1 1");bad("21 1 1");System.out.println("ANIMATION89_THREE_SCALE_ROUNDTRIP_INVALID_AND_OUT_OF_RANGE_REJECTION_OK");}
}
