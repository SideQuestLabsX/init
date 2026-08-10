#!/bin/sh
set -eu

SUCCESS_BUILD="${1:?usage: run-mount-table-tests.sh <success-build> <failure-build>}"
FAILURE_BUILD="${2:?usage: run-mount-table-tests.sh <success-build> <failure-build>}"
TIMEOUT="${TIMEOUT:-15}"

Unavailable()
{
    if [ "${INIT_TEST_REQUIRED:-0}" -ne 0 ]; then
        echo "ERROR: $1" >&2
        exit 1
    fi
    echo "SKIP: $1"
    exit 0
}

if ! command -v unshare >/dev/null 2>&1 ||
   ! command -v timeout >/dev/null 2>&1 ||
   ! command -v strace >/dev/null 2>&1
then
    Unavailable "unshare, timeout and strace are required for mount table tests"
fi

if ! unshare --user --map-root-user true 2>/dev/null; then
    Unavailable "unprivileged user namespaces are unavailable"
fi

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/init-mount-tests.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

Stage()
{
    build=$1
    rm -rf "$STAGE"
    mkdir -p "$STAGE/dev" "$STAGE/proc" "$STAGE/run" "$STAGE/sys" \
             "$STAGE/tasks/boot" "$STAGE/var/log" "$STAGE/var/lib"
    cp "$build/init" "$STAGE/init"
    chmod 0755 "$STAGE/init"
    : > "$STAGE/dev/console"
    : > "$STAGE/dev/null"
    chmod 0666 "$STAGE/dev/console" "$STAGE/dev/null"
}

Run()
{
    log=$1
    trace=$2
    set +e
    timeout "$TIMEOUT" unshare --user --map-root-user --mount \
        strace -f -qq -s 1024 -e trace=mount -o "$trace" \
        unshare --pid --fork chroot "$STAGE" /init > "$log" 2>&1
    rc=$?
    set -e
    cat "$STAGE/dev/console" >> "$log" 2>/dev/null || true
    case "$rc" in
        0|124|129)
            ;;
        *)
            cat "$log"
            echo "FAIL: mount table boot exited with status $rc"
            exit 1
            ;;
    esac
}

success_log="$SUCCESS_BUILD/mount-table-success.log"
success_trace="$SUCCESS_BUILD/mount-table-success.trace"
Stage "$SUCCESS_BUILD"
cp "$SUCCESS_BUILD/fixtures/task_mount_shutdown" "$STAGE/tasks/boot/shutdown"
chmod 0755 "$STAGE/tasks/boot/shutdown"
Run "$success_log" "$success_trace"

if ! grep -qF 'MOUNT TABLE TASK STARTED' "$success_log"; then
    cat "$success_log"
    echo "FAIL: task did not start after an optional mount failure"
    exit 1
fi
if grep -qF 'init: PANIC:' "$success_log"; then
    cat "$success_log"
    echo "FAIL: optional mount failure entered the panic path"
    exit 1
fi
if [ ! -d "$STAGE/mount-test/parent/leaf" ] ||
   [ ! -d "$STAGE/mount-test/optional" ]; then
    echo "FAIL: mount target parents were not created"
    exit 1
fi
if ! grep -F 'mount("mount-test", "/mount-test/parent/leaf", "tmpfs", MS_NOSUID, "mode=0700") = 0' \
    "$success_trace" >/dev/null; then
    cat "$success_trace"
    echo "FAIL: required custom mount or data option was not applied"
    exit 1
fi
if ! grep -F 'mount("mount-test", "/mount-test/optional", "init-no-such-fs", 0, NULL) = -1' \
    "$success_trace" >/dev/null; then
    cat "$success_trace"
    echo "FAIL: optional custom mount failure was not attempted"
    exit 1
fi
if ! awk '
    /\/mount-test\/parent\/leaf/ { required = NR }
    /\/mount-test\/optional/ { optional = NR }
    END { exit !(required > 0 && optional > required) }
' "$success_trace"; then
    cat "$success_trace"
    echo "FAIL: custom mounts did not follow table order"
    exit 1
fi
echo "ok: custom mounts preserve order, data and optional failure policy"

failure_log="$FAILURE_BUILD/mount-table-required.log"
failure_trace="$FAILURE_BUILD/mount-table-required.trace"
Stage "$FAILURE_BUILD"
cp "$FAILURE_BUILD/fixtures/task_mount_shutdown" "$STAGE/tasks/boot/shutdown"
chmod 0755 "$STAGE/tasks/boot/shutdown"
Run "$failure_log" "$failure_trace"

if ! grep -qF 'init: PANIC: required mount (none) on /mount-test/required/leaf failed' \
    "$failure_log"; then
    cat "$failure_log"
    echo "FAIL: required mount failure did not enter the panic path"
    exit 1
fi
if grep -qF 'MOUNT TABLE TASK STARTED' "$failure_log"; then
    cat "$failure_log"
    echo "FAIL: a task started after a required mount failure"
    exit 1
fi
if ! grep -F 'mount("/missing-mount-source", "/mount-test/required/leaf", NULL, MS_BIND, NULL) = -1' \
    "$failure_trace" >/dev/null; then
    cat "$failure_trace"
    echo "FAIL: required bind mount failure was not attempted"
    exit 1
fi
if [ ! -d "$STAGE/mount-test/required/leaf" ]; then
    echo "FAIL: required mount target parents were not created"
    exit 1
fi
echo "ok: required mount failure panics before task startup"
