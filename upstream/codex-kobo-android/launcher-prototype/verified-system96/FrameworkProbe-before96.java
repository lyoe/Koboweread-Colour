import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Button;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.*;
import java.util.List;

/** Platform framework probes; app-server launches the fixed local test APK. */
public final class FrameworkProbe {
    public static void main(String[] args) throws Exception {
        final boolean readingMode=args.length>0 && "reading-server".equals(args[0]);
        final boolean installMode=readingMode || args.length>0 && "install-server".equals(args[0]);
        final boolean appMode=installMode || args.length>0 && "app-server".equals(args[0]);
        final boolean windowMode=appMode || args.length>0 && "window-server".equals(args[0]);
        final boolean settingsMode=windowMode || args.length>0 && "settings-server".equals(args[0]);
        if (args.length != 2) throw new IllegalArgumentException("package APK | context PNG");
        if ("launch-weread".equals(args[0])) {try{InstallProbe.launchWeReadExisting(args[1]);}catch(Throwable error){error.printStackTrace();System.exit(50);}return;}
        if ("launch-installed".equals(args[0])) {try{InstallProbe.launchExisting(args[1]);}catch(Throwable error){error.printStackTrace();System.exit(50);}return;}
        if ("install-apk".equals(args[0])) {try{InstallProbe.run(args[1]);}catch(Throwable error){error.printStackTrace();System.exit(50);}return;}
        if ("apk-cache-selftest".equals(args[0])) {ApkIdentityCache.selfTest();System.exit(0);return;}
        if ("wifi-activity-client".equals(args[0])) {HostWifiQueries.verifyActivityQueries();System.exit(0);return;}
        if ("accessibility-client".equals(args[0])) {
            if(Looper.myLooper()==null)Looper.prepareMainLooper();
            Class<?> t=Class.forName("android.app.ActivityThread");
            Object system=t.getMethod("systemMain").invoke(null);
            AccessibilityBootstrap.verifyClient((Context)t.getMethod("getSystemContext").invoke(system));
            System.exit(0);return;
        }
        if ("heap-properties".equals(args[0])) {
            if(android.os.Process.myUid()!=0)throw new SecurityException("Root setup required");
            Class<?> props=Class.forName("android.os.SystemProperties");Method set=props.getMethod("set",String.class,String.class);
            set.invoke(null,"dalvik.vm.heapsize","128m");set.invoke(null,"dalvik.vm.heapgrowthlimit","64m");
            System.out.println("ANDROID_ACTIVITY_HEAP_PROPERTIES_SET");System.exit(0);
        }
        if ("property".equals(args[0])) {
            Class<?> properties=Class.forName("android.os.SystemProperties");
            Method set=properties.getMethod("set",String.class,String.class);
            Method get=properties.getMethod("get",String.class);
            String key="kobo.lab.probe";
            set.invoke(null,key,"first");
            if(!"first".equals(get.invoke(null,key)))throw new AssertionError("Property add failed");
            set.invoke(null,key,"second");
            if(!"second".equals(get.invoke(null,key)))throw new AssertionError("Property update failed");
            Object before=get.invoke(null,"ro.product.cpu.abilist32");
            set.invoke(null,"ro.product.cpu.abilist32","invalid");
            if(!before.equals(get.invoke(null,"ro.product.cpu.abilist32")))throw new AssertionError("Read-only property modified");
            System.out.println("ANDROID_PROPERTY_WRITE_READ_OK add update immutable-ro");
            return;
        }
        if ("capture-scanout".equals(args[0])) {
            Bitmap captured=SurfaceProbe.captureVirtualFramebuffer(720,960);
            try(FileOutputStream out=new FileOutputStream(args[1])){if(!captured.compress(Bitmap.CompressFormat.PNG,100,out))throw new AssertionError("Capture failed");}
            captured.recycle();System.out.println("ANDROID_ACTIVITY_SCANOUT_CAPTURED");System.exit(0);
        }
        if ("jni-linkage".equals(args[0])) { System.loadLibrary("android_servers"); reportRuntimeMappings();return; }
        if ("verify-package".equals(args[0])) { inspect(args[1],true); return; }
        if ("package".equals(args[0])) { inspect(args[1]); return; }
        if ("display-server".equals(args[0]) || "activity-server".equals(args[0]) || "package-server".equals(args[0]) || settingsMode) {
            if (Looper.myLooper() == null) Looper.prepareMainLooper();
            if(appMode){
                Class<?> props=Class.forName("android.os.SystemProperties");Method set=props.getMethod("set",String.class,String.class);
                set.invoke(null,"dalvik.vm.heapsize","128m");set.invoke(null,"dalvik.vm.heapgrowthlimit","64m");
                set.invoke(null,"kobo.lab.startup.profile",readingMode?"reading":"diagnostic");
                // Use Android's existing local-test switch until network management is adapted.
                set.invoke(null,"config.disable_network","true");
                if(!"true".equals(props.getMethod("get",String.class).invoke(null,"config.disable_network")))throw new AssertionError("Network service test setting failed");
                System.out.println("ANDROID_LOCAL_TEST_NETWORK_MANAGEMENT_AND_SYNC_DISABLED");
                if(installMode){
                    set.invoke(null,"dalvik.vm.dex2oat-Xms","8m");set.invoke(null,"dalvik.vm.dex2oat-Xmx","64m");
                    set.invoke(null,"dalvik.vm.dex2oat-flags","--runtime-arg -Xnorelocate");
                    if(!"64m".equals(props.getMethod("get",String.class).invoke(null,"dalvik.vm.dex2oat-Xmx")))throw new AssertionError("Compiler heap configuration failed");
                    System.out.println("ANDROID_INSTALL_COMPILER_HEAP xms=8m xmx=64m");
                }
            }
            Class<?> activityThread=Class.forName("android.app.ActivityThread");
            Object system=activityThread.getMethod("systemMain").invoke(null);
            Context context=(Context)activityThread.getMethod("getSystemContext").invoke(system);
            if(appMode)Class.forName("com.android.server.AttributeCache").getMethod("init",Context.class).invoke(null,context);
            System.loadLibrary("android_servers");
            reportRuntimeMappings();
            for (String prerequisite : new String[]{"com.android.server.lights.LightsService", "com.android.server.power.PowerManagerService"}) {
                Class<?> type=Class.forName(prerequisite);
                Object instance=type.getConstructor(Context.class).newInstance(context);
                type.getMethod("onStart").invoke(instance);
                System.out.println("ANDROID_BOOTSTRAP_SERVICE_STARTED " + prerequisite);
            }
            Class<?> displayService=Class.forName("com.android.server.display.DisplayManagerService");
            Object service=displayService.getConstructor(Context.class).newInstance(context);
            displayService.getMethod("onStart").invoke(service);
            displayService.getMethod("onBootPhase",int.class).invoke(service,100);
            if ("activity-server".equals(args[0]) || "package-server".equals(args[0]) || settingsMode) {
                Class<?> activityService=Class.forName("com.android.server.am.ActivityManagerService");
                Object manager=activityService.getConstructor(Context.class).newInstance(context);
                if(appMode){Class<?> services=Class.forName("com.android.server.SystemServiceManager");Object registry=services.getConstructor(Context.class).newInstance(context);activityService.getMethod("setSystemServiceManager",services).invoke(manager,registry);}
                Class.forName("android.os.ServiceManager").getMethod("addService",String.class,android.os.IBinder.class)
                    .invoke(null,"activity",(android.os.IBinder)manager);
                System.out.println("ANDROID_ACTIVITY_MANAGER_CONSTRUCTED_AND_PUBLISHED");
                if ("package-server".equals(args[0]) || settingsMode) {
                    Class<?> installerClass=Class.forName("com.android.server.pm.Installer");
                    Object installer=installerClass.getConstructor(Context.class).newInstance(context);
                    installerClass.getMethod("onStart").invoke(installer);
                    if(appMode)activityService.getMethod("setInstaller",installerClass).invoke(manager,installer);
                    System.out.println("ANDROID_INSTALLER_JAVA_STARTED");System.out.flush();
                    Class<?> packages=Class.forName("com.android.server.pm.PackageManagerService");
                    Object packageManager=packages.getMethod("main",Context.class,installerClass,boolean.class,boolean.class)
                        .invoke(null,context,installer,false,!installMode);
                    System.out.println("ANDROID_PACKAGE_MANAGER_CORE_READY");System.out.flush();
                    if(settingsMode) {
                        Method start=activityService.getDeclaredMethod("start");start.setAccessible(true);start.invoke(manager);
                        Class<?> userLifecycle=Class.forName("com.android.server.pm.UserManagerService$LifeCycle");
                        Object users=userLifecycle.getConstructor(Context.class).newInstance(context);userLifecycle.getMethod("onStart").invoke(users);
                        activityService.getMethod("setSystemProcess").invoke(manager);
                        if(appMode){Class<?> accounts=Class.forName("com.android.server.accounts.AccountManagerService$Lifecycle");Object accountService=accounts.getConstructor(Context.class).newInstance(context);accounts.getMethod("onStart").invoke(accountService);System.out.println("ANDROID_ACCOUNT_SERVICE_STARTED");}
                        Class<?> contentLifecycle=Class.forName("com.android.server.content.ContentService$Lifecycle");
                        Object content=contentLifecycle.getConstructor(Context.class).newInstance(context);
                        if(appMode)OfflineSyncAdapterQueries.install(context,content);
                        else contentLifecycle.getMethod("onStart").invoke(content);
                        Class<?> usageService=Class.forName("com.android.server.usage.UsageStatsService");
                        Object usage=usageService.getConstructor(Context.class).newInstance(context);usageService.getMethod("onStart").invoke(usage);
                        Class<?> usageInternal=Class.forName("android.app.usage.UsageStatsManagerInternal");
                        Object usageLocal=Class.forName("com.android.server.LocalServices").getMethod("getService",Class.class).invoke(null,usageInternal);
                        activityService.getMethod("setUsageStatsManager",usageInternal).invoke(manager,usageLocal);
                        activityService.getMethod("installSystemProviders").invoke(manager);
                        String key="kobo_lab_probe";
                        String old=android.provider.Settings.Global.getString(context.getContentResolver(),key);
                        try {
                            if(!android.provider.Settings.Global.putString(context.getContentResolver(),key,"framework-test")
                                || !"framework-test".equals(android.provider.Settings.Global.getString(context.getContentResolver(),key)))throw new AssertionError("Settings write/read failed");
                        } finally { android.provider.Settings.Global.putString(context.getContentResolver(),key,old); }
                        if(!java.util.Objects.equals(old,android.provider.Settings.Global.getString(context.getContentResolver(),key)))throw new AssertionError("Settings restore failed");
                        System.out.println("ANDROID_SETTINGS_PROVIDER_WRITE_READ_RESTORE_OK");System.out.flush();
                        if(windowMode) {
                            context.setTheme(android.R.style.Theme_DeviceDefault_Light_DarkActionBar);
                            Class<?> inputClass=Class.forName("com.android.server.input.InputManagerService");
                            Object input=inputClass.getConstructor(Context.class).newInstance(context);
                            System.out.println("ANDROID_INPUT_MANAGER_CONSTRUCTED");System.out.flush();
                            Class<?> windowClass=Class.forName("com.android.server.wm.WindowManagerService");
                            Object windows=windowClass.getMethod("main",Context.class,inputClass,boolean.class,boolean.class,boolean.class).invoke(null,context,input,appMode,false,true);
                            Method addService=Class.forName("android.os.ServiceManager").getMethod("addService",String.class,android.os.IBinder.class);
                            addService.invoke(null,"window",(android.os.IBinder)windows);addService.invoke(null,"input",(android.os.IBinder)input);
                            activityService.getMethod("setWindowManager",windowClass).invoke(manager,windows);
                            if(appMode)activityService.getMethod("initPowerManagement").invoke(manager);
                            Object callbacks=windowClass.getMethod("getInputMonitor").invoke(windows);
                            inputClass.getMethod("setWindowManagerCallbacks",Class.forName("com.android.server.input.InputManagerService$WindowManagerCallbacks")).invoke(input,callbacks);
                            inputClass.getMethod("start").invoke(input);
                            displayService.getMethod("windowManagerAndInputReady").invoke(service);
                            windowClass.getMethod("displayReady").invoke(windows);
                            if(appMode){Class<?> text=Class.forName("com.android.server.TextServicesManagerService$Lifecycle");Object textService=text.getConstructor(Context.class).newInstance(context);text.getMethod("onStart").invoke(textService);text.getMethod("onBootPhase",int.class).invoke(textService,550);}
                            Class<?> imeLifecycle=Class.forName("com.android.server.InputMethodManagerService$Lifecycle");
                            Object ime=imeLifecycle.getConstructor(Context.class).newInstance(context);imeLifecycle.getMethod("onStart").invoke(ime);
                            if(appMode)imeLifecycle.getMethod("onBootPhase",int.class).invoke(ime,550);
                            if(appMode){Class<?> mount=Class.forName("com.android.server.MountService$Lifecycle");Object storage=mount.getConstructor(Context.class).newInstance(context);mount.getMethod("onStart").invoke(storage);System.out.println("ANDROID_MOUNT_SERVICE_STARTED");}
                            if(appMode){Class<?> idle=Class.forName("com.android.server.DeviceIdleController");Object idleService=idle.getConstructor(Context.class).newInstance(context);idle.getMethod("onStart").invoke(idleService);usageService.getMethod("onBootPhase",int.class).invoke(usage,500);System.out.println("ANDROID_USAGE_BATTERY_DEPENDENCIES_READY");}
                            if(appMode){Class<?> vrClass=Class.forName("com.android.server.vr.VrManagerService");Object vr=vrClass.getConstructor(Context.class).newInstance(context);vrClass.getMethod("onStart").invoke(vr);vrClass.getMethod("onBootPhase",int.class).invoke(vr,500);System.out.println("ANDROID_VR_SERVICE_READY");}
                            if(!appMode)windowClass.getMethod("enableScreenAfterBoot").invoke(windows);
                            windowClass.getMethod("setEventDispatching",boolean.class).invoke(windows,true);
                            System.out.println(appMode?"ANDROID_WINDOW_BOOT_DISPLAY_DEFERRED":"ANDROID_WINDOW_BOOT_DISPLAY_NOTIFIED");
                            System.out.println("ANDROID_WINDOW_INPUT_SERVICES_READY");System.out.flush();
                            if(installMode){packages.getMethod("systemReady").invoke(packageManager);if(HostDefaultConnectivityService.enabled()){HostDefaultConnectivityService.install(context);HostWifiQueries.install(context);}else OfflineConnectivityService.install(context);NoOutputAudioQueries.install(context);WebViewBootstrap.start(context);System.out.println("ANDROID_PACKAGE_MANAGER_SYSTEM_READY");PackageProcessRegistry.write(context);}
                            if(installMode){
                                Class<?> notifications=Class.forName("com.android.server.notification.NotificationManagerService");
                                Object notificationService=notifications.getConstructor(Context.class).newInstance(context);
                                notifications.getMethod("onStart").invoke(notificationService);
                                android.os.IBinder notificationBinder=(android.os.IBinder)Class.forName("android.os.ServiceManager").getMethod("checkService",String.class).invoke(null,Context.NOTIFICATION_SERVICE);
                                if(notificationBinder==null || !notificationBinder.pingBinder())throw new AssertionError("Notification service not published");
                                System.out.println("ANDROID_NOTIFICATION_SERVICE_STARTED");System.out.flush();
                            }
                            if(installMode)AccessibilityBootstrap.install(context);
                            if(installMode)LiveFocusService.install(windows);
                            if(readingMode)StartupServiceGrace.install(manager);
                            if(appMode)ActivityLauncher.start(context,manager,installer);
                        }
                    }
                }
            }
            System.out.println("ANDROID_DISPLAY_MANAGER_DEFAULT_READY");
            System.out.flush();
            Looper.loop();
            throw new AssertionError("Display server loop returned");
        }
        if ("activity-client".equals(args[0])) {
            if (Looper.myLooper()==null) Looper.prepareMainLooper();
            Class<?> thread=Class.forName("android.app.ActivityThread");
            Object system=thread.getMethod("systemMain").invoke(null);
            Context context=(Context)thread.getMethod("getSystemContext").invoke(system);
            int permission=context.checkCallingOrSelfPermission("android.permission.INTERACT_ACROSS_USERS");
            if(permission!=android.content.pm.PackageManager.PERMISSION_GRANTED)throw new AssertionError("Root permission check failed");
            System.out.println("ANDROID_ACTIVITY_BINDER_PERMISSION_OK uid="+android.os.Process.myUid());
            System.exit(0);
        }
        if ("package-client".equals(args[0])) {
            if (Looper.myLooper()==null) Looper.prepareMainLooper();
            Class<?> thread=Class.forName("android.app.ActivityThread");
            Object system=thread.getMethod("systemMain").invoke(null);
            Context context=(Context)thread.getMethod("getSystemContext").invoke(system);
            android.content.pm.PackageInfo info=context.getPackageManager().getPackageInfo("android",0);
            if(!"android".equals(info.packageName))throw new AssertionError("Framework package missing");
            System.out.println("ANDROID_PACKAGE_MANAGER_CLIENT_OK "+info.packageName+" uid="+info.applicationInfo.uid);
            System.exit(0);
        }
        if (!"context".equals(args[0])) throw new IllegalArgumentException(args[0]);
        if (Looper.myLooper() == null) Looper.prepareMainLooper();
        Class<?> thread = Class.forName("android.app.ActivityThread");
        Object system = thread.getMethod("systemMain").invoke(null);
        Context context = (Context) thread.getMethod("getSystemContext").invoke(system);
        if (context == null) throw new AssertionError("No system context");
        System.out.println("ANDROID_SYSTEM_CONTEXT_OK " + context.getPackageName());
        context.setTheme(android.R.style.Theme_Material_Light_NoActionBar);
        // Explicit offscreen device configuration; no WindowManager service is emulated.
        final Resources systemResources=context.getResources();
        final int menuResource=systemResources.getIdentifier("config_overrideHasPermanentMenuKey","integer","android");
        final Resources offscreenResources=new Resources(context.getAssets(),systemResources.getDisplayMetrics(),systemResources.getConfiguration()) {
            @Override public int getInteger(int id) throws Resources.NotFoundException {
                return id==menuResource ? 2 : super.getInteger(id);
            }
        };
        context=new ContextWrapper(context) {
            @Override public Resources getResources() { return offscreenResources; }
            @Override public int checkCallingOrSelfPermission(String permission) {
                // The standalone drawing probe never acts across users.
                if ("android.permission.INTERACT_ACROSS_USERS".equals(permission)
                        || "android.permission.INTERACT_ACROSS_USERS_FULL".equals(permission)) {
                    return android.content.pm.PackageManager.PERMISSION_DENIED;
                }
                return super.checkCallingOrSelfPermission(permission);
            }
        };
        System.out.println("OFFSCREEN_VIEW_NO_MENU_KEY_OVERRIDE");
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32,48,32,32);
        root.setBackgroundColor(0xffffffff);
        TextView title = new TextView(context);
        title.setText("Android 控件测试"); title.setTextSize(26);title.setTextColor(0xff000000);
        root.addView(title);
        TextView note = new TextView(context);
        note.setText("微信读书：安装前解析检查通过\nKobo 本机 Android 软件显示\n\n尚未启动 Activity。");
        note.setTextSize(20);note.setTextColor(0xff000000);root.addView(note);
        Button button = new Button(context);button.setText("Android 标准按钮（绘制测试）");root.addView(button);
        int width=720,height=960;
        root.measure(View.MeasureSpec.makeMeasureSpec(width,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(height,View.MeasureSpec.EXACTLY));
        root.layout(0,0,width,height);
        Bitmap image=Bitmap.createBitmap(width,height,Bitmap.Config.ARGB_8888);
        root.draw(new Canvas(image));
        try(FileOutputStream out=new FileOutputStream(args[1])) {
            if(!image.compress(Bitmap.CompressFormat.PNG,100,out))throw new AssertionError("PNG failed");
        }
        image.recycle();
        System.out.println("ANDROID_FRAMEWORK_VIEW_PNG_OK " + args[1]);
        System.exit(0);
    }
    private static void reportRuntimeMappings() throws Exception {
        int count=0;
        try(java.io.BufferedReader input=new java.io.BufferedReader(new java.io.FileReader("/proc/self/maps"))) {
            String line;while((line=input.readLine())!=null)if(line.contains("r-xp")&&line.endsWith("/libandroid_runtime.so")){count++;System.out.println("NATIVE_RUNTIME_MAPPING "+line);}
        }
        System.out.println("NATIVE_RUNTIME_EXECUTABLE_COPIES "+count);
    }
    private static void inspect(String path) throws Exception { inspect(path,false); }
    private static void inspect(String path,boolean verify) throws Exception {
        Class<?> parserClass=Class.forName("android.content.pm.PackageParser");
        Object parser=parserClass.getConstructor().newInstance();
        Object pkg=parserClass.getMethod("parsePackage",File.class,int.class).invoke(parser,new File(path),0);
        Class<?> pkgClass=pkg.getClass();
        String name=(String)pkgClass.getField("packageName").get(pkg);
        ApplicationInfo info=(ApplicationInfo)pkgClass.getField("applicationInfo").get(pkg);
        if(!"com.tencent.weread.eink".equals(name))throw new AssertionError("Unexpected package "+name);
        System.out.println("ANDROID_PACKAGE_PARSE_OK "+name+" minSdk="+info.minSdkVersion+" targetSdk="+info.targetSdkVersion);
        List<?> activities=(List<?>)pkgClass.getField("activities").get(pkg);
        int launcherCount=0;
        for(Object activity:activities) {
            ActivityInfo activityInfo=(ActivityInfo)activity.getClass().getField("info").get(activity);
            List<?> filters=(List<?>)activity.getClass().getField("intents").get(activity);
            for(Object f:filters) {
                IntentFilter filter=(IntentFilter)f;
                if(filter.hasAction(Intent.ACTION_MAIN)&&filter.hasCategory(Intent.CATEGORY_LAUNCHER)) {
                    launcherCount++;
                    System.out.println("APK_LAUNCHER "+activityInfo.name+" process="+activityInfo.processName);
                }
            }
        }
        System.out.println("APK_COMPONENTS activities="+activities.size()+" services="+((List<?>)pkgClass.getField("services").get(pkg)).size()+" providers="+((List<?>)pkgClass.getField("providers").get(pkg)).size());
        AssetManager assets=AssetManager.class.getConstructor().newInstance();
        int cookie=(Integer)AssetManager.class.getMethod("addAssetPath",String.class).invoke(assets,path);
        if(cookie==0)throw new AssertionError("AssetManager rejected APK");
        DisplayMetrics metrics=new DisplayMetrics();metrics.setToDefaults();
        Resources resources=new Resources(assets,metrics,Resources.getSystem().getConfiguration());
        CharSequence label=info.nonLocalizedLabel;
        if(info.labelRes!=0)label=resources.getText(info.labelRes);
        System.out.println("APK_RESOURCE_LABEL "+label);
        if(verify) {
            parserClass.getMethod("collectCertificates",pkgClass,int.class).invoke(null,pkg,0);
            android.content.pm.Signature[] signatures=(android.content.pm.Signature[])pkgClass.getField("mSignatures").get(pkg);
            if(signatures==null||signatures.length==0)throw new AssertionError("No verified signatures");
            byte[] digest=java.security.MessageDigest.getInstance("SHA-256").digest(signatures[0].toByteArray());
            StringBuilder fingerprint=new StringBuilder();for(byte value:digest)fingerprint.append(String.format(java.util.Locale.ROOT,"%02x",value&255));
            System.out.println("ANDROID_APK_CERTIFICATES_VERIFIED count="+signatures.length+" signerSha256="+fingerprint);
        }
        if(launcherCount==0)throw new AssertionError("No launcher");
        assets.close();
        System.out.println("ANDROID_APK_PREFLIGHT_OK");
    }
}
