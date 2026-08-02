#!/bin/sh
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${BUILD:-build/$ARCH}"
TIMEOUT="${TIMEOUT:-}"
LOG="$BUILD/qemu-console.log"
KERNEL="${KERNEL:-}"
DTB="${DTB:-}"
BIOS="${BIOS:-}"
MARKER_ARCH="$ARCH"
MARKER_SHUTDOWN_SIGNAL=10
NEEDS_DTB=0
NEEDS_BIOS=0

case "$ARCH" in
    x86)
        QEMU=qemu-system-i386; MACHINE="-machine q35"; CONSOLE=ttyS0
        ;;
    x86_64) QEMU=qemu-system-x86_64; MACHINE="-machine q35"; CONSOLE=ttyS0 ;;
    aarch64) QEMU=qemu-system-aarch64; MACHINE="-machine virt -cpu cortex-a57"; CONSOLE=ttyAMA0 ;;
    armv6)
        QEMU=qemu-system-arm; MACHINE="-machine realview-eb -cpu arm1136"; CONSOLE=ttyAMA0
        MARKER_ARCH=arm
        NEEDS_DTB=1
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    armv7)
        QEMU=qemu-system-arm; MACHINE="-machine virt,highmem=off -cpu cortex-a15"; CONSOLE=ttyAMA0
        MARKER_ARCH=arm
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    riscv64)
        QEMU=qemu-system-riscv64; MACHINE="-machine virt"; CONSOLE=ttyS0
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    loongarch64)
        QEMU=qemu-system-loongarch64; MACHINE="-machine virt -cpu la464"; CONSOLE=ttyS0
        NEEDS_BIOS=1
        MEMORY="${MEMORY:-2G}"
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    mips)
        QEMU=qemu-system-mips; MACHINE="-machine malta"; CONSOLE=ttyS0
        MARKER_SHUTDOWN_SIGNAL=16
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    mipsel)
        QEMU=qemu-system-mipsel; MACHINE="-machine malta"; CONSOLE=ttyS0
        MARKER_SHUTDOWN_SIGNAL=16
        TIMEOUT="${TIMEOUT:-300}"
        ;;
    *) echo "SKIP: no qemu boot test for ARCH=$ARCH"; exit 0 ;;
esac

TIMEOUT="${TIMEOUT:-90}"
MEMORY="${MEMORY:-256}"

if ! command -v "$QEMU" >/dev/null 2>&1; then
    echo "SKIP: $QEMU not installed"
    exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: python3 not installed (needed to pack the initramfs)"
    exit 0
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
            echo "SKIP: x86 boot requires a 32-bit kernel; set KERNEL=/path/to/vmlinuz"
            exit 0
            ;;
        *)
            echo "SKIP: $ARCH boot requires an explicit kernel image"
            exit 0
            ;;
    esac
fi
if [ -z "$KERNEL" ] || [ ! -r "$KERNEL" ]; then
    echo "SKIP: no readable kernel image; set KERNEL=/path/to/vmlinuz"
    exit 0
fi
if [ "$NEEDS_DTB" -ne 0 ] && { [ -z "$DTB" ] || [ ! -r "$DTB" ]; }; then
    echo "SKIP: $ARCH boot requires a readable DTB; set DTB=/path/to/board.dtb"
    exit 0
fi
if [ "$NEEDS_BIOS" -ne 0 ] && { [ -z "$BIOS" ] || [ ! -r "$BIOS" ]; }; then
    echo "SKIP: $ARCH boot requires a readable UEFI firmware; set BIOS=/path/to/QEMU_EFI.fd"
    exit 0
fi

BUILD="$BUILD" OUT="$BUILD/initramfs.cpio" sh tools/mkinitramfs.sh

echo "booting $KERNEL under $QEMU"
set +e
set -- "$QEMU" $MACHINE -m "$MEMORY"
if [ -n "$BIOS" ]; then
    set -- "$@" -bios "$BIOS"
fi
if [ -n "$DTB" ]; then
    set -- "$@" -dtb "$DTB"
fi
set -- "$@" \
    -kernel "$KERNEL" \
    -initrd "$BUILD/initramfs.cpio" \
    -append "console=$CONSOLE panic=1 loglevel=4" \
    -nographic -no-reboot -serial mon:stdio -display none
"$@" > "$LOG" 2>&1 &
QEMU_PID=$!
QEMU_RC=0
QEMU_COMPLETE=0
QEMU_ELAPSED=0
while kill -0 "$QEMU_PID" 2>/dev/null; do
    if grep -qF "init: syncing" "$LOG" 2>/dev/null; then
        QEMU_COMPLETE=1
        kill "$QEMU_PID" 2>/dev/null
        break
    fi
    if [ "$QEMU_ELAPSED" -ge "$TIMEOUT" ]; then
        kill "$QEMU_PID" 2>/dev/null
        break
    fi
    sleep 1
    QEMU_ELAPSED=$((QEMU_ELAPSED + 1))
done
wait "$QEMU_PID"
QEMU_CHILD_RC=$?
if [ "$QEMU_COMPLETE" -ne 0 ]; then
    QEMU_RC=0
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

echo "checking markers:"
export MARKER_ARCH
export MARKER_SHUTDOWN_SIGNAL
. tools/boot-markers.sh

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
