# shellcheck shell=sh

case "$FEATURE_VARIANT" in
    OFFLINE_MODE=1)
        MARKER_SNTP=0
        ;;
    FEATURE_EXEC_PROBES=0)
        MARKER_PROBES=0
        ;;
    FEATURE_LOG_DISK=0)
        MARKER_LOGFILE=0
        MARKER_CAPTURE=0
        MARKER_LOGD=0
        MARKER_CHILD_ERROR=0
        ;;
    FEATURE_LOG_CAPTURE=0)
        MARKER_CAPTURE=0
        MARKER_CHILD_ERROR=0
        ;;
    FEATURE_CAPABILITY_DROP=0)
        MARKER_NS_TIER=
        ;;
    FEATURE_TASK_DISCOVERY=0)
        MARKER_DISCOVERY=0
        ;;
    FEATURE_PERSIST_SCHEDULE=1)
        MARKER_SNTP_CLOCK_SET=1
        ;;
esac

. tools/boot-markers.sh

case "$FEATURE_VARIANT" in
    FEATURE_WATCHDOG=0)
        reject "/dev/watchdog"
        reject "watchdog armed"
        ;;
    FEATURE_EXEC_PROBES=0)
        expect "watchdog armed"
        reject "FIXTURE probe ran"
        reject "probe failed"
        ;;
    FEATURE_LOG_DISK=0)
        expect "FIXTURE logfile missing"
        reject "log writer pid"
        ;;
    FEATURE_TASK_DISCOVERY=0)
        reject "FIXTURE discovery"
        ;;
esac
