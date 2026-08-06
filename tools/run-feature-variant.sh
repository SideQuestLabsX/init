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
       -e trace=execve,socket,connect,capset,pipe2,openat,\
inotify_init1,inotify_add_watch,inotify_rm_watch \
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
init_line_number=${init_line%%:*}
init_pid=$(sed -n "${init_line_number}p" "$trace" | awk '{ print $1 }')
if [ -z "$init_pid" ]; then
    echo "ERROR: trace did not identify the /init process"
    exit 1
fi
awk -v pid="$init_pid" '$1 == pid { print }' "$trace" > "$runtime_trace"

if [ "$FEATURE_VARIANT" != OFFLINE_MODE=1 ]; then
    if ! awk '
        /connect\(.*sin_port=htons\(40123\).*inet_addr\("127\.0\.0\.1"\)/ {
            if(index($0, "= 0"))
                bFound = 1
            else
                bPending = 1
        }
        bPending && /connect resumed/ && index($0, "= 0") { bFound = 1 }
        END { exit bFound ? 0 : 1 }
    ' "$runtime_trace"; then
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
        reject_trace 'socket(AF_INET'
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
    FEATURE_LOG_COMPRESSION=1)
        log="$stage/var/log/init.log"
        extracted="$stage/var/log/init.log.extracted"
        if ! python3 tools/init-log-read.py verify "$log"; then
            echo "BAD: FEATURE_LOG_COMPRESSION=1 produced an invalid framed log"
            exit 1
        fi
        python3 tools/init-log-read.py extract -o "$extracted" "$log"
        if ! grep -qF 'legacy-before-compression' "$extracted" ||
           ! grep -qF 'init: log writer pid' "$extracted" ||
           ! grep -qF 'init: subsecond: started' "$extracted"; then
            echo "BAD: FEATURE_LOG_COMPRESSION=1 extraction lost log records"
            exit 1
        fi
        echo "ok: FEATURE_LOG_COMPRESSION=1 produced a readable framed log"
        ;;
    FEATURE_CAPABILITY_DROP=0)
        if grep -qF 'capset(' "$trace"; then
            echo "BAD: FEATURE_CAPABILITY_DROP=0 issued capset in a child"
            exit 1
        fi
        if ! grep -qF 'FIXTURE caps status Uid 0 0 0 0' "$BUILD/ns-console.log"; then
            echo "BAD: FEATURE_CAPABILITY_DROP=0 did not run the caps fixture"
            exit 1
        fi
        if ! grep -qF 'FIXTURE caps status NoNewPrivs 0' "$BUILD/ns-console.log"; then
            echo "BAD: FEATURE_CAPABILITY_DROP=0 changed child privilege state"
            exit 1
        fi
        echo "ok: FEATURE_CAPABILITY_DROP=0 retained the unmodified child privilege state"
        ;;
    FEATURE_TASK_DISCOVERY=0)
        reject_trace 'inotify_init1('
        reject_trace 'inotify_add_watch('
        reject_trace 'inotify_rm_watch('
        ;;
    FEATURE_STATIC_TASKS=1)
        reject_trace 'inotify_init1('
        reject_trace 'inotify_add_watch('
        reject_trace 'inotify_rm_watch('
        ;;
    FEATURE_NETLINK_EVENTS=0)
        reject_trace 'socket(AF_NETLINK'
        ;;
    FEATURE_PERSIST_SCHEDULE=1)
        state="$stage/var/lib/init.schedule"
        if [ ! -f "$state" ]; then
            echo "BAD: FEATURE_PERSIST_SCHEDULE=1 created no schedule state"
            exit 1
        fi
        state_header_bytes=16
        state_record_bytes=268
        state_path_bytes=256
        state_bytes=$(wc -c < "$state")
        state_magic=$(od -An -tx1 -N4 "$state" | tr -d ' \n')
        state_version=$(od -An -tu4 -j4 -N4 "$state" | tr -d ' ')
        state_count=$(od -An -tu4 -j8 -N4 "$state" | tr -d ' ')
        state_reserved=$(od -An -tu4 -j12 -N4 "$state" | tr -d ' ')
        if [ "$state_magic" != 53544131 ] || [ "$state_version" -ne 1 ] ||
           [ "$state_count" -le 0 ] || [ "$state_count" -gt 1024 ] ||
           [ "$state_reserved" -ne 0 ]; then
            echo "BAD: schedule state header is malformed"
            exit 1
        fi
        expected_bytes=$((state_header_bytes + state_count * state_record_bytes))
        if [ "$state_bytes" -ne "$expected_bytes" ]; then
            echo "BAD: schedule state size does not match its record count"
            exit 1
        fi

        tick_found=0
        record=0
        while [ "$record" -lt "$state_count" ]; do
            offset=$((state_header_bytes + record * state_record_bytes))
            path_len=$(od -An -tu2 -j "$offset" -N2 "$state" | tr -d ' ')
            record_reserved=$(od -An -tu2 -j "$((offset + 2))" -N2 "$state" | tr -d ' ')
            last_run=$(od -An -tu8 -j "$((offset + 4))" -N8 "$state" | tr -d ' ')
            path=$(dd if="$state" bs=1 skip="$((offset + 12))" \
                   count="$state_path_bytes" 2>/dev/null | tr -d '\0')

            if [ "$path_len" -le 0 ] || [ "$path_len" -ge "$state_path_bytes" ] ||
               [ "$record_reserved" -ne 0 ] || [ "$last_run" -eq 0 ] ||
               [ "${#path}" -ne "$path_len" ]; then
                echo "BAD: schedule state record $record is malformed"
                exit 1
            fi
            case "$path" in
                /tasks/*)
                    ;;
                *)
                    echo "BAD: schedule state record $record has an invalid path"
                    exit 1
                    ;;
            esac
            if [ "$path" = "/tasks/2s/tick" ]; then
                tick_found=1
            fi
            record=$((record + 1))
        done

        if [ "$tick_found" -eq 0 ]; then
            echo "BAD: schedule state has no successful interval record"
            exit 1
        fi
        echo "ok: FEATURE_PERSIST_SCHEDULE=1 wrote valid interval records"
        ;;
esac
