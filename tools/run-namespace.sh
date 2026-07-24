#!/bin/sh
# Boot the binary as PID 1 inside a user + PID + mount namespace.
#
# Same fixtures and assertions as the qemu test, needing neither root, a kernel
# image, nor an emulator, so it runs on any Linux box and gives the supervision
# paths real coverage locally. What the namespace refuses (devtmpfs, the
# hardware watchdog, a real SYS_reboot) stays the qemu test's job.
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${BUILD:-build/$ARCH}"
TIMEOUT="${TIMEOUT:-60}"
# Staged outside the build tree on purpose. The repo may sit on a filesystem
# without real POSIX semantics (a Windows drive under WSL, a network share), and
# the transcript needs O_APPEND to actually be atomic.
STAGE="${STAGE:-${TMPDIR:-/tmp}/init-nstest}"
LOG="$BUILD/ns-console.log"

if ! command -v unshare >/dev/null 2>&1; then
    echo "SKIP: unshare(1) not installed"
    exit 0
fi
if ! unshare --user --map-root-user true 2>/dev/null; then
    echo "SKIP: unprivileged user namespaces are unavailable here"
    exit 0
fi

sh tools/stage-rootfs.sh "$BUILD" "$STAGE"

# No devtmpfs here, so stand in a regular file for /dev/console. init and the
# fixtures both open it O_APPEND, so the merged transcript stays ordered.
: > "$STAGE/dev/console"
chmod 0666 "$STAGE/dev/console"

echo "booting $BUILD/init as namespace PID 1"
set +e
timeout "$TIMEOUT" unshare --user --map-root-user --mount --pid --fork \
    chroot "$STAGE" /init > "$LOG" 2>&1
RC=$?
set -e

cat "$STAGE/dev/console" >> "$LOG" 2>/dev/null || true

echo "--- console transcript ---"
cat "$LOG"
echo "--- end transcript ---"

fail=0
expect()
{
    if grep -qF "$1" "$LOG"; then
        echo "  ok    $1"
    else
        echo "  MISS  $1"
        fail=1
    fi
}
reject()
{
    if grep -qF "$1" "$LOG"; then
        echo "  BAD   unexpected: $1"
        fail=1
    fi
}

echo "checking markers:"
expect "init $ARCH starting, pid 1"
expect "12 tasks in /tasks"
expect "ok: started pid"
expect "FIXTURE ok started"
expect "FIXTURE probe ran"                 # exec probe fired
expect "FIXTURE oneshot ran"
expect "oneshot: done"                  # a clean boot task is not respawned
# every accepted spelling parses: a regression here logs "not a schedule"
reject "not a schedule"
expect "flap: exit 3 sig 0, respawn in" # exponential backoff engaged
expect "flap: FAILED after"
expect "hangcheck: probe timed out, killing pid"   # unreapable check
expect "hangcheck: restarting on probe failure"    # timeouts counted
expect "hangcheck: FAILED after"                   # and it terminates             # gave up rather than fork-bombing
expect "FIXTURE tick fired"                # interval task on CLOCK_BOOTTIME
expect "FIXTURE logfile begin"
expect "ok-stderr-line"                    # capture -> ring -> writer -> disk
expect "flap-dying"
expect "FIXTURE selftest done"
expect "shutdown requested by signal 10"
expect "syncing"
reject "PANIC"

if [ $RC -eq 124 ]; then
    echo "  BAD   timed out after ${TIMEOUT}s without shutting down"
    fail=1
fi

if [ $fail -ne 0 ]; then
    echo "NAMESPACE BOOT TEST FAILED"
    exit 1
fi
echo "NAMESPACE BOOT TEST PASSED"
