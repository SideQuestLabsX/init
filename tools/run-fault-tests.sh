#!/bin/sh
set -eu

BUILD="${1:?usage: run-fault-tests.sh <build-dir>}"

if ! command -v unshare >/dev/null 2>&1 ||
   ! command -v timeout >/dev/null 2>&1 ||
   ! command -v strace >/dev/null 2>&1
then
    echo "SKIP: unshare, timeout and strace are required for fault tests"
    exit 0
fi

if ! unshare --user --map-root-user true 2>/dev/null
then
    echo "SKIP: unprivileged user namespaces are unavailable"
    exit 0
fi

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/init-fault-tests.XXXXXX")
NULL_LOG="$BUILD/ns-null-device.log"
CLOCK_LOG="$BUILD/ns-clock-failure.log"
CLOCK_TRACE="$BUILD/ns-clock-failure.trace"
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

INIT_LOGD_FIXTURE=0 INIT_SNTP_FIXTURE=0 INIT_NS_PRIVILEGE_FIXTURES=0 \
    sh tools/stage-rootfs.sh "$BUILD" "$STAGE"
: > "$STAGE/dev/console"
chmod 0666 "$STAGE/dev/console"
: > "$STAGE/dev/null"
chmod 0666 "$STAGE/dev/null"

rm -f "$NULL_LOG"
rm "$STAGE/dev/null"
set +e
timeout 4 unshare --user --map-root-user --mount --pid --fork \
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
