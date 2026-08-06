#!/bin/sh
set -eu

build="${1:?usage: run-profile.sh <build-dir>}"
profile="${PROFILE:?PROFILE is required}"
release="${RELEASE:-development}"
arch="${ARCH:-x86_64}"
stage="${STAGE:-${TMPDIR:-/tmp}/init-profile-$profile}"
trace="$build/profile.strace"
runtime_trace="$build/profile-runtime.strace"
cpu="$build/profile.cpu"
result="$build/profile.namespace"

case "$profile" in
    standard|offline|volatile|offline-volatile|compressed|persistent|durable|lean)
        ;;
    *)
        echo "ERROR: unknown profile $profile"
        exit 1
        ;;
esac

Unavailable()
{
    if [ "${INIT_TEST_REQUIRED:-0}" -ne 0 ]; then
        echo "ERROR: $1" >&2
        exit 1
    fi
    echo "SKIP: $1"
    exit 0
}

if ! command -v strace >/dev/null 2>&1; then
    Unavailable "strace is required for profile behavior tests"
fi
if ! test -x /usr/bin/time; then
    Unavailable "/usr/bin/time is required for profile behavior tests"
fi

unset FEATURE_VARIANT
rm -f "$result" "$trace" "$runtime_trace" "$cpu"

PROFILE="$profile" RELEASE="$release" ARCH="$arch" \
BOOT_MARKERS=tools/profile-markers.sh \
INIT_NS_RESULT="$result" STAGE="$stage" \
/usr/bin/time -f '%U %S %e' -o "$cpu" \
strace -f -qq -s 256 \
       -e trace=execve,socket,connect,capset,pipe2,openat,\
inotify_init1,inotify_add_watch,inotify_rm_watch \
       -o "$trace" \
       sh tools/run-namespace.sh "$build" tools/stage-rootfs.sh

if [ "$(cat "$result" 2>/dev/null || true)" = skip ]; then
    Unavailable "profile behavior test needs a working user namespace"
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

reject_trace()
{
    if grep -qF "$1" "$runtime_trace"; then
        echo "BAD: profile $profile issued $1"
        exit 1
    fi
    echo "ok: profile $profile issued no $1"
}

case "$profile" in
    offline|offline-volatile|lean)
        reject_trace 'socket(AF_INET'
        ;;
    *)
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
            echo "BAD: profile $profile did not connect the local SNTP peer"
            exit 1
        fi
        echo "ok: profile $profile connected the local SNTP peer"
        ;;
esac

case "$profile" in
    volatile|offline-volatile|lean)
        if [ -e "$stage/var/log/init.log" ]; then
            echo "BAD: profile $profile created init.log"
            exit 1
        fi
        reject_trace '"/var/log/init.log"'
        ;;
    compressed|durable)
        log="$stage/var/log/init.log"
        extracted="$stage/var/log/init.log.extracted"
        log_magic=$(od -An -tx1 -N8 "$log" | tr -d ' \n')
        if [ "$log_magic" != 494e49544c4f4700 ]; then
            echo "BAD: profile $profile did not produce a framed active log"
            exit 1
        fi
        if ! python3 tools/init-log-read.py verify "$log"; then
            echo "BAD: profile $profile produced an invalid framed log"
            exit 1
        fi
        python3 tools/init-log-read.py extract -o "$extracted" "$log"
        if ! grep -qF 'legacy-before-compression' "$extracted" ||
           ! grep -qF 'init: log writer pid' "$extracted" ||
           ! grep -qF 'init: subsecond: started' "$extracted"; then
            echo "BAD: profile $profile extraction lost log records"
            exit 1
        fi
        echo "ok: profile $profile produced a readable framed log"
        ;;
    *)
        if [ ! -f "$stage/var/log/init.log" ]; then
            echo "BAD: profile $profile produced no init.log"
            exit 1
        fi
        ;;
esac

case "$profile" in
    persistent|durable)
        sh tools/check-schedule-state.sh "$stage/var/lib/init.schedule" \
           "PROFILE=$profile"
        ;;
    *)
        if [ -e "$stage/var/lib/init.schedule" ]; then
            echo "BAD: profile $profile wrote schedule state"
            exit 1
        fi
        echo "ok: profile $profile wrote no schedule state"
        ;;
esac

if [ "$profile" = lean ]; then
    reject_trace 'openat(AT_FDCWD, "/dev/watchdog"'
    reject_trace '.check"'
    reject_trace 'pipe2('
    reject_trace 'inotify_init1('
    reject_trace 'inotify_add_watch('
    reject_trace 'inotify_rm_watch('
    reject_trace 'socket(AF_NETLINK'
fi
