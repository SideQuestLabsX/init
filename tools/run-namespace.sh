#!/bin/sh
# Boot the binary as PID 1 inside a user + PID + mount namespace.
#
# Same fixtures and assertions as the qemu test, needing neither root, a kernel
# image, nor an emulator, so it runs on any Linux box and gives the supervision
# paths real coverage locally. What the namespace refuses (devtmpfs, the
# hardware watchdog, a real SYS_reboot) stays the qemu test's job.
set -eu

ARCH="${ARCH:-x86_64}"
BUILD="${1:-${BUILD:-build/$ARCH}}"
ROOTFS_STAGE="${2:-${ROOTFS_STAGE:-tools/stage-rootfs.sh}}"
TIMEOUT="${TIMEOUT:-60}"
# Staged outside the build tree on purpose. The repo may sit on a filesystem
# without real POSIX semantics (a Windows drive under WSL, a network share), and
# the transcript needs O_APPEND to actually be atomic.
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

# must reach the uid the privilege fixtures drop to
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

sh "$ROOTFS_STAGE" "$BUILD" "$STAGE"

# No devtmpfs here, so stand in a regular file for /dev/console. init and the
# fixtures both open it O_APPEND, so the merged transcript stays ordered.
: > "$STAGE/dev/console"
chmod 0666 "$STAGE/dev/console"

echo "namespace tier: $ns_tier"
if [ "$ns_tier" = caps ]; then
    echo "namespace tier caps: uid and gid assertions unavailable"
fi
echo "booting $BUILD/init as namespace PID 1"
set +e
if [ "$ns_tier" = auto ]; then
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
. "${BOOT_MARKERS:-tools/boot-markers.sh}"

if [ $RC -eq 124 ]; then
    echo "  BAD   timed out after ${TIMEOUT}s without shutting down"
    fail=1
fi

if [ $fail -ne 0 ]; then
    record_result fail
    echo "NAMESPACE BOOT TEST FAILED"
    exit 1
fi
record_result pass
echo "NAMESPACE BOOT TEST PASSED"
