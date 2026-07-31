#!/bin/sh
set -eu

BUILD="${1:?usage: run-feature-variant.sh <build-dir>}"
FEATURE_VARIANT="${FEATURE_VARIANT:?FEATURE_VARIANT is required}"
key=${FEATURE_VARIANT%%=*}
stage="${TMPDIR:-/tmp}/init-variant-$key"
trace="$BUILD/variant.strace"
runtime_trace="$BUILD/variant-runtime.strace"
cpu="$BUILD/variant.cpu"
result="$BUILD/variant.namespace"

if ! command -v strace >/dev/null 2>&1
then
    echo "SKIP: strace is required for feature behavior tests"
    exit 0
fi
if ! test -x /usr/bin/time
then
    echo "SKIP: /usr/bin/time is required for feature behavior tests"
    exit 0
fi

rm -f "$result"

FEATURE_VARIANT="$FEATURE_VARIANT" \
BOOT_MARKERS=tools/feature-variant-markers.sh \
INIT_NS_RESULT="$result" \
STAGE="$stage" \
/usr/bin/time -f '%U %S %e' -o "$cpu" \
strace -f -qq -s 256 \
       -e trace=execve,socket,connect,capset,pipe2,openat \
       -o "$trace" \
sh tools/run-namespace.sh "$BUILD" tools/stage-feature-variant.sh

if [ "$(cat "$result" 2>/dev/null || true)" = skip ]; then
    echo "SKIP: feature behavior test needs a working user namespace"
    exit 0
fi

init_line=$(grep -nF 'execve("/init"' "$trace" | head -n 1 || true)
if [ -z "$init_line" ]; then
    echo "ERROR: trace did not observe /init"
    exit 1
fi
init_line=${init_line%%:*}
sed -n "${init_line},\$p" "$trace" > "$runtime_trace"

if [ "$FEATURE_VARIANT" != OFFLINE_MODE=1 ]; then
    if ! grep -Eq 'connect\(.*sin_port=htons\(40123\).*inet_addr\("127\.0\.0\.1"\).*= 0' \
         "$runtime_trace"; then
        echo "BAD: $FEATURE_VARIANT did not connect the local SNTP peer"
        exit 1
    fi
    echo "ok: $FEATURE_VARIANT connected the local SNTP peer"
fi

reject_trace()
{
    if grep -qF "$1" "$runtime_trace"; then
        echo "BAD: $FEATURE_VARIANT issued $1"
        exit 1
    fi
    echo "ok: $FEATURE_VARIANT issued no $1"
}

case "$FEATURE_VARIANT" in
    OFFLINE_MODE=1)
        reject_trace 'socket('
        ;;
    FEATURE_WATCHDOG=0)
        reject_trace 'openat(AT_FDCWD, "/dev/watchdog"'
        ;;
    FEATURE_EXEC_PROBES=0)
        reject_trace '.check"'
        if grep -aFq 'probe failed (' "$BUILD/init"; then
            echo "BAD: probe failure code remains in $BUILD/init"
            exit 1
        fi
        if ! awk '(($1 + $2) * 2) < $3 { exit 0 } { exit 1 }' "$cpu"; then
            echo "BAD: probe-less boot CPU ratio too high: $(cat "$cpu")"
            exit 1
        fi
        watchdog_bytes=$(wc -c < "$stage/dev/watchdog")
        if [ "$watchdog_bytes" -lt 3 ]; then
            echo "BAD: probe-less critical task withheld watchdog pets"
            exit 1
        fi
        echo "ok: probe-less boot CPU $(cat "$cpu") (user system elapsed seconds), watchdog pets $watchdog_bytes"
        ;;
    FEATURE_LOG_DISK=0)
        if [ -e "$stage/var/log/init.log" ]; then
            echo "BAD: FEATURE_LOG_DISK=0 created init.log"
            exit 1
        fi
        if ! awk '(($1 + $2) * 4) < $3 { exit 0 } { exit 1 }' "$cpu"; then
            echo "BAD: disk-less boot CPU ratio too high: $(cat "$cpu")"
            exit 1
        fi
        echo "ok: FEATURE_LOG_DISK=0 created no init.log"
        echo "ok: disk-less boot CPU $(cat "$cpu") (user system elapsed seconds)"
        ;;
    FEATURE_LOG_CAPTURE=0)
        reject_trace 'pipe2('
        log="$stage/var/log/init.log"
        if [ ! -f "$log" ]; then
            echo "BAD: FEATURE_LOG_CAPTURE=0 produced no init.log"
            exit 1
        fi
        if grep -qF 'ok-stderr-line' "$log"; then
            echo "BAD: FEATURE_LOG_CAPTURE=0 captured task stderr"
            exit 1
        fi
        echo "ok: FEATURE_LOG_CAPTURE=0 captured no task stderr"
        ;;
    FEATURE_CAPABILITY_DROP=0)
        reject_trace 'capset('
        ;;
esac
