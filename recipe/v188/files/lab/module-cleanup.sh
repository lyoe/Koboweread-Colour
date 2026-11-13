# Only modules explicitly loaded by this run may be passed by its cleanup.
lab_module_present() { grep -q "^$1 " /proc/modules; }
lab_unload_module() {
 case "$1" in binder_linux|ashmem_linux|kobo_madvise) :;; *) return 2;;esac
 lab_module=$1
 for lab_attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20;do
  lab_module_present "$lab_module" || return 0
  rmmod "$lab_module" 2>/dev/null && return 0
  [ "$lab_attempt" = 20 ] || sleep 0.1
 done
 if lab_module_present "$lab_module";then echo "LAB_MODULE_UNLOAD_STILL_BUSY module=$lab_module";return 1;fi
 return 0
}
