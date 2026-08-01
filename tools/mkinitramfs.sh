#!/bin/sh
set -eu

BUILD="${BUILD:-build/x86_64}"
STAGE="$BUILD/initramfs"
OUT="${OUT:-$BUILD/initramfs.cpio}"

sh tools/stage-rootfs.sh "$BUILD" "$STAGE"
python3 tools/cpio_newc.py "$STAGE" "$OUT"
