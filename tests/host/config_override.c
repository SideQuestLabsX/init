typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

#define NULL ((void *)0)
#define NS_PER_MS 1000000ull
#define NS_PER_SEC 1000000000ull

#define CFG_TASK_DIR "/override/tasks"
#define CFG_LOG_PATH "/override/init.log"
#define CFG_WATCHDOG_DEV "/override/watchdog"
#define FEATURE_PERSIST_SCHEDULE 1
#define FEATURE_TASK_DISCOVERY 0
#define CFG_SCHEDULE_STATE_PATH "/override/schedule.state"
#define CFG_SCHEDULE_STATE_TMP_PATH "/override/schedule.state.tmp"
#define CFG_LOGD_UID 123u
#define CFG_PATH_MAX 320u
#define CFG_NAME_MAX 80u
#define CFG_MAX_TASKS 24u
#define CFG_MAX_TASK_DIRS 12u
#define CFG_LINE_MAX 640u
#define CFG_ARENA_BYTES (320u * 1024u)
#define CFG_LOG_RING_BYTES (140u * 1024u)
#define CFG_BACKOFF_MIN_NS (200ull * NS_PER_MS)
#define CFG_BACKOFF_MAX_NS (30ull * NS_PER_SEC)
#define CFG_STABLE_NS (45ull * NS_PER_SEC)
#define CFG_MAX_RESTARTS 8u
#define CFG_PROBE_INTERVAL_NS (20ull * NS_PER_SEC)
#define CFG_PROBE_TIMEOUT_NS (8ull * NS_PER_SEC)
#define CFG_PROBE_GRACE_NS (12ull * NS_PER_SEC)
#define CFG_PROBE_FAIL_LIMIT 4u
#define CFG_PROC_SAMPLE_NS (4ull * NS_PER_SEC)
#define CFG_WDOG_TIMEOUT_SEC 25
#define CFG_WDOG_PET_NS (8ull * NS_PER_SEC)
#define CFG_SHUTDOWN_GRACE_NS (4ull * NS_PER_SEC)
#define CFG_RESTART_GRACE_NS (3ull * NS_PER_SEC)
#define CFG_LOOP_MAX_WAIT_NS (2ull * NS_PER_SEC)
#define CFG_LOGD_FLUSH_NS (90ull * NS_PER_SEC)
#define CFG_LOGD_BUF_BYTES (96u * 1024u)
#define CFG_LOGD_FLUSH_BYTES (48u * 1024u)
#define CFG_LOGD_MAX_BYTES (3u * 1024u * 1024u)
#define CFG_LOGD_ROTATIONS 4u
#define CFG_LOGD_STALL_NS (20ull * NS_PER_SEC)
#define CFG_TZ_OFFSET_SEC 3600
#define CFG_SNTP_SERVER "192.0.2.1"
#define CFG_SNTP_PORT 9123
#define CFG_SNTP_POLL_NS (1800ull * NS_PER_SEC)
#define CFG_SNTP_RETRY_NS (45ull * NS_PER_SEC)
#define CFG_SNTP_TIMEOUT_NS (4ull * NS_PER_SEC)
#define CFG_STDOUT_POLICY 1u
#define CFG_STDERR_POLICY 2u

#include "../../config.h"

int ConfigOverrideProbe(void)
{
    return TASK_RULES[0].name != NULL;
}
