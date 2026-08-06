#!/bin/sh
# shellcheck shell=sh disable=SC2034

profile="${PROFILE:?PROFILE is required}"

case "$profile" in
    standard)
        ;;
    offline)
        MARKER_SNTP=0
        ;;
    volatile)
        MARKER_LOGD=0
        MARKER_LOGFILE=0
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        ;;
    offline-volatile)
        MARKER_SNTP=0
        MARKER_LOGD=0
        MARKER_LOGFILE=0
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        ;;
    compressed)
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        ;;
    persistent)
        MARKER_SNTP_CLOCK_SET=1
        ;;
    durable)
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        MARKER_SNTP_CLOCK_SET=1
        ;;
    lean)
        MARKER_SNTP=0
        MARKER_LOGD=0
        MARKER_LOGFILE=0
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        MARKER_PROBES=0
        ;;
    *)
        echo "BAD: unknown profile $profile"
        fail=1
        ;;
esac

# shellcheck disable=SC1091
. tools/boot-markers.sh

reject "FIXTURE profile completion incomplete"
expect_once_live "init ${MARKER_ARCH:-$ARCH} starting, pid 1, release ${RELEASE:-development}, profile $profile"

case "$profile" in
    volatile|offline-volatile|lean)
        expect "FIXTURE logfile missing"
        reject "log writer pid"
        reject "FIXTURE log writer signaled"
        ;;
esac

if [ "$profile" = lean ]; then
    reject "FIXTURE probe ran"
    reject "watchdog armed"
fi
