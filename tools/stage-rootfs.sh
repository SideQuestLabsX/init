#!/bin/sh
set -eu

BUILD="${1:?usage: stage-rootfs.sh <build-dir> <stage-dir>}"
STAGE="${2:?usage: stage-rootfs.sh <build-dir> <stage-dir>}"

if [ ! -x "$BUILD/init" ]; then
    echo "stage-rootfs: $BUILD/init not built" >&2
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/dev" "$STAGE/proc" "$STAGE/run" "$STAGE/sys" "$STAGE/var/log" "$STAGE/var/lib" \
         "$STAGE/tasks/always" "$STAGE/tasks/boot" \
         "$STAGE/tasks/2s" "$STAGE/tasks/500ms" "$STAGE/tasks/1h" "$STAGE/tasks/3d" \
         "$STAGE/tasks/1d-03-30" "$STAGE/tasks/4d-03-30" "$STAGE/tasks/sun-04-00"

if [ "${INIT_STATUS_FALLBACK:-0}" -ne 0 ]; then
    rm -rf "$STAGE/run"
    ln -s missing-run "$STAGE/run"
fi

cp "$BUILD/init" "$STAGE/init"
chmod 0755 "$STAGE/init"

# Symlink mode verifies that the target stays untouched
if [ "${INIT_LOG_SYMLINK:-0}" -ne 0 ]; then
    printf 'SENTINEL-UNTOUCHED\n' > "$STAGE/var/log/sentinel"
    ln -s sentinel "$STAGE/var/log/init.log"
    : > "$STAGE/dev/log-symlink-test"
fi

install_bin()
{
    cp "$BUILD/fixtures/$1" "$STAGE/$2"
    chmod 0755 "$STAGE/$2"
}

if [ "${INIT_WATCHDOG_TEST:-0}" -ne 0 ]; then
    install_bin task_watchdog tasks/always/watchdog
    install_bin probe_watchdog tasks/always/watchdog.check
    exit 0
fi

install_bin task_ok           tasks/always/ok
install_bin probe_ok          tasks/always/ok.check
install_bin task_log_edge     tasks/always/log_edge
install_bin task_log_interleave_a tasks/always/log_interleave_a
install_bin task_log_interleave_b tasks/always/log_interleave_b
install_bin task_flap         tasks/always/flap
install_bin task_ok           tasks/always/hangcheck
install_bin probe_tree        tasks/always/hangcheck.check
install_bin task_selftest     tasks/always/selftest
install_bin task_exit_delayed tasks/always/midprobe
install_bin probe_hang        tasks/always/midprobe.check
install_bin task_ignore_term  tasks/always/ignoreterm
install_bin probe_fail        tasks/always/ignoreterm.check
install_bin task_ok           tasks/always/probefail
install_bin probe_fail        tasks/always/probefail.check
install_bin task_tree         tasks/always/tree
install_bin probe_fail        tasks/always/tree.check
install_bin task_oneshot      tasks/boot/oneshot
install_bin task_stable       tasks/boot/stable
install_bin task_tick         tasks/2s/tick

if [ "${INIT_TASK_DISCOVERY_TEST:-0}" -ne 0 ]; then
    : > "$STAGE/dev/discovery-test"
    install_bin task_discovery_controller tasks/boot/discovery_controller
    install_bin task_oneshot tasks/boot/discovery_content
    install_bin task_discovery_v1 tasks/boot/.discovery_content_v1
    install_bin task_discovery_v2 tasks/boot/.discovery_content_v2
    install_bin task_discovery_v1 tasks/always/discovery_replace
    install_bin task_discovery_v1 tasks/always/.discovery_add
    install_bin task_discovery_v1 tasks/always/.discovery_new
    install_bin task_discovery_v2 tasks/always/.discovery_replace_v2
fi

if [ "${INIT_STATUS_READER:-0}" -ne 0 ]; then
    cp "$BUILD/init-status" "$STAGE/tasks/500ms/status-reader"
    chmod 0755 "$STAGE/tasks/500ms/status-reader"
fi

if [ "${INIT_SNTP_FIXTURE:-1}" -ne 0 ]; then
    install_bin task_sntp_server tasks/always/sntp_server
fi

if [ "${INIT_LOGD_FIXTURE:-1}" -ne 0 ]; then
    install_bin task_signal_logd tasks/boot/signal_logd
    : > "$STAGE/var/log/logd-fixture-expected"
fi

if [ -n "${INIT_NS_ACTIVE_TIER:-}" ] &&
   [ "${INIT_NS_PRIVILEGE_FIXTURES:-1}" -ne 0 ]; then
    install_bin task_status tasks/boot/caps
    if [ "$INIT_NS_ACTIVE_TIER" = auto ]; then
        install_bin task_status tasks/boot/identity
    else
        install_bin task_ok tasks/always/impossible
    fi
fi

# Only 2s and 500ms may fire in the test window
# The remaining directories cover accepted schedule spellings
install_bin task_oneshot  tasks/500ms/subsecond
install_bin task_oneshot  tasks/1h/hourly
install_bin task_oneshot  tasks/3d/interval_days
install_bin task_oneshot  tasks/1d-03-30/nightly
install_bin task_oneshot  tasks/4d-03-30/every_fourth
install_bin task_oneshot  tasks/sun-04-00/weekly
