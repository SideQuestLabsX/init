#!/bin/sh
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${BUILD:-build/$ARCH}"
TIMEOUT="${TIMEOUT:-}"
WATCHDOG_TEST="${WATCHDOG_TEST:-0}"
LOG="$BUILD/qemu-console.log"
KERNEL="${KERNEL:-}"
DTB="${DTB:-}"
BIOS="${BIOS:-}"

Unavailable()
{
    if [ "${INIT_TEST_REQUIRED:-0}" -ne 0 ]; then
        echo "ERROR: $1" >&2
        exit 1
    fi
    echo "SKIP: $1"
    exit 0
}

unset MARKER_LOGD MARKER_NS_TIER MARKER_CHILD_ERROR MARKER_PROBES \
      MARKER_LOGFILE MARKER_CAPTURE MARKER_SNTP MARKER_STATUS \
      MARKER_STATUS_FALLBACK MARKER_DISCOVERY MARKER_NAMESPACE \
      MARKER_SNTP_CLOCK_SET EXPECT_TASKS

MARKER_ARCH="$ARCH"
MARKER_SHUTDOWN_SIGNAL=10
NEEDS_DTB=0
NEEDS_BIOS=0
CPU=

case "$ARCH" in
    x86)
        QEMU=qemu-system-i386; MACHINE=q35; CONSOLE=ttyS0
        ;;
    x86_64) QEMU=qemu-system-x86_64; MACHINE=q35; CONSOLE=ttyS0 ;;
    aarch64)
        QEMU=qemu-system-aarch64; MACHINE=virt; CPU=cortex-a57; CONSOLE=ttyAMA0
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    armv6)
        QEMU=qemu-system-arm; MACHINE=realview-eb; CPU=arm1136; CONSOLE=ttyAMA0
        MARKER_ARCH=arm
        NEEDS_DTB=1
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    armv7)
        QEMU=qemu-system-arm; MACHINE=virt,highmem=off; CPU=cortex-a15; CONSOLE=ttyAMA0
        MARKER_ARCH=arm
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    riscv64)
        QEMU=qemu-system-riscv64; MACHINE=virt; CONSOLE=ttyS0
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    loongarch64)
        QEMU=qemu-system-loongarch64; MACHINE=virt; CPU=la464; CONSOLE=ttyS0
        NEEDS_BIOS=1
        MEMORY="${MEMORY:-2G}"
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    mips)
        QEMU=qemu-system-mips; MACHINE=malta; CONSOLE=ttyS0
        MARKER_SHUTDOWN_SIGNAL=16
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    mipsel)
        QEMU=qemu-system-mipsel; MACHINE=malta; CONSOLE=ttyS0
        MARKER_SHUTDOWN_SIGNAL=16
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    *) Unavailable "no qemu boot test for ARCH=$ARCH" ;;
esac

if [ "$WATCHDOG_TEST" -ne 0 ] && [ "$ARCH" != x86_64 ]; then
    echo "ERROR: hardware watchdog test requires ARCH=x86_64"
    exit 2
fi

TIMEOUT="${TIMEOUT:-90}"
MEMORY="${MEMORY:-256}"

if ! command -v "$QEMU" >/dev/null 2>&1; then
    Unavailable "$QEMU not installed"
fi
if ! command -v python3 >/dev/null 2>&1; then
    Unavailable "python3 not installed (needed to pack the initramfs)"
fi

if [ -z "$KERNEL" ]; then
    case "$ARCH" in
        x86_64|aarch64)
            for candidate in "/boot/vmlinuz-$(uname -r)" /boot/vmlinuz /boot/vmlinuz-*; do
                if [ -r "$candidate" ]; then
                    KERNEL="$candidate"
                    break
                fi
            done
            ;;
        x86)
            Unavailable "x86 boot requires a 32-bit kernel; set KERNEL=/path/to/vmlinuz"
            ;;
        *)
            Unavailable "$ARCH boot requires an explicit kernel image"
            ;;
    esac
fi
if [ -z "$KERNEL" ] || [ ! -r "$KERNEL" ]; then
    Unavailable "no readable kernel image; set KERNEL=/path/to/vmlinuz"
fi
if [ "$NEEDS_DTB" -ne 0 ] && { [ -z "$DTB" ] || [ ! -r "$DTB" ]; }; then
    Unavailable "$ARCH boot requires a readable DTB; set DTB=/path/to/board.dtb"
