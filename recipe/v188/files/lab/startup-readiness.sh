#!/bin/sh
# Fixed service readiness checks; no synthetic network or application results.
readiness_alive(){
 case "$1" in ''|*[!0-9]*) return 1;;esac
 [ "$1" -gt 1 ] && kill -0 "$1" 2>/dev/null || return 1
 ready_state=$(awk '$1=="State:" {print $2}' "/proc/$1/status" 2>/dev/null)
 case "$ready_state" in R|S|D|I) return 0;;*) return 1;;esac
}
wait_log_ready(){
 [ "$#" = 6 ] || return 2
 ready_label=$1;ready_pid=$2;ready_log=$3;ready_pattern=$4;ready_kind=$5;ready_path=$6
 case "$ready_kind" in file|socket) :;;*) return 2;;esac
 ready_attempt=0
 while [ "$ready_attempt" -lt 50 ];do
  readiness_alive "$ready_pid" || { echo "STARTUP_COMPONENT_EXITED component=$ready_label";return 1; }
  if grep -Fq "$ready_pattern" "$ready_log" 2>/dev/null;then
   if { [ "$ready_kind" = file ] && [ -f "$ready_path" ]; } || { [ "$ready_kind" = socket ] && [ -S "$ready_path" ]; };then
    echo "STARTUP_COMPONENT_READY component=$ready_label attempts=$ready_attempt";return 0
   fi
  fi
  ready_attempt=$((ready_attempt+1));sleep 0.1
 done
 echo "STARTUP_COMPONENT_TIMEOUT component=$ready_label";return 1
}
wait_installd_ready(){
 [ "$#" = 2 ] || return 2
 ready_pid=$1;ready_root=$2
 ready_deadline=$(awk '{printf "%d",$1+8}' /proc/uptime)
 while [ "$(awk '{printf "%d",$1}' /proc/uptime)" -lt "$ready_deadline" ];do
  readiness_alive "$ready_pid" || { echo STARTUP_COMPONENT_EXITED component=installd;return 1; }
  if [ -S "$ready_root/dev/socket/installd" ] && timeout 1 "$ready_root/lab/installd-ping" "$ready_root" --ping-only > "$ready_root/lab/installd-readiness.log" 2>&1;then
   cat "$ready_root/lab/installd-readiness.log";echo STARTUP_COMPONENT_READY component=installd;return 0
  fi
  sleep 0.1
 done
 echo STARTUP_COMPONENT_TIMEOUT component=installd;return 1
}
