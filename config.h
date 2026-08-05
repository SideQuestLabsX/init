/* Included by init.c after its base types */

/* features */

#ifndef FEATURE_CAPABILITY_DROP
  #define FEATURE_CAPABILITY_DROP 1
#endif
#ifndef FEATURE_EXEC_PROBES
  #define FEATURE_EXEC_PROBES 1
#endif
#ifndef FEATURE_WATCHDOG
  #define FEATURE_WATCHDOG 1   /* pet only while critical tasks are healthy */
#endif
#ifndef FEATURE_LOG_CAPTURE
  #define FEATURE_LOG_CAPTURE 1
#endif
#ifndef FEATURE_LOG_DISK
  #define FEATURE_LOG_DISK 1
#endif
#ifndef FEATURE_PERSIST_SCHEDULE
  #define FEATURE_PERSIST_SCHEDULE 0
#endif
#ifndef FEATURE_STATIC_TASKS
  #define FEATURE_STATIC_TASKS 0
#endif
#ifndef FEATURE_TASK_DISCOVERY
  #define FEATURE_TASK_DISCOVERY 1
#endif
#if FEATURE_STATIC_TASKS
  #undef FEATURE_TASK_DISCOVERY
  #define FEATURE_TASK_DISCOVERY 0
#endif
#ifndef OFFLINE_MODE
  #define OFFLINE_MODE 0
#endif

#if FEATURE_STATIC_TASKS
typedef struct
{
    const char *schedule;
    const char *name;
} StaticTaskSpec;
#endif

/* paths */

#ifndef CFG_TASK_DIR
  #define CFG_TASK_DIR      "/tasks"
#endif
#ifndef CFG_LOG_PATH
  #define CFG_LOG_PATH      "/var/log/init.log"
#endif
#ifndef CFG_WATCHDOG_DEV
  #define CFG_WATCHDOG_DEV  "/dev/watchdog"
#endif

/* 0 keeps the log writer as root */
#ifndef CFG_LOGD_UID
  #define CFG_LOGD_UID 0
#endif

/* limits */

#ifndef CFG_PATH_MAX
  #define CFG_PATH_MAX        256
#endif
#ifndef CFG_NAME_MAX
  #define CFG_NAME_MAX        64
#endif
#ifndef CFG_MAX_TASKS
  #define CFG_MAX_TASKS       48
#endif
#ifndef CFG_MAX_TASK_DIRS
  #define CFG_MAX_TASK_DIRS   16
#endif
#ifndef CFG_LINE_MAX
  #define CFG_LINE_MAX        512
#endif
#ifndef CFG_ARENA_BYTES
  #define CFG_ARENA_BYTES     (256u * 1024u)
#endif

#ifndef CFG_LOG_RING_BYTES
  #define CFG_LOG_RING_BYTES  (132u * 1024u)
#endif

#ifndef CFG_SCHEDULE_STATE_PATH
  #define CFG_SCHEDULE_STATE_PATH     "/var/lib/init.schedule"
#endif
#ifndef CFG_SCHEDULE_STATE_TMP_PATH
  #define CFG_SCHEDULE_STATE_TMP_PATH "/var/lib/init.schedule.tmp"
#endif

_Static_assert(CFG_PATH_MAX >= CFG_NAME_MAX + sizeof(CFG_TASK_DIR) + 8,
               "CFG_PATH_MAX cannot hold a task and check path");
_Static_assert(CFG_PATH_MAX <= 0xffffu,
               "CFG_PATH_MAX cannot use a 16-bit name offset");

/* timing */

#ifndef CFG_BACKOFF_MIN_NS
  #define CFG_BACKOFF_MIN_NS   (100ull * NS_PER_MS)
#endif
#ifndef CFG_BACKOFF_MAX_NS
  #define CFG_BACKOFF_MAX_NS   (60ull * NS_PER_SEC)
#endif
#ifndef CFG_STABLE_NS
  #define CFG_STABLE_NS      (60ull * NS_PER_SEC)
#endif
#ifndef CFG_MAX_RESTARTS
  #define CFG_MAX_RESTARTS     10
#endif

#ifndef CFG_PROBE_INTERVAL_NS
  #define CFG_PROBE_INTERVAL_NS (30ull * NS_PER_SEC)
#endif
#ifndef CFG_PROBE_TIMEOUT_NS
  #define CFG_PROBE_TIMEOUT_NS  (10ull * NS_PER_SEC)
#endif
#ifndef CFG_PROBE_GRACE_NS
  #define CFG_PROBE_GRACE_NS    (15ull * NS_PER_SEC)
#endif
#ifndef CFG_PROBE_FAIL_LIMIT
  #define CFG_PROBE_FAIL_LIMIT  3
#endif
#ifndef CFG_PROC_SAMPLE_NS
  #define CFG_PROC_SAMPLE_NS    (5ull * NS_PER_SEC)
#endif

#ifndef CFG_WDOG_TIMEOUT_SEC
  #define CFG_WDOG_TIMEOUT_SEC  30
#endif
#ifndef CFG_WDOG_PET_NS
  #define CFG_WDOG_PET_NS       (10ull * NS_PER_SEC)
#endif

#ifndef CFG_SHUTDOWN_GRACE_NS
  #define CFG_SHUTDOWN_GRACE_NS (5ull * NS_PER_SEC)