fi
if [ "$NEEDS_BIOS" -ne 0 ] && { [ -z "$BIOS" ] || [ ! -r "$BIOS" ]; }; then
    Unavailable "$ARCH boot requires a readable UEFI firmware; set BIOS=/path/to/QEMU_EFI.fd"
fi

env INIT_WATCHDOG_TEST="$WATCHDOG_TEST" BUILD="$BUILD" \
    OUT="$BUILD/initramfs.cpio" sh tools/mkinitramfs.sh

echo "booting $KERNEL under $QEMU"
set +e
set -- "$QEMU" -machine "$MACHINE" -m "$MEMORY"
if [ -n "$CPU" ]; then
    set -- "$@" -cpu "$CPU"
fi
if [ -n "$BIOS" ]; then
    set -- "$@" -bios "$BIOS"
fi
if [ -n "$DTB" ]; then
    set -- "$@" -dtb "$DTB"
fi
if [ "$WATCHDOG_TEST" -ne 0 ]; then
    set -- "$@" -device i6300esb -watchdog-action reset
else
    set -- "$@" -no-reboot
fi
set -- "$@" \
    -kernel "$KERNEL" \
    -initrd "$BUILD/initramfs.cpio" \
    -append "console=$CONSOLE panic=1 loglevel=4" \
    -nographic -serial mon:stdio -display none
"$@" > "$LOG" 2>&1 &
QEMU_PID=$!
QEMU_RC=0
QEMU_COMPLETE=0
QEMU_TIMED_OUT=0
QEMU_ELAPSED=0
while kill -0 "$QEMU_PID" 2>/dev/null; do
    if [ "$WATCHDOG_TEST" -ne 0 ]; then
        boot_count=$(grep -cF "watchdog armed" "$LOG" 2>/dev/null || true)
        if [ "$boot_count" -ge 2 ]; then
            QEMU_COMPLETE=1
            kill "$QEMU_PID" 2>/dev/null
            break
        fi
    else
        if grep -qF "reboot: Restarting system" "$LOG" 2>/dev/null; then
            QEMU_COMPLETE=1
            kill "$QEMU_PID" 2>/dev/null
            break
        fi
    fi
    if [ "$QEMU_ELAPSED" -ge "$TIMEOUT" ]; then
        QEMU_TIMED_OUT=1
        kill "$QEMU_PID" 2>/dev/null
        break
    fi
    sleep 1
    QEMU_ELAPSED=$((QEMU_ELAPSED + 1))
done
wait "$QEMU_PID"
QEMU_CHILD_RC=$?
if [ "$QEMU_COMPLETE" -eq 0 ]; then
    if [ "$WATCHDOG_TEST" -ne 0 ]; then
        boot_count=$(grep -cF "watchdog armed" "$LOG" 2>/dev/null || true)
        if [ "$boot_count" -ge 2 ]; then
            QEMU_COMPLETE=1
        fi
    elif grep -qF "reboot: Restarting system" "$LOG" 2>/dev/null; then
        QEMU_COMPLETE=1
    fi
fi
if [ "$QEMU_COMPLETE" -ne 0 ]; then
    QEMU_RC=0
elif [ "$QEMU_TIMED_OUT" -ne 0 ]; then
    QEMU_RC=124
else
    QEMU_RC=$QEMU_CHILD_RC
fi
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

if [ "$WATCHDOG_TEST" -ne 0 ]; then
    echo "checking watchdog markers:"

    armed_count=$(grep -cF "watchdog armed, 2s timeout" "$LOG" 2>/dev/null || true)
    if [ "$armed_count" -ge 2 ]; then
        echo "  ok    watchdog armed on two boots"
    else
        echo "  MISS  watchdog armed on two boots"
        fail=1
    fi
    expect "WATCHDOG task started"
    expect "watchdog: probe failed ("

    if [ $QEMU_RC -ne 0 ]; then
        echo "  BAD   qemu exited $QEMU_RC before watchdog reset was observed"
        fail=1
    else
        echo "  ok    qemu remained alive through the watchdog reset"
    fi

    if [ $fail -ne 0 ]; then
        echo "QEMU WATCHDOG TEST FAILED"
        exit 1
    fi
    echo "QEMU WATCHDOG TEST PASSED"
    exit 0
fi

echo "checking markers:"
export MARKER_ARCH
export MARKER_SHUTDOWN_SIGNAL
. tools/boot-markers.sh

if [ "$QEMU_COMPLETE" -eq 0 ]; then
    echo "  BAD   qemu exited before the reboot marker"
    fail=1
else
    echo "  ok    reboot marker observed"
fi

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
