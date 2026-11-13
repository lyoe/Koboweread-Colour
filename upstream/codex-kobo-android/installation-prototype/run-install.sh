#!/bin/sh
set -eu
ulimit -c 0
root=/tmp/kobo-android-runtime
cd "$root/lab"
previous_installed_uid=$(awk '$1=="com.kobo.lab.installed" {print $2}' "$root/data/system/kobo-install-process-allowlist.txt" 2>/dev/null || :)
[ "$(uname -r)" = '4.1.15-00286-g2a6a4cb08c66' ] || exit 2
if [ -e /dev/binder ] || [ -e /dev/ashmem ] || grep -qE '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules; then echo 'Existing Android devices; refusing';exit 3;fi
binder_loaded=0;ashmem_loaded=0;vm_loaded=0;manager_pid=;logger_pid=;debugger_pid=;proc_mounted=0;property_pid=;sf_pid=;sensor_pid=;installer_pid=;vold_pid=;broker_pid=;dms_pid=
cleanup() {
 set +e
 if [ -n "$dms_pid" ];then kill "$dms_pid" 2>/dev/null;wait "$dms_pid" 2>/dev/null;cat "$root/lab/display-service.log";fi
 if [ -f "$root/lab/install-processes.pid" ];then
  while read owned_pid;do
   case "$owned_pid" in ''|*[!0-9]*) continue;;esac
   if [ "$(readlink "/proc/$owned_pid/root" 2>/dev/null)" = "$root" ] && [ "$(readlink "/proc/$owned_pid/exe" 2>/dev/null)" = "$root/system/bin/app_process32" ];then kill -KILL "$owned_pid" 2>/dev/null;fi
  done < "$root/lab/install-processes.pid"
  rm -f "$root/lab/install-processes.pid"
 fi
 if [ -f "$root/lab/app-probe.pid" ];then
  read app_pid < "$root/lab/app-probe.pid"
  case "$app_pid" in ''|*[!0-9]*) :;; *)
   app_root=$(readlink "/proc/$app_pid/root" 2>/dev/null)
   app_exe=$(readlink "/proc/$app_pid/exe" 2>/dev/null)
   if [ "$app_root" = "$root" ] && { [ "$app_exe" = "$root/system/bin/app_process32" ] || [ "$app_exe" = "$root/lab/app-uid-exec" ]; };then kill -KILL "$app_pid" 2>/dev/null;fi
  esac
  rm -f "$root/lab/app-probe.pid"
 fi
 if [ -f "$root/lab/apk-process.log" ];then cat "$root/lab/apk-process.log";fi
 if [ -n "$broker_pid" ];then kill "$broker_pid" 2>/dev/null;wait "$broker_pid" 2>/dev/null;cat "$root/lab/process-broker.log";rm -f "$root/dev/socket/zygote";fi
 if [ -n "$vold_pid" ];then kill "$vold_pid" 2>/dev/null;wait "$vold_pid" 2>/dev/null;cat "$root/lab/vold.log";rm -f "$root/dev/socket/vold" "$root/dev/socket/cryptd";fi
 if [ -n "$sensor_pid" ];then kill "$sensor_pid" 2>/dev/null;wait "$sensor_pid" 2>/dev/null;cat "$root/lab/sensorservice.log";fi
 if [ -n "$installer_pid" ];then kill "$installer_pid" 2>/dev/null;wait "$installer_pid" 2>/dev/null;rm -f "$root/dev/socket/installd";cat "$root/lab/installd.log";fi
 if [ -n "$sf_pid" ];then kill "$sf_pid" 2>/dev/null;wait "$sf_pid" 2>/dev/null;cat "$root/lab/surfaceflinger.log";fi
 if [ -n "$property_pid" ];then kill "$property_pid" 2>/dev/null;wait "$property_pid" 2>/dev/null;rm -f "$root/dev/socket/property_service";cat "$root/lab/property-server.log";fi
 if [ -n "$manager_pid" ]; then kill "$manager_pid" 2>/dev/null;wait "$manager_pid" 2>/dev/null;fi
 if [ -n "$debugger_pid" ];then kill "$debugger_pid" 2>/dev/null;wait "$debugger_pid" 2>/dev/null;tail -80 "$root/lab/debuggerd.log";fi
 if [ -n "$logger_pid" ];then kill "$logger_pid" 2>/dev/null;wait "$logger_pid" 2>/dev/null;cat "$root/lab/android.log";fi
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
chroot "$root" /system/bin/sh -c 'export KOBO_PROPERTY_DAEMON=1 LD_PRELOAD=/lab/install-property-server.so; exec /system/bin/sh -c true' > "$root/lab/property-server.log" 2>&1 &
property_pid=$!
sleep 1
kill -0 "$property_pid"
[ -S "$root/dev/socket/property_service" ] || { cat "$root/lab/property-server.log";exit 8; }
mkdir -p "$root/dev/socket"
"$root/lab/android-log-capture" "$root/dev/socket/logdw" > "$root/lab/android.log" 2>&1 &
logger_pid=$!
sleep 1
mkdir -p "$root/data/tombstones" "$root/data/anr"
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

