import android.content.Context;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import java.lang.reflect.*;

/** Android 7 config.disable_network leaves one ContentService query null-unsafe.
 * Query its actual framework SyncAdaptersCache without creating network sync.
 * No fabricated adapter list, framework bytecode patch, or success reply. */
public final class OfflineSyncAdapterQueries {
    public static void install(final Context context,Object lifecycle) throws Exception {
        Class<?> props=Class.forName("android.os.SystemProperties");
        if(!"true".equals(props.getMethod("get",String.class).invoke(null,"config.disable_network")))throw new IllegalStateException("Local test switch required");
        Field service=lifecycle.getClass().getDeclaredField("mService");service.setAccessible(true);
        if(service.get(lifecycle)!=null)throw new IllegalStateException("Publish only once");
        Class<?> type=Class.forName("com.android.server.content.ContentService");
        Constructor<?> contentConstructor=type.getDeclaredConstructor(Context.class,boolean.class);contentConstructor.setAccessible(true);
        final Object original=contentConstructor.newInstance(context,false);
        service.set(lifecycle,original);
        final IBinder originalBinder=(IBinder)original;
        Class<?> cacheType=Class.forName("android.content.SyncAdaptersCache");
        Constructor<?> constructor=cacheType.getDeclaredConstructor(Context.class);constructor.setAccessible(true);
        final Object cache=constructor.newInstance(context);
        final Method query=cacheType.getMethod("getSyncAdapterPackagesForAuthority",String.class,int.class);
        final Method enforce=original.getClass().getDeclaredMethod("enforceCrossUserPermission",int.class,String.class);enforce.setAccessible(true);
        Class<?> stub=Class.forName("android.content.IContentService$Stub");
        Field transaction=stub.getDeclaredField("TRANSACTION_getSyncAdapterPackagesForAuthorityAsUser");transaction.setAccessible(true);
        final int queryCode=transaction.getInt(null);
        Binder boundary=new Binder(){
            @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags) throws RemoteException {
                if(code!=queryCode){
                    System.out.println("ANDROID_CONTENT_FORWARD transaction="+code);
                    return originalBinder.transact(code,data,reply,flags);
                }
                data.enforceInterface("android.content.IContentService");
                String authority=data.readString();int user=data.readInt();
                System.out.println("ANDROID_OFFLINE_SYNC_CACHE_QUERY authority="+authority+" user="+user);
                String[] packages=read(enforce,original,query,cache,authority,user);
                reply.writeNoException();reply.writeStringArray(packages);return true;
            }
        };
        boundary.attachInterface(null,"android.content.IContentService");
        Class.forName("android.os.ServiceManager").getMethod("addService",String.class,IBinder.class).invoke(null,"content",boundary);
        // ContentService also registers a direct in-process PackageManager callback.
        Class<?> provider=Class.forName("android.content.pm.PackageManagerInternal$SyncAdapterPackagesProvider");
        Object callback=Proxy.newProxyInstance(provider.getClassLoader(),new Class<?>[]{provider},new InvocationHandler(){
            @Override public Object invoke(Object proxy,Method method,Object[] args){
            if("getPackages".equals(method.getName()))return read(enforce,original,query,cache,(String)args[0],(Integer)args[1]);
            if("toString".equals(method.getName()))return "ActualOfflineSyncAdaptersCache";
            if("hashCode".equals(method.getName()))return System.identityHashCode(proxy);
            if("equals".equals(method.getName()))return proxy==args[0];
            throw new UnsupportedOperationException(method.toString());
            }
        });
        Class<?> internal=Class.forName("android.content.pm.PackageManagerInternal");
        Object packages=Class.forName("com.android.server.LocalServices").getMethod("getService",Class.class).invoke(null,internal);
        internal.getMethod("setSyncAdapterPackagesprovider",provider).invoke(packages,callback);
        System.out.println("ANDROID_OFFLINE_SYNC_QUERY_USES_REAL_ADAPTER_CACHE transaction="+queryCode);
    }
    private static String[] read(Method enforce,Object original,Method query,Object cache,String authority,int user){
        try {
            enforce.invoke(original,user,"no permission to read sync settings for user "+user);
            long token=Binder.clearCallingIdentity();
            try {return (String[])query.invoke(cache,authority,user);}
            finally {Binder.restoreCallingIdentity(token);}
        } catch(InvocationTargetException e){
            Throwable cause=e.getCause();if(cause instanceof RuntimeException)throw (RuntimeException)cause;
            throw new IllegalStateException(cause);
        } catch(ReflectiveOperationException e){throw new IllegalStateException(e);}
    }
}
