#!/bin/sh
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${1:-${BUILD:-build/$ARCH}}"
ROOTFS_STAGE="${2:-${ROOTFS_STAGE:-tools/stage-rootfs.sh}}"
TIMEOUT="${TIMEOUT:-60}"
# WSL-mounted Windows drives and network shares may not provide atomic O_APPEND
STAGE="${STAGE:-${TMPDIR:-/tmp}/init-nstest}"
LOG="$BUILD/ns-console.log"

requested_tier="${INIT_NS_TIER:-}"

record_result()
{
    if [ -n "${INIT_NS_RESULT:-}" ]; then
        printf '%s\n' "$1" > "$INIT_NS_RESULT"
    fi
}

case "$requested_tier" in
    ""|auto|caps)
        ;;
    skip)
        record_result skip
        echo "SKIP: namespace tier forced to skip"
        exit 0
        ;;
    *)
        echo "ERROR: INIT_NS_TIER must be auto, caps or skip"
        exit 1
        ;;
esac

unavailable()
{
    if [ -n "$requested_tier" ]; then
        echo "ERROR: namespace tier '$requested_tier' is unavailable: $1"
        exit 1
    fi
    record_result skip
    echo "SKIP: $1"
    exit 0
}

# Privilege fixtures require UID 65534
has_subid()
{
    awk -F: -v user="$2" '$1 == user && $3 >= 65536 { found = 1 } END { exit !found }' \
        "$1" 2>/dev/null
}

can_caps()
{
    unshare --user --map-root-user true 2>/dev/null
}

# --map-auto alone leaves the caller unmapped and the staged tree unwritable
can_auto()
{
    command -v newuidmap >/dev/null 2>&1 || return 1
    command -v newgidmap >/dev/null 2>&1 || return 1
    user=$(id -un)
    has_subid /etc/subuid "$user" || return 1
    has_subid /etc/subgid "$user" || return 1
    unshare --map-auto --map-root-user true 2>/dev/null
}

if ! command -v unshare >/dev/null 2>&1; then
    unavailable "unshare(1) is not installed"
fi

case "$requested_tier" in
    "")
        if can_auto; then
            ns_tier=auto
        elif can_caps; then
            ns_tier=caps
        else
            unavailable "unprivileged user namespaces are unavailable"
        fi
        ;;
    auto)
        can_auto || unavailable "automatic subordinate-id mapping failed"
        ns_tier=auto
        ;;
    caps)
        can_caps || unavailable "root-user mapping failed"
        ns_tier=caps
        ;;
esac

logd_fixture=1
privilege_fixtures=1
sntp_fixture=1
log_symlink=${INIT_LOG_SYMLINK:-0}
status_reader=${INIT_STATUS_READER:-0}
status_fallback=${INIT_STATUS_FALLBACK:-0}
remount_test=${INIT_NS_REMOUNT_TEST:-0}
case "${FEATURE_VARIANT:-}" in
    OFFLINE_MODE=1)
        sntp_fixture=0
        ;;
    FEATURE_LOG_DISK=0)
        logd_fixture=0
        ;;
    FEATURE_CAPABILITY_DROP=0)
        privilege_fixtures=0
        ;;
esac
if [ "$log_symlink" -ne 0 ]; then
    logd_fixture=0
fi

INIT_NS_ACTIVE_TIER="$ns_tier" \
INIT_LOGD_FIXTURE=$logd_fixture \
INIT_SNTP_FIXTURE=$sntp_fixture \
INIT_NS_PRIVILEGE_FIXTURES=$privilege_fixtures \
INIT_LOG_SYMLINK=$log_symlink \
INIT_STATUS_READER=$status_reader \
INIT_STATUS_FALLBACK=$status_fallback \
sh "$ROOTFS_STAGE" "$BUILD" "$STAGE"

task_count=$((20 + sntp_fixture + logd_fixture + status_reader + 2 * privilege_fixtures))
EXPECT_TASKS="${EXPECT_TASKS:-$task_count}"
if [ "$privilege_fixtures" -ne 0 ]; then
    MARKER_NS_TIER=$ns_tier
fi
if [ "$status_reader" -ne 0 ]; then
    MARKER_STATUS=1
fi
if [ "$status_fallback" -ne 0 ]; then
    MARKER_STATUS_FALLBACK=1
fi

# User namespaces cannot mount devtmpfs
# O_APPEND keeps the regular-file console transcript ordered
: > "$STAGE/dev/console"
chmod 0666 "$STAGE/dev/console"
: > "$STAGE/dev/null"
chmod 0666 "$STAGE/dev/null"

echo "namespace tier: $ns_tier"
if [ "$ns_tier" = caps ]; then
    echo "namespace tier caps: uid and gid assertions unavailable"
