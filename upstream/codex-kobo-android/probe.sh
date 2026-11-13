#!/bin/sh
# Read-only capability inventory. Report goes to stdout; never loads modules.
set -u
section() { printf '\n=== %s ===\n' "$1"; }
show() { if [ -r "$1" ]; then printf '\n-- %s --\n' "$1"; cat "$1"; fi; }
section 'Identity'
uname -a
id
show /sys/devices/soc0/machine
show /sys/devices/soc0/soc_id
# Firmware string only; omit the serial number in the first field.
if [ -r /mnt/onboard/.kobo/version ]; then
    printf '\nFirmware: '; cut -d, -f3 /mnt/onboard/.kobo/version
fi
section 'CPU and memory'
if [ -r /proc/cpuinfo ]; then
    grep -E '^(Processor|model name|CPU architecture|CPU implementer|CPU part|Features|Hardware)' /proc/cpuinfo
fi
show /proc/meminfo
section 'Kernel configuration (absence is UNKNOWN, not unsupported)'
pattern='^(CONFIG_(ANDROID.*|ASHMEM|MEMFD.*|NAMESPACES|.*_NS|CGROUP.*|SECCOMP.*|DEVTMPFS.*|TMPFS.*|EXT[234]_FS.*|BLK_DEV_LOOP.*|MODULES|MODVERSIONS|IKCONFIG.*|SWAP|ZRAM.*|VETH|BRIDGE.*|NETFILTER.*)=|# CONFIG_(ANDROID.*|ASHMEM|MEMFD.*|NAMESPACES|.*_NS|CGROUP.*|SECCOMP.*|DEVTMPFS.*|TMPFS.*|EXT[234]_FS.*|BLK_DEV_LOOP.*|MODULES|MODVERSIONS|IKCONFIG.*|SWAP|ZRAM.*|VETH|BRIDGE.*|NETFILTER.*) is not set)'
if [ -r /proc/config.gz ] && command -v gzip >/dev/null 2>&1; then
    gzip -dc /proc/config.gz | grep -E "$pattern"
elif [ -r "/boot/config-$(uname -r)" ]; then
    grep -E "$pattern" "/boot/config-$(uname -r)"
else
    printf 'UNKNOWN: running kernel configuration is not exposed.\n'
fi
section 'Existing Android and graphics device nodes'
for node in /dev/binder /dev/hwbinder /dev/vndbinder /dev/ashmem /dev/binderfs /dev/fb0 /dev/graphics/fb0 /dev/dri /dev/uinput; do
    if [ -e "$node" ]; then ls -ld "$node"; else printf 'Not present: %s\n' "$node"; fi
done
section 'Modules, filesystems, namespaces, cgroups'
show /proc/modules
show /proc/filesystems
show /proc/cgroups
ls -l /proc/self/ns 2>/dev/null || :
if [ -d /lib/modules ]; then find /lib/modules -type f -name '*.ko*'; fi
section 'Storage and current mounts'
df -k /mnt/onboard /tmp 2>/dev/null || :
show /proc/mounts
section 'Display and input metadata (no input events recorded)'
show /proc/bus/input/devices
for path in /sys/class/graphics/fb0/name /sys/class/graphics/fb0/virtual_size /sys/class/graphics/fb0/bits_per_pixel /sys/class/graphics/fb0/rotate; do show "$path"; done
section 'Available tools'
for tool in busybox chroot unshare mount losetup insmod modprobe fbink nickel; do
    command -v "$tool" 2>/dev/null || :
done
section 'End (this inventory does not prove Android will boot)'
