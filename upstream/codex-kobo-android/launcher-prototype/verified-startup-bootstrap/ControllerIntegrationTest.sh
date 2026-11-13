#!/bin/sh
# Isolated controller test. Caller supplies the launcher fixture with base and
# finish paths changed to this /tmp directory, and polling sleep shortened.
# Does not mount anything, start Android or call input/power APIs.
set -eu
b=/tmp/kobo-startup68-controller-test
fixture=${1:?launcher fixture path}
[ ! -e "$b" ] || exit 90
mkdir "$b"
trap 'rm -rf "$b"' EXIT
cp "$fixture" "$b/launch.sh"
: > "$b/graphics.img";: > "$b/fd-clean-exec"
cat > "$b/session-status.sh" <<'EOF'
#!/bin/sh
if test -f /tmp/kobo-startup68-controller-test/active;then echo "WEREAD_INTERACTIVE_SESSION_ACTIVE remaining_seconds=170";else echo INACTIVE;fi
EOF
cat > "$b/run-weread-session.sh" <<'EOF'
#!/bin/sh
set -eu
b=/tmp/kobo-startup68-controller-test
n=0;test ! -f "$b/count" || read n < "$b/count"
n=$((n+1));echo "$n" > "$b/count"
trap 'rm -f "$b/active" "$b/finish"' EXIT
if test -f "$b/fail" && test "$n" = 3;then exit 71;fi
: > "$b/active"
echo 'WEREAD_STARTUP_STAGE stage=interactive_ready uptime=1 profile=reading'
if test -f "$b/cancel";then
 sleep 0.08
 echo HOST_EXIT_CORNER_HOLD_RELEASED
else
 attempts=0
 while test ! -f "$b/finish";do
  attempts=$((attempts+1));test "$attempts" -lt 300 || exit 72
  sleep 0.02
 done
 grep -q '^reason=developer-startup-check$' "$b/finish"
 test "$(stat -c %a "$b/finish")" = 400
fi
echo LIVE_DISPLAY_FRAMEBUFFER_RESTORE_BYTES_MATCH
EOF
/bin/sh "$b/launch.sh" startup-checks & controller=$!
sleep 0.06
set +e
/bin/sh "$b/launch.sh" startup-checks
second=$?
set -e
test "$second" = 5
read owner start boot < "$b/weread-launch.lock/owner"
test "$owner" = "$controller"
wait "$controller"
grep -q '^completed_rounds=10$' "$b/startup-validation-result.txt"
test "$(grep -c '^round=.* result=0 ready_seen=1 developer_finish=1$' "$b/startup-validation-result.txt")" = 10
test ! -e "$b/weread-launch.lock"
rm "$b/count"
: > "$b/fail"
set +e
/bin/sh "$b/launch.sh" startup-checks
failure=$?
set -e
test "$failure" = 71
grep -q '^completed_rounds=2$' "$b/startup-validation-result.txt"
grep -q '^exit=71$' "$b/weread-launch.state"
test ! -e "$b/weread-launch.lock"
rm "$b/count" "$b/fail"
: > "$b/cancel"
/bin/sh "$b/launch.sh" startup-checks
grep -q '^cancelled_by_user=1$' "$b/startup-validation-result.txt"
grep -q '^completed_rounds=1$' "$b/startup-validation-result.txt"
grep -q '^stage=check_cancelled$' "$b/weread-launch.state"
test ! -e "$b/weread-launch.lock"
echo CONTROLLER_INTEGRATION_OK ten_rounds duplicate_lock failure_stops user_exit_stops root_readonly_finish no_android_or_input
