#!/bin/sh
set -eu

BUILD="${1:?usage: stage-feature-variant.sh <build-dir> <stage-dir>}"
STAGE="${2:?usage: stage-feature-variant.sh <build-dir> <stage-dir>}"

sh tools/stage-rootfs.sh "$BUILD" "$STAGE"

if [ "${FEATURE_VARIANT:-}" = FEATURE_EXEC_PROBES=0 ]; then
    : > "$STAGE/dev/watchdog"
    chmod 0666 "$STAGE/dev/watchdog"
fi
