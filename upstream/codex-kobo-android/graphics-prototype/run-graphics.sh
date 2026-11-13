#!/bin/sh
set -eu
ulimit -c 0
root=/tmp/kobo-android-runtime
cd "$root/lab"
[ "$(uname -r)" = '4.1.15-00286-g2a6a4cb08c66' ] || exit 2
if [ -e /dev/binder ] || [ -e /dev/ashmem ] || grep -qE '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules; then echo 'Existing Android devices; refusing';exit 3;fi
binder_loaded=0;ashmem_loaded=0;vm_loaded=0;manager_pid=;logger_pid=;debugger_pid=;proc_mounted=0
cleanup() {
 set +e
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
chroot "$root" /system/bin/sh -c 'LD_PRELOAD=/lab/property-init.so /system/bin/sh -c true' 
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
printf "ART Java execution:\n"
KOBO_VM_SHIM=1 timeout 30 "$root/lab/legacy-exec" "$root" /system/bin/dalvikvm32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx64m -cp /lab/hello.dex Hello

export CLASSPATH=/lab/canvas-probe.dex
printf 'Android framework tool entry:\n'
KOBO_VM_SHIM=1 timeout 45 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin CanvasProbe hello > "$root/lab/framework-entry.log" 2>&1
cat "$root/lab/framework-entry.log"
grep -q ANDROID_FRAMEWORK_MAIN_OK "$root/lab/framework-entry.log"
echo FRAMEWORK_ENTRY_EXIT_OK
rm -f "$root/lab/canvas-test.png"
printf 'Android Canvas CPU render:\n'
KOBO_VM_SHIM=1 timeout 45 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx128m /system/bin CanvasProbe /lab/canvas-test.png

[ -s "$root/lab/canvas-test.png" ] || { echo CANVAS_OUTPUT_MISSING;exit 6; }
