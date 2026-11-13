// Isolated host fixture; never included in the Android runtime.
package android.os; public final class SystemProperties {public static String launch="1000";public static String get(String key){return key.equals("kobo.lab.weread.start_uptime")?launch:"";}}
