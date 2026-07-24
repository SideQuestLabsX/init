#!/bin/sh
# Boot the freestanding binary as PID 1 under qemu and assert on the console
# transcript.
#
# This is the half of the test suite the host unit tests cannot reach: mounting,
# forking, reaping, backoff, probes, the shared ring, the forked disk writer and
# the shutdown path only mean anything against a real kernel.
#
# Skips with a clear message when qemu or a kernel image is missing, so a
# developer without them still gets a useful `make test`.
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${BUILD:-build/$ARCH}"
TIMEOUT="${TIMEOUT:-90}"
LOG="$BUILD/qemu-console.log"

case "$ARCH" in
    x86_64) QEMU=qemu-system-x86_64; MACHINE="-machine q35"; CONSOLE=ttyS0 ;;
    aarch64) QEMU=qemu-system-aarch64; MACHINE="-machine virt -cpu cortex-a57"; CONSOLE=ttyAMA0 ;;
    *) echo "SKIP: no qemu boot test for ARCH=$ARCH"; exit 0 ;;
esac

if ! command -v "$QEMU" >/dev/null 2>&1; then
    echo "SKIP: $QEMU not installed"
    exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: python3 not installed (needed to pack the initramfs)"
    exit 0
fi

KERNEL="${KERNEL:-}"
if [ -z "$KERNEL" ]; then
    for candidate in "/boot/vmlinuz-$(uname -r)" /boot/vmlinuz /boot/vmlinuz-*; do
        if [ -r "$candidate" ]; then
            KERNEL="$candidate"
            break
        fi
    done
fi
if [ -z "$KERNEL" ] || [ ! -r "$KERNEL" ]; then
    echo "SKIP: no readable kernel image; set KERNEL=/path/to/vmlinuz"
    exit 0
fi

BUILD="$BUILD" OUT="$BUILD/initramfs.cpio" sh tools/mkinitramfs.sh

echo "booting $KERNEL under $QEMU"
set +e
timeout "$TIMEOUT" "$QEMU" \
    $MACHINE \
    -m 256 \
    -kernel "$KERNEL" \
    -initrd "$BUILD/initramfs.cpio" \
    -append "console=$CONSOLE panic=1 loglevel=4" \
    -nographic -no-reboot -serial mon:stdio -display none \
    > "$LOG" 2>&1
QEMU_RC=$?
set -e

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
reject "Kernel panic"

if [ $QEMU_RC -ne 0 ]; then
    echo "  BAD   qemu exited $QEMU_RC (expected 0 from the reboot syscall)"
    fail=1
else
    echo "  ok    qemu exited on the reboot syscall"
fi

if [ $fail -ne 0 ]; then
    echo "QEMU BOOT TEST FAILED"
    exit 1
fi
echo "QEMU BOOT TEST PASSED"