export ANDROID_ROOT=/system ANDROID_DATA=/data ANDROID_STORAGE=/storage EXTERNAL_STORAGE=/storage/emulated/legacy
export BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar
mkdir -p "$root/data/dalvik-cache/arm"
"$root/lab/vm-helper-smoke" "$root/dev/kobo_madvise" "$root/lab/madvise-private-test"
export CLASSPATH=/lab/install-framework-probe.dex:/system/framework/services.jar
mkdir -p "$root/dev/graphics"
# This is a newly created regular file, never a device node or bind mount.
[ ! -e "$root/dev/graphics/fb0" ] || [ -f "$root/dev/graphics/fb0" ] || exit 9
dd if=/dev/zero of="$root/dev/graphics/fb0" bs=4096 count=1350 2>/dev/null
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/libkobo_fb.so:/lab/virtual-fb-probe.so /system/bin/sh -c true' 
printf 'SurfaceFlinger on the file-backed display:\n'
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/libkobo_fb.so:/lab/libkobo_vm.so exec /system/bin/surfaceflinger' > "$root/lab/surfaceflinger.log" 2>&1 &
sf_pid=$!
sleep 2
kill -0 "$sf_pid"
mkdir -p "$root/data/misc/user" "$root/data/user" "$root/data/user_de/0" "$root/data/app" "$root/data/app-lib" "$root/data/app-private" "$root/data/app-asec" "$root/data/dalvik-cache" "$root/data/system" "$root/data/local/tmp" "$root/system/app" "$root/system/priv-app"
mkdir -p "$root/data/misc_ce" "$root/data/misc_de" "$root/data/data" "$root/data/system_de/0" "$root/data/system_ce/0" "$root/data/misc/profiles/cur/0" "$root/data/misc/profiles/ref"
if [ -d "$root/data/user/0" ] && [ ! -L "$root/data/user/0" ];then rmdir "$root/data/user/0";fi
[ -L "$root/data/user/0" ] || ln -s /data/data "$root/data/user/0"
# Vold sees only experiment-owned mount records and cannot enumerate host PIDs.
if awk -v p="$root/sys" '$2==p || index($2,p "/")==1 {found=1} END {exit !found}' /proc/mounts;then echo UNEXPECTED_SYS_MOUNT;exit 34;fi
[ -z "$(find "$root/dev" -type b -print)" ] || { echo UNEXPECTED_BLOCK_DEVICES;exit 35; }
mkdir -p "$root/dev/block" "$root/data/media" "$root/data/misc/vold" "$root/mnt/secure/asec" "$root/mnt/asec" "$root/mnt/obb" "$root/storage" "$root/lab/vold-owned-proc"
[ -z "$(ls -A "$root/lab/vold-owned-proc")" ] || { echo NONEMPTY_VOLD_PROCESS_SCOPE;exit 36; }
awk -v root="$root" '$2==root || index($2,root "/")==1 {path=substr($2,length(root)+1);$2=(path==""?"/":path);print}' /proc/mounts > "$root/lab/vold-owned-mounts"
chmod 0444 "$root/lab/vold-owned-mounts"
for hardware in '' goldfish ranchu;do
 printf '# Private existing data; no vold-managed disks.\nnone /data ext2 defaults defaults\n' > "$root/fstab.$hardware"
