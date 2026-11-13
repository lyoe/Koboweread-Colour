#!/bin/sh
# Private bounded ART diagnostics for the two fixed, verified app processes.
set -eu
[ "$#" = 0 ] && [ "$(id -u)" = 0 ] || exit 2
r=/tmp/kobo-android-runtime
file=$r/data/anr/traces.txt
old=$r/data/anr/traces.before-reader-probe
[ ! -e "$old" ] && [ ! -L "$old" ] && [ ! -L "$file" ] || exit 3
saved=0
[ ! -f "$file" ] || { mv "$file" "$old";saved=1; }
cleanup(){
 code=$?;trap - EXIT
 rm -f "$file"
 [ "$saved" != 1 ] || mv "$old" "$file"
 exit "$code"
}
trap cleanup EXIT
for kind in main seg;do
 name=com.tencent.weread.eink
 [ "$kind" = main ] || name=$name:seg
 target=
 while read p;do
  case "$p" in ''|*[!0-9]*) continue;;esac
  [ "$(readlink /proc/$p/root 2>/dev/null || :)" = "$r" ] || continue
  [ "$(readlink /proc/$p/exe 2>/dev/null || :)" = "$r/system/bin/app_process32" ] || continue
  [ "$(awk '/^Uid:/ {print $2}' /proc/$p/status 2>/dev/null || :)" = 10007 ] || continue
  [ "$(tr '\000' '\n' < /proc/$p/cmdline | head -n1)" = "$name" ] || continue
  [ -z "$target" ] || exit 4
  target=$p;start=$(awk '{print $22}' /proc/$p/stat)
 done < "$r/lab/install-processes.pid"
 [ -n "$target" ] || { echo "READER_PRIVATE_STACK_MISSING kind=$kind";continue; }
 : > "$file";chown 10007:10007 "$file";chmod 0600 "$file"
 [ "$(awk '{print $22}' /proc/$target/stat 2>/dev/null || :)" = "$start" ] || exit 5
 kill -QUIT "$target"
 complete=0
 for attempt in 1 2 3 4 5 6 7 8 9 10;do
  if grep -q -- "----- end $target -----" "$file";then complete=1;break;fi
  sleep 1
 done
 mv "$file" "$r/lab/private-reader-$kind-threads.txt"
 chown 0:0 "$r/lab/private-reader-$kind-threads.txt";chmod 0600 "$r/lab/private-reader-$kind-threads.txt"
 echo "READER_PRIVATE_STACK_CAPTURE kind=$kind complete=$complete"
done
