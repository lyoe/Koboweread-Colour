#!/bin/sh
# Journal mode of WeRead 1.8.8's account databases (WRBook, WRReader), set
# offline in graphics-v188.img while Android is down:
#   sh v188-wal.sh status|wal|delete [v198]
# With v198 the same is done in 1.9.8's graphics-v198.img (its wrapper sets
# no journal mode either; backups then go to resident-v188/wal-backup-v198/).
# WeRead's SQLite wrapper (com.tencent.moai.database) never sets a journal
# mode on these (getDefaultJournalMode is unused; only databases that ask
# for WAL, like WRAccount, get one), so the mode stored in the file stays.
# Uses KOReader's SQLite through its luajit (v188-wal.lua). A database with
# a leftover -wal/-journal is opened the same way (SQLite recovers it
# first); the conversion is skipped when quick_check is not "ok". On the
# first "wal" each file is copied to resident-v188/wal-backup/ (device
# only). Afterwards no -wal/-shm/-journal is left behind, so the app creates
# its own with its own owner and labels.
set -u
b=/mnt/onboard/.adds/kobo-android-lab; res=$b/resident-v188; m=/tmp/v188-wal-mount; k=/mnt/onboard/.adds/koreader
pkg=com.tencent.weread.eink; want=${1:?status|wal|delete}; img=${2:-v188}
case "$want" in status|wal|delete) ;; *) exit 2;; esac
case "$img" in v188) bk=$res/wal-backup;; v198) bk=$res/wal-backup-v198;; *) exit 2;; esac
for v in resident-v188;do [ "$(sh $b/$v/resident-ctl.sh status)" = none ] || { echo WAL_BUSY $v;exit 3; };done
mkdir $b/weread-launch.lock 2>/dev/null || { echo WAL_LOCK_BUSY;exit 3; }
printf '%s %s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" "$(cat /proc/sys/kernel/random/boot_id)" > $b/weread-launch.lock/owner
release(){ umount $m 2>/dev/null;rmdir $m 2>/dev/null;rm -f $b/weread-launch.lock/owner;rmdir $b/weread-launch.lock; }
trap release EXIT
grep -q graphics /proc/mounts && { echo WAL_IMAGE_MOUNTED;exit 3; }
opt=noatime,user_xattr
# Image file system (2026-11-10): images are built as ext4 with a journal;
# older images without one (has_journal, bit 2 of the compat word at byte
# 1116) are still mounted as ext2.
img_fstype(){ j_=$(dd if="$1" bs=1 skip=1116 count=1 2>/dev/null | od -An -tu1 | tr -d ' ');[ $(( ${j_:-0} & 4 )) = 0 ] && echo ext2 || echo ext4; }
mkdir -p $m && mount -t $(img_fstype $b/graphics-$img.img) -o loop,$opt $b/graphics-$img.img $m || { echo WAL_MOUNT_FAILED;exit 4; }
rc=0;n=0
for f in $m/data/data/$pkg/databases/[0-9]*/WRBook $m/data/data/$pkg/databases/[0-9]*/WRReader;do
 [ -f "$f" ] || continue;n=$((n+1));name=$(basename $f);own=$(stat -c '%u:%g:%a' $f)
 if [ $want = wal ] && [ ! -e $bk/$name ];then
  for s in -wal -journal;do [ -e "$f$s" ] && echo "WAL_PENDING $name$s";done
  mkdir -p $bk && cp $f $bk/$name.tmp && mv $bk/$name.tmp $bk/$name || { echo WAL_BACKUP_FAILED $name;rc=5;continue; }
 fi
 out=$(cd /tmp && LD_LIBRARY_PATH=$k/libs $k/luajit $res/v188-wal.lua $f $want 2>&1);echo "$out" | sed "s/^/$name /"
 echo "$out" | grep -q '^WAL_DB' || rc=6
 [ "$(stat -c '%u:%g:%a' $f)" = "$own" ] || { chown ${own%:*} $f;chmod ${own##*:} $f;echo "WAL_OWNER_RESTORED $name $own"; }
 left=$(ls -A $(dirname $f) | grep -E "^$name-(wal|shm|journal)$" | tr '\n' ' ');[ -z "$left" ] || { echo "WAL_LEFTOVER $left";rc=7; }
done
[ $n -gt 0 ] || { echo WAL_NO_DATABASES;rc=8; }
sync;umount $m || { echo WAL_UMOUNT_FAILED;exit 9; }
echo "WAL_DONE want=$want image=$img rc=$rc";exit $rc
