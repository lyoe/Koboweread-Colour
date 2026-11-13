set -eu
umask 077
ulimit -c 0
r=/tmp/kobo-android-runtime
base=/mnt/onboard/.adds/kobo-android-lab
[ -d "$base/shelf82-maintenance" ]
[ ! -e "$base/weread-launch.lock" ]
mountpoint -q "$r"
! grep -qE '^(binder_linux|ashmem_linux|kobo_madvise) ' /proc/modules
[ "$(sha256sum "$r/lab/weread-selected82.prof" | cut -d' ' -f1)" = 54e2f7f89ddc8572462b206ad9f0787bef79447cdfbcfc5c11042474a667b152 ]
[ "$(sha256sum "$r/data/app/com.tencent.weread.eink-1/base.apk" | cut -d' ' -f1)" = 04d86bcdc1bbd77cab4ecc4d1c04e869d76dbabc79c60c52a707c64fc3e4b75f ]
[ ! -e "$r/lab/weread-selected82.odex" ]
[ "$(df -Pk "$r" | awk 'NR==2 {print $4}')" -ge 145000 ]
vm=0;ash=0;proc=0;pid=;watcher=
cleanup(){
 code=$?;trap - EXIT
 if [ -n "$watcher" ];then kill "$watcher" 2>/dev/null || :;wait "$watcher" 2>/dev/null || :;fi
 if [ -n "$pid" ] && [ "$(readlink /proc/$pid/exe 2>/dev/null)" = "$r/system/bin/dex2oat" ];then kill -KILL "$pid" 2>/dev/null || :;wait "$pid" 2>/dev/null || :;fi
 [ "$proc" != 1 ] || umount "$r/proc"
 [ "$vm" != 1 ] || rmmod kobo_madvise
 [ "$ash" != 1 ] || rmmod ashmem_linux
 echo AOT82_COMPILER_EXIT=$code
 exit "$code"
}
trap cleanup EXIT
trap 'exit 143' TERM INT HUP
insmod "$r/lab/ashmem_linux.ko";ash=1
insmod "$r/lab/kobo_madvise.ko";vm=1
for n in ashmem kobo_madvise;do
 dev=$(cat /sys/class/misc/$n/dev);rm -f "$r/dev/$n";mknod "$r/dev/$n" c "${dev%:*}" "${dev#*:}";chmod 0600 "$r/dev/$n"
done
mount -t proc -o ro proc "$r/proc";proc=1
export ANDROID_ROOT=/system ANDROID_DATA=/data ANDROID_STORAGE=/storage EXTERNAL_STORAGE=/storage/emulated/legacy
export BOOTCLASSPATH=/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar
export CLASSPATH=/lab/install-framework-probe.dex:/system/framework/services.jar
start=$(cut -d' ' -f1 /proc/uptime)
KOBO_VM_SHIM=1 nice -n 19 /bin/sh -c 'echo 800 > /proc/$$/oom_score_adj; exec "$@"' sh "$r/lab/legacy-exec" "$r" /system/bin/dex2oat --dex-file=/data/app/com.tencent.weread.eink-1/base.apk --oat-file=/lab/weread-selected82.odex --oat-location=/data/app/com.tencent.weread.eink-1/oat/arm/base.odex --instruction-set=arm --instruction-set-variant=cortex-a9 --compiler-filter=speed-profile --profile-file=/lab/weread-selected82.prof --compile-pic --runtime-arg -Xms16m --runtime-arg -Xmx96m --runtime-arg -Xnorelocate -j1 > "$r/lab/aot82-compiler.log" 2>&1 &
pid=$!
(
 ticks=0
 while kill -0 "$pid" 2>/dev/null;do
  sleep 2;ticks=$((ticks+1))
  rss=$(awk '$1=="VmRSS:" {print $2}' /proc/$pid/status 2>/dev/null || :)
  bytes=$(stat -c %s "$r/lab/weread-selected82.odex" 2>/dev/null || echo 0)
  avail=$(awk '$1=="MemAvailable:" {print $2}' /proc/meminfo)
  free=$(df -Pk "$r" | awk 'NR==2 {print $4}')
  echo "AOT82_RESOURCE tick=$ticks rss_kib=${rss:-0} output_bytes=$bytes free_kib=$free available_kib=$avail"
  if [ "$avail" -lt 98304 ] || [ "$ticks" -ge 300 ] || [ "${rss:-0}" -gt 307200 ] || [ "$bytes" -gt 110100480 ] || [ "$free" -lt 32768 ];then
   if [ "$(readlink /proc/$pid/exe 2>/dev/null)" = "$r/system/bin/dex2oat" ];then echo AOT82_RESOURCE_LIMIT;kill -KILL "$pid";fi
   break
  fi
 done
) &
watcher=$!
result=0;wait "$pid" || result=$?;pid=
kill "$watcher" 2>/dev/null || :;wait "$watcher" 2>/dev/null || :;watcher=
echo "AOT82_COMPILE_RESULT code=$result start=$start finish=$(cut -d' ' -f1 /proc/uptime)"
cat "$r/lab/aot82-compiler.log"
[ "$result" = 0 ]
sha256sum "$r/lab/weread-selected82.odex"
stat -c 'AOT82_OUTPUT_BYTES=%s' "$r/lab/weread-selected82.odex"
