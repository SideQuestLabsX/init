case "$FEATURE_VARIANT" in
    FEATURE_EXEC_PROBES=0)
        MARKER_PROBES=0
        ;;
    FEATURE_LOG_DISK=0)
        MARKER_LOGFILE=0
        MARKER_CAPTURE=0
        ;;
    FEATURE_LOG_CAPTURE=0)
        MARKER_CAPTURE=0
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
esac
