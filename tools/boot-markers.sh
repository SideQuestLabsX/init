# Sourced by run-namespace.sh and run-qemu.sh, which define expect(), reject()
# and $ARCH. EXPECT_TASKS is the staged task count.

expect "init $ARCH starting, pid 1"
expect "${EXPECT_TASKS:-12} tasks in /tasks"
expect "ok: started pid"
expect "FIXTURE ok started"
expect "FIXTURE oneshot ran"
expect "oneshot: done"
reject "not a schedule"
expect "flap: exit 3 sig 0, respawn in"
expect "flap: FAILED after"
expect "FIXTURE tick fired"
expect "FIXTURE selftest done"
expect "shutdown requested by signal 10"
expect "syncing"
reject "PANIC"
reject "Kernel panic"

if [ "${MARKER_PROBES:-1}" -ne 0 ]; then
    expect "FIXTURE probe ran"
    expect "hangcheck: probe timed out, killing pid"
    expect "hangcheck: restarting on probe failure"
    expect "hangcheck: FAILED after"
fi

if [ "${MARKER_LOGFILE:-1}" -ne 0 ]; then
    expect "FIXTURE logfile begin"
fi

if [ "${MARKER_CAPTURE:-1}" -ne 0 ]; then
    expect "ok-stderr-line"
    expect "flap-dying"
fi
