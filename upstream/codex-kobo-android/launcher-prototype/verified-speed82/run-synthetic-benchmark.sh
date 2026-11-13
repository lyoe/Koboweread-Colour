set -eu
umask 077
ulimit -c 0
r=/tmp/kobo-android-runtime
base=/mnt/onboard/.adds/kobo-android-lab
[ -d "$base/shelf82-maintenance" ]
[ ! -e "$base/weread-launch.lock" ]
mountpoint -q "$r"
! grep -qE '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules
vm=0;ash=0;binder=0;proc=0;pid=;watcher=;logger=
cleanup(){
 code=$?;trap - EXIT
 if [ -n "$logger" ];then kill "$logger" 2>/dev/null || :;wait "$logger" 2>/dev/null || :;fi
 if [ -n "$watcher" ];then kill "$watcher" 2>/dev/null || :;wait "$watcher" 2>/dev/null || :;fi
 if [ -n "$pid" ] && [ "$(readlink /proc/$pid/exe 2>/dev/null)" = "$r/system/bin/dex2oat" ];then kill -KILL "$pid" 2>/dev/null || :;wait "$pid" 2>/dev/null || :;fi
 [ "$proc" != 1 ] || umount "$r/proc"
 [ "$binder" != 1 ] || rmmod binder_linux
 [ "$vm" != 1 ] || rmmod kobo_madvise
 [ "$ash" != 1 ] || rmmod ashmem_linux
 echo SHELF82_BENCH_EXIT=$code
 exit "$code"
}
trap cleanup EXIT
trap 'exit 143' TERM INT HUP
insmod "$r/lab/ashmem_linux.ko";ash=1
insmod "$r/lab/kobo_madvise.ko";vm=1
insmod "$r/lab/binder_linux.ko";binder=1
for n in ashmem kobo_madvise binder;do
 if [ "$n" = binder ];then dev=$(cat /sys/class/binder/binder/dev);else dev=$(cat /sys/class/misc/$n/dev);fi;rm -f "$r/dev/$n";mknod "$r/dev/$n" c "${dev%:*}" "${dev#*:}";chmod 0600 "$r/dev/$n"
done
mount -t proc -o ro proc "$r/proc";proc=1
export ANDROID_ROOT=/system ANDROID_DATA=/data ANDROID_STORAGE=/storage EXTERNAL_STORAGE=/storage/emulated/legacy
export BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar
export CLASSPATH=/lab/install-framework-probe.dex:/system/framework/services.jar

"$r/lab/android-log-capture" "$r/dev/socket/logdw" > "$r/lab/shelf-bench82-android.log" 2>&1 &
logger=$!
sleep 1
export CLASSPATH=/lab/install-framework-probe.dex:/system/framework/services.jar:/lab/shelf-bench-launcher82.dex
case "${SHELF_BENCH82_MODE:-interpreter}" in
 interpreter) KOBO_VM_SHIM=1 timeout 45 "$r/lab/legacy-exec" "$r" /system/bin/app_process32 -Xint -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx64m /system/bin ShelfBenchLauncher ;;
 jit) KOBO_VM_SHIM=1 timeout 45 "$r/lab/legacy-exec" "$r" /system/bin/sh -c 'export LD_PRELOAD=/lab/shelf-bench-jit82.so:/lab/libkobo_vm.so; exec /system/bin/app_process32 -Xusejit:true -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat -Xms8m -Xmx64m /system/bin ShelfBenchLauncher' ;;
 *) exit 2;;
esac
