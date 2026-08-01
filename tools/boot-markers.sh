MARKER_LOGD="${MARKER_LOGD:-1}"
MARKER_NS_TIER="${MARKER_NS_TIER:-}"
MARKER_CHILD_ERROR="${MARKER_CHILD_ERROR:-1}"
MARKER_PROBES="${MARKER_PROBES:-1}"
MARKER_LOGFILE="${MARKER_LOGFILE:-1}"
MARKER_CAPTURE="${MARKER_CAPTURE:-1}"
MARKER_SNTP="${MARKER_SNTP:-1}"
MARKER_STATUS="${MARKER_STATUS:-0}"
MARKER_STATUS_FALLBACK="${MARKER_STATUS_FALLBACK:-0}"

expect "init $ARCH starting, pid 1"
expect "${EXPECT_TASKS:-21} tasks in /tasks"
expect "ok: started pid"
expect "FIXTURE ok started"
expect "FIXTURE log edge line sent"
expect "FIXTURE oneshot ran"
expect "oneshot: done"
expect "stable: done"
reject "stable: FAILED after"
reject "not a schedule"
expect "flap: exit 3 sig 0, respawn in"
expect "flap: FAILED after"
expect "FIXTURE tick fired"
expect "FIXTURE selftest done"
expect "shutdown requested by signal 10"
expect "syncing"
reject "PANIC"
reject "Kernel panic"

if [ "$MARKER_SNTP" -ne 0 ] && [ "${MARKER_NAMESPACE:-0}" -ne 0 ]; then
    expect "FIXTURE sntp reply sent"
    expect "sntp: clock_settime rejected"
fi

if [ "$MARKER_STATUS" -ne 0 ]; then
    if awk -v expected_tasks="$EXPECT_TASKS" '
        index($0, "STATUS snapshot seq=") {
            line = $0
            sub(/^.*STATUS snapshot seq=/, "", line)
            split(line, fields, " ")
            sequence = fields[1]
            bOpen = sequence ~ /^[1-9][0-9]*$/ &&
                    sequence ~ /[02468]$/ &&
                    fields[2] == "tasks=" expected_tasks
            bFlap = 0
            next
        }
        bOpen && index($0, "STATUS task=flap state=failed pid=0 runs=3 failures=3 exit=3 signal=0") {
            bFlap = 1
            next
        }
        bOpen && index($0, "STATUS snapshot end seq=") {
            line = $0
            sub(/^.*STATUS snapshot end seq=/, "", line)
            if(bFlap && line ~ /^[1-9][0-9]*$/ &&
               "x" line == "x" sequence) {
                bFound = 1
                exit
            }
            bOpen = 0
        }
        END { exit bFound ? 0 : 1 }
    ' "$LOG"; then
        echo "  ok    consistent status snapshot"
    else
        echo "  BAD   no consistent status snapshot"
        fail=1
    fi
    reject "STATUS error"
fi

if [ "$MARKER_STATUS_FALLBACK" -ne 0 ]; then
    expect "mount tmpfs on /run failed"
    expect "status: using anonymous fallback"
fi

if [ "$MARKER_LOGD" -ne 0 ]; then
    expect "FIXTURE log writer signaled"
    expect "log writer exited (exit -1 sig 15), respawning in"
    expect "FIXTURE log writer stopped"
    expect "log writer stalled, killing pid"
    expect "stalled log writer respawning in "
    expect "FIXTURE stalled log writer replaced"
fi

if [ -n "$MARKER_NS_TIER" ]; then
    status_task=caps
    if [ "$MARKER_NS_TIER" = auto ]; then
        status_task=identity
    fi

    expect "FIXTURE $status_task status CapInh 0000000000000400"
    expect "FIXTURE $status_task status CapPrm 0000000000000400"
    expect "FIXTURE $status_task status CapEff 0000000000000400"
    expect "FIXTURE $status_task status CapBnd 0000000000000400"
    expect "FIXTURE $status_task status CapAmb 0000000000000400"
    expect "FIXTURE $status_task status NoNewPrivs 1"

    if [ "$MARKER_NS_TIER" = auto ]; then
        expect "FIXTURE identity status Uid 65534 65534 65534 65534"
        expect "FIXTURE identity status Gid 65534 65534 65534 65534"
        expect "FIXTURE identity status Groups none"
    else
        if [ "$MARKER_CHILD_ERROR" -ne 0 ]; then
            expect "init: child setgroups failed, errno 1"
        fi
        expect "impossible: exit 126 sig 0"
        expect "impossible: FAILED after"
    fi
fi

if [ "$MARKER_PROBES" -ne 0 ]; then
    expect "FIXTURE probe ran"
    expect "hangcheck: probe timed out, killing pid"
    expect "hangcheck: restarting on probe failure"
    expect "hangcheck: FAILED after"
    expect "midprobe: cancelling probe pid"
    reject "midprobe: probe timed out"
    reject "midprobe: probe failed"
    expect "ignoreterm: restart grace expired, sending SIGKILL"
    expect "probefail: FAILED after"
    expect "FIXTURE tree replacement started"

    if awk '
        index($0, "FIXTURE tree replacement started") { bAfter = 1; next }
        bAfter && index($0, "FIXTURE tree worker alive") { bFound = 1; exit }
        END { exit bFound ? 0 : 1 }
    ' "$LOG"; then
        echo "  BAD   tree worker survived its replacement"
        fail=1
    else
        echo "  ok    tree worker stopped with its process group"
    fi
fi

if [ "$MARKER_LOGFILE" -ne 0 ]; then
    expect "FIXTURE logfile begin"
fi

if [ "$MARKER_CAPTURE" -ne 0 ]; then
    expect "ok-stderr-line"
    expect "flap-dying"
    expect "partial-stdout-line"
    expect "LOGEDGE-HEAD"
    expect "LOGEDGE-B1"
    expect "LOGEDGE-B2"
    expect "LOGINTERLEAVE-B"
    expect "FIXTURE interleaved log verified"
fi
