#!/bin/sh
set -eu
ulimit -c 0
root=/tmp/kobo-android-runtime
cd "$root/lab"
[ "$(uname -r)" = '4.1.15-00286-g2a6a4cb08c66' ] || exit 2
if [ -e /dev/binder ] || [ -e /dev/ashmem ] || grep -qE '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules; then echo 'Existing Android devices; refusing';exit 3;fi
binder_loaded=0;ashmem_loaded=0;vm_loaded=0;manager_pid=;logger_pid=;debugger_pid=;proc_mounted=0;property_pid=;sf_pid=;installer_pid=;dms_pid=
cleanup() {
 set +e
 if [ -n "$dms_pid" ];then kill "$dms_pid" 2>/dev/null;wait "$dms_pid" 2>/dev/null;cat "$root/lab/display-service.log";fi
 if [ -n "$installer_pid" ];then kill "$installer_pid" 2>/dev/null;wait "$installer_pid" 2>/dev/null;rm -f "$root/dev/socket/installd";cat "$root/lab/installd.log";fi
 if [ -n "$sf_pid" ];then kill "$sf_pid" 2>/dev/null;wait "$sf_pid" 2>/dev/null;cat "$root/lab/surfaceflinger.log";fi
 if [ -n "$property_pid" ];then kill "$property_pid" 2>/dev/null;wait "$property_pid" 2>/dev/null;rm -f "$root/dev/socket/property_service";cat "$root/lab/property-server.log";fi
 if [ -n "$manager_pid" ]; then kill "$manager_pid" 2>/dev/null;wait "$manager_pid" 2>/dev/null;fi
 if [ -n "$debugger_pid" ];then kill "$debugger_pid" 2>/dev/null;wait "$debugger_pid" 2>/dev/null;tail -80 "$root/lab/debuggerd.log";fi
 if [ -n "$logger_pid" ];then kill "$logger_pid" 2>/dev/null;wait "$logger_pid" 2>/dev/null;tail -120 "$root/lab/android.log";fi
 if [ "$proc_mounted" = 1 ];then umount "$root/proc";fi
 if [ "$vm_loaded" = 1 ];then rmmod kobo_madvise;fi
 if [ "$binder_loaded" = 1 ]; then rmmod binder_linux;fi
 if [ "$ashmem_loaded" = 1 ]; then rmmod ashmem_linux;fi
 echo 'Remaining test modules:';grep -E '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules || :
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
insmod ./ashmem_linux.ko;ashmem_loaded=1
insmod ./binder_linux.ko;binder_loaded=1
insmod ./kobo_madvise.ko;vm_loaded=1
mkdir -p "$root/dev" "$root/proc" "$root/sys" "$root/data" "$root/tmp"
for entry in 'null 1 3' 'zero 1 5' 'random 1 8' 'urandom 1 9';do set -- $entry;[ -c "$root/dev/$1" ] || mknod "$root/dev/$1" c "$2" "$3";done
for name in binder ashmem kobo_madvise;do
 if [ "$name" = binder ];then number=$(cat /sys/class/binder/binder/dev);else number=$(cat "/sys/class/misc/$name/dev");fi
 rm -f "$root/dev/$name"
 mknod "$root/dev/$name" c "${number%:*}" "${number#*:}"
done
mount -t proc -o ro proc "$root/proc"
proc_mounted=1
rm -rf "$root/dev/__properties__"
mkdir -p "$root/dev/socket"
chroot "$root" /system/bin/sh -c 'export KOBO_PROPERTY_DAEMON=1 LD_PRELOAD=/lab/property-server.so; exec /system/bin/sh -c true' > "$root/lab/property-server.log" 2>&1 &
property_pid=$!
sleep 1
kill -0 "$property_pid"
[ -S "$root/dev/socket/property_service" ] || { cat "$root/lab/property-server.log";exit 8; }
mkdir -p "$root/dev/socket"
"$root/lab/android-log-capture" "$root/dev/socket/logdw" > "$root/lab/android.log" 2>&1 &
logger_pid=$!
sleep 1
mkdir -p "$root/data/tombstones"
chroot "$root" /system/bin/debuggerd > "$root/lab/debuggerd.log" 2>&1 &
debugger_pid=$!
sleep 1
printf 'Native Android shell: '
chroot "$root" /system/bin/sh -c 'echo BIONIC_SHELL_OK'
chroot "$root" /system/bin/servicemanager > "$root/lab/servicemanager.log" 2>&1 &
manager_pid=$!
sleep 1
if ! kill -0 "$manager_pid" 2>/dev/null;then cat "$root/lab/servicemanager.log";echo 'servicemanager exited';exit 4;fi
printf 'Android service list:\n'
timeout 8 chroot "$root" /system/bin/service list
printf 'PASS: Android native service manager and Binder service-list transaction\n'

export ANDROID_ROOT=/system ANDROID_DATA=/data
export BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar
mkdir -p "$root/data/dalvik-cache/arm"
"$root/lab/vm-helper-smoke" "$root/dev/kobo_madvise" "$root/lab/madvise-private-test"
export CLASSPATH=/lab/framework-probe.dex:/system/framework/services.jar
printf 'Android APK parser test:\n'
KOBO_VM_SHIM=1 timeout 90 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin FrameworkProbe verify-package /lab/weread.apk
printf 'Android property service write test:\n'
KOBO_VM_SHIM=1 timeout 30 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin FrameworkProbe property test
printf 'Android software EGL test:\n'
if chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/egl-probe.so /system/bin/sh -c true';then
 echo EGL_PIXEL_TEST_PASSED
else
 echo EGL_PIXEL_TEST_BLOCKED
fi
mkdir -p "$root/dev/graphics"
# This is a newly created regular file, never a device node or bind mount.
[ ! -e "$root/dev/graphics/fb0" ] || [ -f "$root/dev/graphics/fb0" ] || exit 9
dd if=/dev/zero of="$root/dev/graphics/fb0" bs=4096 count=1350 2>/dev/null
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/libkobo_fb.so:/lab/virtual-fb-probe.so /system/bin/sh -c true' 
printf 'Direct software driver diagnostic:\n'
if chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/egl-driver-probe.so /system/bin/sh -c true';then
 echo DIRECT_EGL_PIXEL_TEST_PASSED
else
 echo DIRECT_EGL_PIXEL_TEST_BLOCKED
fi
printf 'SurfaceFlinger on the file-backed display:\n'
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/libkobo_fb.so:/lab/libkobo_vm.so exec /system/bin/surfaceflinger' > "$root/lab/surfaceflinger.log" 2>&1 &
sf_pid=$!
sleep 2
kill -0 "$sf_pid"
mkdir -p "$root/data/misc/user" "$root/data/user" "$root/data/user_de/0" "$root/data/app" "$root/data/app-lib" "$root/data/app-private" "$root/data/app-asec" "$root/data/dalvik-cache" "$root/data/system" "$root/data/local/tmp" "$root/system/app" "$root/system/priv-app"
mkdir -p "$root/data/data" "$root/data/system_de/0" "$root/data/system_ce/0" "$root/data/misc/profiles/cur/0" "$root/data/misc/profiles/ref"
if [ -d "$root/data/user/0" ] && [ ! -L "$root/data/user/0" ];then rmdir "$root/data/user/0";fi
[ -L "$root/data/user/0" ] || ln -s /data/data "$root/data/user/0"
"$root/lab/android-socket-exec" "$root" > "$root/lab/installd.log" 2>&1 &
installer_pid=$!
sleep 2
kill -0 "$installer_pid" || { cat "$root/lab/installd.log";exit 13; }
"$root/lab/installd-ping" "$root"
[ ! -e "$root/acct" ] && [ ! -L "$root/acct" ] || { echo UNEXPECTED_ACCT_PATH;exit 14; }
printf 'Android display service bootstrap:\n'
KOBO_VM_SHIM=1 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin FrameworkProbe settings-server unused > "$root/lab/display-service.log" 2>&1 &
dms_pid=$!
ready=0
attempt=0
while [ "$attempt" -lt 60 ];do
 attempt=$((attempt+1))
 if grep -q ANDROID_DISPLAY_MANAGER_DEFAULT_READY "$root/lab/display-service.log";then ready=1;break;fi
 kill -0 "$dms_pid" 2>/dev/null || break
 sleep 1
done
[ "$ready" = 1 ] || { echo DISPLAY_SERVICE_NOT_READY;exit 10; }
timeout 8 chroot "$root" /system/bin/service list
printf 'Activity manager permission IPC test:\n'
KOBO_VM_SHIM=1 timeout 30 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin FrameworkProbe activity-client unused
printf 'Package manager core package IPC test:\n'
KOBO_VM_SHIM=1 timeout 30 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin FrameworkProbe package-client unused
