#!/bin/sh
set -eu

BUILD="${1:?usage: run-fault-tests.sh <build-dir>}"

Unavailable()
{
    if [ "${INIT_TEST_REQUIRED:-0}" -ne 0 ]; then
        echo "ERROR: $1" >&2
        exit 1
    fi
    echo "SKIP: $1"
    exit 0
}

if ! command -v unshare >/dev/null 2>&1 ||
   ! command -v timeout >/dev/null 2>&1 ||
   ! command -v strace >/dev/null 2>&1
then
    Unavailable "unshare, timeout and strace are required for fault tests"
fi

if ! unshare --user --map-root-user true 2>/dev/null
then
    Unavailable "unprivileged user namespaces are unavailable"
fi

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/init-fault-tests.XXXXXX")
NULL_LOG="$BUILD/ns-null-device.log"
NULL_TRACE="$BUILD/ns-null-device.trace"
CLOCK_LOG="$BUILD/ns-clock-failure.log"
CLOCK_TRACE="$BUILD/ns-clock-failure.trace"
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

INIT_LOGD_FIXTURE=0 INIT_SNTP_FIXTURE=0 INIT_NS_PRIVILEGE_FIXTURES=0 \
    sh tools/stage-rootfs.sh "$BUILD" "$STAGE"
: > "$STAGE/dev/console"
chmod 0666 "$STAGE/dev/console"
: > "$STAGE/dev/null"
chmod 0666 "$STAGE/dev/null"

rm -f "$NULL_LOG" "$NULL_TRACE"
rm "$STAGE/dev/null"
set +e
timeout 4 strace -f -qq -e trace=execve,exit_group -o "$NULL_TRACE" \
    unshare --user --map-root-user --mount --pid --fork \
    chroot "$STAGE" /init > "$NULL_LOG" 2>&1
rc=$?
set -e
case "$rc" in
    124|129)
        ;;
    *)
        cat "$NULL_LOG"
        echo "FAIL: null-device test exited with status $rc"
        exit 1
        ;;
esac
if ! grep -qF 'init: child open /dev/null failed, errno 2' "$NULL_LOG"; then
    cat "$NULL_LOG"
    echo "FAIL: missing null-device child failure"
    exit 1
fi
if ! grep -qF 'exit_group(126)' "$NULL_TRACE"; then
    cat "$NULL_TRACE"
    echo "FAIL: null-device child did not exit with status 126"
    exit 1
fi
if grep -qF 'FIXTURE ok started' "$NULL_LOG"; then
    cat "$NULL_LOG"
    echo "FAIL: child ran after null-device setup failure"
    exit 1
fi
echo "ok: null-device failure exits the child before exec (timeout rc $rc)"

: > "$STAGE/dev/null"
chmod 0666 "$STAGE/dev/null"
rm -f "$CLOCK_LOG" "$CLOCK_TRACE"
set +e
timeout 4 strace -f -qq -e trace=clock_gettime \
    -e inject=clock_gettime:error=EPERM:when=1 -o "$CLOCK_TRACE" \
    unshare --user --map-root-user --mount --pid --fork \
    chroot "$STAGE" /init > "$CLOCK_LOG" 2>&1
rc=$?
set -e
case "$rc" in
    124|129)
        ;;
    *)
        cat "$CLOCK_LOG"
        echo "FAIL: clock failure test exited with status $rc"
        exit 1
        ;;
esac
if ! grep -qF '= -1 EPERM' "$CLOCK_TRACE"; then
    cat "$CLOCK_TRACE"
    echo "FAIL: clock_gettime failure was not injected"
    exit 1
fi
if ! grep -qF 'init: PANIC: clock_gettime failed' "$CLOCK_LOG"; then
    cat "$CLOCK_LOG"
    echo "FAIL: missing clock failure panic"
    exit 1
fi
echo "ok: clock failure enters the panic path (timeout rc $rc)"
