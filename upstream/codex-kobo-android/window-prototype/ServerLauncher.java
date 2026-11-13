import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
/** Mirrors Android 7 system_server's shared native-library namespace. */
public final class ServerLauncher {
    public static void main(String[] args) throws Exception {
        Class<?> factory=Class.forName("com.android.internal.os.PathClassLoaderFactory");
        Method create=factory.getMethod("createClassLoader",String.class,String.class,String.class,ClassLoader.class,int.class,boolean.class);
        ClassLoader loader=(ClassLoader)create.invoke(null,"/lab/framework-probe.dex:/system/framework/services.jar","/system/lib",null,ClassLoader.getSystemClassLoader(),24,true);
        Thread.currentThread().setContextClassLoader(loader);
        System.out.println("ANDROID_SERVER_SHARED_NAMESPACE_READY");
        try { Class.forName("FrameworkProbe",true,loader).getMethod("main",String[].class).invoke(null,(Object)args); }
        catch(InvocationTargetException e) {
            Throwable cause=e.getCause();
            try(java.io.PrintWriter diagnostic=new java.io.PrintWriter("/lab/server-entry-error.txt")){
                Throwable item=cause;
                for(int depth=0;item!=null && depth<8;depth++,item=item.getCause()){
                    diagnostic.println(item.getClass().getName()+": "+item.getMessage());
                    StackTraceElement[] trace=item.getStackTrace();
                    for(int i=0;i<Math.min(trace.length,48);i++)diagnostic.println(trace[i]);
                }
            }
            if(cause instanceof Exception)throw (Exception)cause;throw new RuntimeException(cause);
        }
    }
}
