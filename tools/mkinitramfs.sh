#!/bin/sh
# Pack the staged root into an initramfs.
#
# The kernel runs /init from an initramfs by default, so the binary under test
# really does come up as PID 1 with no userland underneath it.
set -eu

BUILD="${BUILD:-build/x86_64}"
STAGE="$BUILD/initramfs"
OUT="${OUT:-$BUILD/initramfs.cpio}"

sh tools/stage-rootfs.sh "$BUILD" "$STAGE"
python3 tools/cpio_newc.py "$STAGE" "$OUT"