done
"$root/lab/install-vold-socket-exec" "$root" > "$root/lab/vold.log" 2>&1 &
vold_pid=$!
sleep 2
kill -0 "$vold_pid" || { cat "$root/lab/vold.log";exit 37; }
"$root/lab/install-android-socket-exec" "$root" > "$root/lab/installd.log" 2>&1 &
installer_pid=$!
sleep 2
kill -0 "$installer_pid" || { cat "$root/lab/installd.log";exit 13; }
"$root/lab/installd-ping" "$root"
[ ! -e "$root/acct" ] && [ ! -L "$root/acct" ] || { echo UNEXPECTED_ACCT_PATH;exit 14; }
mkdir -p "$root/dev/input"
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/libkobo_vm.so exec /system/bin/sensorservice' > "$root/lab/sensorservice.log" 2>&1 &
sensor_pid=$!
sleep 1
kill -0 "$sensor_pid" || { cat "$root/lab/sensorservice.log";exit 15; }
chmod 0666 "$root/dev/binder" "$root/dev/ashmem" "$root/dev/kobo_madvise" "$root/dev/null" "$root/dev/zero"
rm -f "$root/lab/install-processes.pid" "$root/lab/install-result.txt" "$root/lab/app-probe.pid" "$root/lab/apk-process.log" "$root/lab/activity-window.png" "$root/lab/activity-returned-home.png" "$root/lab/server-entry-error.txt"
[ ! -e "$root/dev/socket/zygote" ] || { echo EXISTING_ZYGOTE_SOCKET;exit 32; }
"$root/lab/install-process-broker" "$root" > "$root/lab/process-broker.log" 2>&1 &
broker_pid=$!
sleep 1
kill -0 "$broker_pid" || { cat "$root/lab/process-broker.log";exit 33; }
KOBO_VM_SHIM=1 timeout 20 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe heap-properties unused
# System metadata changes ownership; app-owned directories keep their UIDs.
mkdir -p "$root/data/resource-cache" "$root/cache"
chown 1000:1000 "$root/data" "$root/data/data" "$root/data/user" "$root/data/user_de" "$root/data/user_de/0" "$root/data/app" "$root/data/app-lib" "$root/data/app-private" "$root/data/app-asec" "$root/data/misc" "$root/data/resource-cache" "$root/cache"
chmod 0771 "$root/data" "$root/data/data" "$root/data/user_de" "$root/data/user_de/0" "$root/data/app"
chown -hR 1000:1000 "$root/data/system" "$root/data/system_ce" "$root/data/system_de" "$root/data/anr" "$root/data/misc/user"
for area in data/data data/user_de/0;do
 for package in android com.android.providers.settings;do
  [ ! -d "$root/$area/$package" ] || chown -hR 1000:1000 "$root/$area/$package"
 done
done
printf 'Android display service bootstrap:\n' 
CLASSPATH=/lab/install-server-launcher.dex KOBO_VM_SHIM=1 "$root/lab/system-server-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin ServerLauncher install-server unused > "$root/lab/display-service.log" 2>&1 &
dms_pid=$!
ready=0
attempt=0
while [ "$attempt" -lt 90 ];do
 attempt=$((attempt+1))
 if grep -q ANDROID_DISPLAY_MANAGER_DEFAULT_READY "$root/lab/display-service.log";then ready=1;break;fi
 kill -0 "$dms_pid" 2>/dev/null || break
 if [ "$attempt" = 50 ];then kill -QUIT "$dms_pid";fi
 sleep 1
