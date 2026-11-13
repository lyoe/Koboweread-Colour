#!/bin/sh
# Host-only Android API fixtures; none are installed on the device.
set -eu
: "${KOBO_JAVA_HOME:?Set a JDK 17 directory}"
: "${KOBO_BUILD_OUT:?Set an intermediate host-test directory}"
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT"
set -- "$here/host-stubs/android/content/"*.java "$here/host-stubs/android/os/"*.java "$here/host-stubs/android/system/"*.java "$here/host-stubs/com/android/server/am/"*.java
"$KOBO_JAVA_HOME/bin/javac" -d "$KOBO_BUILD_OUT" "$@" "$here/../../framework-prototype/StartupServiceGracePolicy.java" "$here/../../framework-prototype/StartupServiceGrace.java" "$here/StartupServiceGracePolicyTest.java" "$here/StartupServiceGraceContractTest.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_BUILD_OUT" StartupServiceGracePolicyTest
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_BUILD_OUT" StartupServiceGraceContractTest