fi
echo "booting $BUILD/init as namespace PID 1"
set +e
if [ "$remount_test" -ne 0 ]; then
    if ! command -v strace >/dev/null 2>&1; then
        echo "SKIP: strace is required for the remount test"
        exit 0
    fi
    MOUNT_TRACE="$BUILD/ns-mount.trace"
    rm -f "$MOUNT_TRACE"
    # newuidmap rejects a target already controlled by ptrace
    if [ "$ns_tier" = auto ]; then
        timeout "$TIMEOUT" unshare --map-auto --map-root-user --mount \
            strace -f -qq -s 8192 -e trace=mount -o "$MOUNT_TRACE" \
            unshare --pid --fork \
            sh tools/run-namespace-remount.sh "$STAGE" > "$LOG" 2>&1
    else
        timeout "$TIMEOUT" unshare --user --map-root-user --mount \
            strace -f -qq -s 8192 -e trace=mount -o "$MOUNT_TRACE" \
            unshare --pid --fork \
            sh tools/run-namespace-remount.sh "$STAGE" > "$LOG" 2>&1
    fi
elif [ "$ns_tier" = auto ]; then
    timeout "$TIMEOUT" unshare --map-auto --map-root-user --mount --pid --fork \
        chroot "$STAGE" /init > "$LOG" 2>&1
else
    timeout "$TIMEOUT" unshare --user --map-root-user --mount --pid --fork \
        chroot "$STAGE" /init > "$LOG" 2>&1
fi
RC=$?
set -e

cat "$STAGE/dev/console" >> "$LOG" 2>/dev/null || true

echo "--- console transcript ---"
cat "$LOG"
echo "--- end transcript ---"

fail=0
expect()
{
    if grep -qF "$1" "$LOG"; then
        echo "  ok    $1"
    else
        echo "  MISS  $1"
        fail=1
    fi
}
reject()
{
    if grep -qF "$1" "$LOG"; then
        echo "  BAD   unexpected: $1"
        fail=1
    fi
}

echo "checking markers:"
MARKER_NAMESPACE=1
if [ "$log_symlink" -ne 0 ]; then
    MARKER_LOGD=0
    MARKER_LOGFILE=0
    MARKER_CAPTURE=0
    MARKER_CHILD_ERROR=0
fi

if [ "$status_fallback" -ne 0 ] && [ -e "$STAGE/run/init.status" ]; then
    echo "  BAD   status fallback left a file"
    fail=1
fi
. "${BOOT_MARKERS:-tools/boot-markers.sh}"

if [ "$remount_test" -ne 0 ]; then
    if awk '
        index($0, "MS_REMOUNT") && index($0, "/remount-test/") {
            path = $0
            sub(/^.*mount\(NULL, "/, "", path)
            sub(/".*$/, "", path)
            depth = gsub(/\//, "/", path)
            testCount++
            if(bSeen && depth > previousDepth)
                bBadOrder = 1
            previousDepth = depth
            bSeen = 1
            if(index(path, "/early/a/b"))
                earlyLine = earlyLine ? -1 : NR
            if(index(path, "/shallow"))
                shallowLine = shallowLine ? -1 : NR
            if(index(path, "zz_deepest"))
                deepestCount++
            if(index(path, "/space path"))
                decodedCount++
            leaf = path
            sub(/^.*\//, "", leaf)
            if(leaf ~ /^d[0-9][0-9]_/) {
                id = substr(leaf, 2, 2) + 0
                chain[id]++
            }
        }
        END {
            for(i = 0; i < 48; i++) {
                if(chain[i] != 1)
                    bBadChain = 1
            }
            exit !(testCount == 52 && deepestCount == 1 && decodedCount == 1 &&
                   earlyLine > 0 && shallowLine > 0 && earlyLine < shallowLine &&
                   !bBadOrder && !bBadChain)
        }
    ' "$MOUNT_TRACE"; then
        echo "  ok    complete deepest-first remount attempts"
    else
        echo "  BAD   incomplete or unordered remount attempts"
        fail=1
    fi
fi

if [ "$log_symlink" -ne 0 ]; then
    if [ -L "$STAGE/var/log/init.log" ] &&
       [ "$(cat "$STAGE/var/log/sentinel")" = SENTINEL-UNTOUCHED ]; then
        echo "  ok    log symlink target unchanged"
    else
        echo "  BAD   log symlink target changed"
        fail=1
    fi
fi

# PID 1 teardown can deliver SIGHUP to the namespace shell
case "$RC" in
    0|129)
        ;;
    124)
        echo "  BAD   timed out after ${TIMEOUT}s without shutting down"
        fail=1
        ;;
    *)
        echo "  BAD   namespace exited with status $RC"
        fail=1
        ;;
esac

if [ $fail -ne 0 ]; then
    record_result fail
    echo "NAMESPACE BOOT TEST FAILED"
    exit 1
fi
record_result pass
echo "NAMESPACE BOOT TEST PASSED"