#endif
#ifndef CFG_RESTART_GRACE_NS
  #define CFG_RESTART_GRACE_NS CFG_SHUTDOWN_GRACE_NS
#endif
#ifndef CFG_LOOP_MAX_WAIT_NS
  #define CFG_LOOP_MAX_WAIT_NS  (1ull * NS_PER_SEC)
#endif
#ifndef CFG_TASK_SCAN_NS
  #define CFG_TASK_SCAN_NS      (1ull * NS_PER_SEC)
#endif
#ifndef CFG_TASK_DISCOVERY_GRACE_NS
  #define CFG_TASK_DISCOVERY_GRACE_NS (250ull * NS_PER_MS)
#endif

/* disk logging */

/* Batch writes to limit flash write amplification */
#ifndef CFG_LOGD_FLUSH_NS
  #define CFG_LOGD_FLUSH_NS (120ull * NS_PER_SEC)
#endif
#ifndef CFG_LOGD_BUF_BYTES
  #define CFG_LOGD_BUF_BYTES   (64u * 1024u)
#endif
#ifndef CFG_LOGD_FLUSH_BYTES
  #define CFG_LOGD_FLUSH_BYTES (32u * 1024u)
#endif
#ifndef CFG_LOGD_MAX_BYTES
  #define CFG_LOGD_MAX_BYTES   (2u * 1024u * 1024u)
#endif
#ifndef CFG_LOGD_ROTATIONS
  #define CFG_LOGD_ROTATIONS   3
#endif

_Static_assert(CFG_LOGD_BUF_BYTES <= CFG_LOGD_MAX_BYTES,
               "log buffer must fit within one rotation");

#ifndef CFG_LOGD_STALL_NS
  #define CFG_LOGD_STALL_NS (30ull * NS_PER_SEC)
#endif

/* wall clock */

/* Fixed seconds east of UTC, no tzdata or DST */
#ifndef CFG_TZ_OFFSET_SEC
  #define CFG_TZ_OFFSET_SEC 0
#endif

/* time sync */

/* Dotted quad only */
#ifndef CFG_SNTP_SERVER
  #define CFG_SNTP_SERVER   "162.159.200.1"
#endif
#ifndef CFG_SNTP_PORT
  #define CFG_SNTP_PORT     123
#endif
#ifndef CFG_SNTP_POLL_NS
  #define CFG_SNTP_POLL_NS    (3600ull * NS_PER_SEC)
#endif
#ifndef CFG_SNTP_RETRY_NS
  #define CFG_SNTP_RETRY_NS (60ull * NS_PER_SEC)
#endif
#ifndef CFG_SNTP_TIMEOUT_NS
  #define CFG_SNTP_TIMEOUT_NS (5ull * NS_PER_SEC)
#endif

/* output routing */

#define LOGP_INHERIT 0u
#define LOGP_DROP    1u
#define LOGP_RING    2u
#define LOGP_DISK    4u
#define LOGP_BOTH    (LOGP_RING | LOGP_DISK)

#ifndef CFG_STDOUT_POLICY
  #define CFG_STDOUT_POLICY LOGP_RING
#endif
#ifndef CFG_STDERR_POLICY
  #define CFG_STDERR_POLICY LOGP_BOTH
#endif

#define LOGP_IS_ROUTE(p) \
    ((p) == LOGP_DROP || (p) == LOGP_RING || \
     (p) == LOGP_DISK || (p) == LOGP_BOTH)

_Static_assert(LOGP_IS_ROUTE(CFG_STDOUT_POLICY),
               "CFG_STDOUT_POLICY must be a concrete LOGP_ route");
_Static_assert(LOGP_IS_ROUTE(CFG_STDERR_POLICY),
               "CFG_STDERR_POLICY must be a concrete LOGP_ route");

#if FEATURE_STATIC_TASKS
#ifndef INIT_STATIC_TASKS_H
  #error "FEATURE_STATIC_TASKS requires INIT_STATIC_TASKS_H"
#endif
#include INIT_STATIC_TASKS_H
#endif

/* task overrides */

#define RULE_CRITICAL 0x1u   /* gates watchdog petting */

#define CAP_BIT(n) (1ull << (n))

typedef struct
{
    const char *name;
    u32  uid;                    /* 0 leaves it root */
    u32  gid;                    /* 0 with a uid set means "same as uid" */
    u64  capMask;                /* retained across execve */
    u32  flags;
    u32  maxRestarts;            /* 0 uses CFG_MAX_RESTARTS */
    u64  stableNs;               /* 0 uses CFG_STABLE_NS */
    u64  probeIntervalNs;        /* 0 uses CFG_PROBE_* defaults */
    u64  probeTimeoutNs;
    u64  graceNs;
    u8   outPolicy;              /* LOGP_INHERIT uses CFG_STDOUT_POLICY */
    u8   errPolicy;              /* LOGP_INHERIT uses CFG_STDERR_POLICY */
} TaskRule;

/* First match wins, NULL terminates, INIT_TASK_RULES_H replaces the table */
#ifdef INIT_TASK_RULES_H
  #include INIT_TASK_RULES_H
#else
static const TaskRule TASK_RULES[] =
{
    { .name = NULL },
};
#endif
