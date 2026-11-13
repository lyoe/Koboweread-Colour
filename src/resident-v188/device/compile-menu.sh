#!/bin/sh
# Run by the KOReader menu item "重新编译加速文件" (plugin-main-resident.lua,
# 2026-11-10): compile WeRead 1.8.8's speed-profile odex from the usage profile
# (fast-v188/compile-speed-profile.sh, about a minute, Android must be closed)
# and leave a log the menu reads: first line COMPILE_MENU_BEGIN with the boot
# id (a log from before a reboot is not "running"), last line
# COMPILE_MENU_DONE code=N.
b=/mnt/onboard/.adds/kobo-android-lab;log=$b/resident-v188/compile-menu.log
echo "COMPILE_MENU_BEGIN boot=$(cat /proc/sys/kernel/random/boot_id)" > $log
/bin/sh $b/fast-v188/compile-speed-profile.sh >> $log 2>&1
echo "COMPILE_MENU_DONE code=$?" >> $log