done
[ "$ready" = 1 ] || { echo DISPLAY_SERVICE_NOT_READY;cat "$root/data/system/kobo-server-entry-error.txt" 2>/dev/null || :;cat "$root/data/anr/traces.txt" 2>/dev/null || :;exit 10; }
timeout 8 chroot "$root" /system/bin/service list
printf 'Wait for APK Activity resume:\n'
resumed=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30;do
 if grep -q APK_ACTIVITY_ONRESUME "$root/lab/apk-process.log" "$root/lab/android.log" 2>/dev/null;then resumed=1;break;fi
 if grep -q "FATAL EXCEPTION" "$root/lab/android.log";then break;fi
 kill -0 "$dms_pid" 2>/dev/null || break
 sleep 1
done
[ "$resumed" = 1 ] || { echo APK_ACTIVITY_NOT_RESUMED;exit 25; }
sleep 1
KOBO_VM_SHIM=1 timeout 20 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe capture-scanout /lab/activity-window.png
/bin/sh "$root/lab/audit-runtime-fds.sh"
finished=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20;do
 if grep -q APK_ACTIVITY_ONDESTROY "$root/lab/apk-process.log" "$root/lab/android.log" && grep -q APK_ACTIVITY_RETURNED_TO_HOME "$root/lab/apk-process.log" "$root/lab/android.log";then finished=1;break;fi
 sleep 1
done
[ "$finished" = 1 ] || { echo APK_LIFECYCLE_NOT_COMPLETED;exit 26; }
grep -q APK_ACTIVITY_RETURNED_TO_HOME "$root/lab/apk-process.log" "$root/lab/android.log" || { echo APK_HOME_NOT_RESUMED;exit 27; }
sleep 1
KOBO_VM_SHIM=1 timeout 20 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe capture-scanout /lab/activity-returned-home.png
echo ANDROID_REAL_APK_ACTIVITY_LIFECYCLE_OK

printf 'Install an ordinary APK using a real Session:\n'
if [ "${KOBO_INSTALL_REUSE:-0}" = 1 ];then
 [ -n "$previous_installed_uid" ] || { echo NO_PERSISTED_APP_IDENTITY;exit 56; }
 KOBO_VM_SHIM=1 timeout 30 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe launch-installed "$previous_installed_uid"
else
 KOBO_VM_SHIM=1 timeout 100 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe install-apk /lab/kobo-installed-probe.apk
fi
resumed=0
for attempt in $(seq 1 30);do
 if grep -q INSTALLED_APK_ONRESUME "$root/lab/android.log" "$root/lab/apk-process.log";then resumed=1;break;fi
 if grep -q "FATAL EXCEPTION" "$root/lab/android.log";then break;fi
 sleep 1
done
[ "$resumed" = 1 ] || { echo INSTALLED_APP_NOT_RESUMED;exit 54; }
/bin/sh "$root/lab/audit-install-identities.sh"
KOBO_VM_SHIM=1 timeout 20 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe capture-scanout /lab/installed-app.png
/bin/sh "$root/lab/audit-runtime-fds.sh"
finished=0
for attempt in $(seq 1 25);do
 if grep -q INSTALLED_APK_ONDESTROY "$root/lab/android.log" "$root/lab/apk-process.log";then finished=1;break;fi
 sleep 1
done
[ "$finished" = 1 ] || { echo INSTALLED_APP_NOT_DESTROYED;exit 55; }
if [ "${KOBO_INSTALL_REUSE:-0}" = 1 ];then echo ANDROID_ORDINARY_APK_REOPEN_LAUNCH_EXIT_OK;else echo ANDROID_ORDINARY_APK_INSTALL_LAUNCH_EXIT_OK;fi
