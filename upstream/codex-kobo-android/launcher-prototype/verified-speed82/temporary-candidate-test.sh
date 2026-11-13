set -eu
r=/tmp/kobo-android-runtime
old=$r/data/app/com.tencent.weread.eink-1/oat/arm/base.odex
new=$r/lab/weread-selected82.odex
[ ! -e /mnt/onboard/.adds/kobo-android-lab/weread-launch.lock ]
[ "$(sha256sum "$new" | cut -d' ' -f1)" = 271f9f648e1e76e5c11cadc1cab1cfa0548063fc1af7fb2048badd47670e7d3d ]
! awk -v p="$old" '$2==p {found=1} END {exit !found}' /proc/mounts
mount -o bind "$new" "$old"
cleanup(){ code=$?;trap - EXIT;umount "$old";exit "$code"; }
trap cleanup EXIT
trap 'exit 143' TERM INT HUP
echo CANDIDATE82_INTERPRETER_BEGIN
SHELF_BENCH82_MODE=interpreter sh "$r/lab/run-shelf-bench82.sh"
echo CANDIDATE82_JIT_BEGIN
SHELF_BENCH82_MODE=jit sh "$r/lab/run-shelf-bench82.sh"
echo CANDIDATE82_TEMPORARY_BIND_TEST_COMPLETE
