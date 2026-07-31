/* Everything configurable lives here. Edit it, rebuild, done.
 *
 * init parses no configuration at runtime. It reads a directory of executables
 * and nothing else: no config file to get wrong, to go missing, to be writable
 * by something that should not be writing it.
 *
 * Included from init.c once the base types exist. */

/* ---- what to build ---------------------------------------------------- */

#ifndef FEATURE_CAPABILITY_DROP
  #define FEATURE_CAPABILITY_DROP 1   /* drop privileges before execve */
#endif
#ifndef FEATURE_EXEC_PROBES
  #define FEATURE_EXEC_PROBES 1       /* <task>.check liveness probes */
#endif
#ifndef FEATURE_WATCHDOG
  #define FEATURE_WATCHDOG 1          /* probe-gated hardware watchdog */
#endif
#ifndef FEATURE_LOG_CAPTURE
  #define FEATURE_LOG_CAPTURE 1       /* pipe capture into the shared ring */
#endif
#ifndef FEATURE_LOG_DISK
  #define FEATURE_LOG_DISK 1          /* forked writer, batched disk flush */
#endif
#ifndef OFFLINE_MODE
  #define OFFLINE_MODE 0              /* 1 strips all networking and SNTP */
#endif

/* ---- where things live ------------------------------------------------ */

#define CFG_TASK_DIR      "/tasks"
#define CFG_LOG_PATH      "/var/log/init.log"
#define CFG_WATCHDOG_DEV  "/dev/watchdog"

/* uid the log writer runs as. 0 keeps it root. */
#define CFG_LOGD_UID 0

/* ---- limits ----------------------------------------------------------- */

#define CFG_PATH_MAX        256
#define CFG_NAME_MAX        64
#define CFG_MAX_TASKS       48
#define CFG_MAX_TASK_DIRS   16
#define CFG_LINE_MAX        512
#define CFG_ARENA_BYTES     (256u * 1024u)

/* Holds 1024 slots plus the ring header */
#define CFG_LOG_RING_BYTES  (132u * 1024u)

_Static_assert(CFG_PATH_MAX >= CFG_NAME_MAX + sizeof(CFG_TASK_DIR) + 8,
               "CFG_PATH_MAX cannot hold a task and check path");
_Static_assert(CFG_PATH_MAX <= 0xffffu,
               "CFG_PATH_MAX cannot use a 16-bit name offset");

/* ---- timing ----------------------------------------------------------- */

/* respawn backoff: doubles from min to max, resets once a task has stayed up
 * for CFG_STABLE_NS */
#define CFG_BACKOFF_MIN_NS   (100ull * NS_PER_MS)
#define CFG_BACKOFF_MAX_NS   (60ull * NS_PER_SEC)
#ifndef CFG_STABLE_NS
  #define CFG_STABLE_NS      (60ull * NS_PER_SEC)
#endif
#define CFG_MAX_RESTARTS     10

#define CFG_PROBE_INTERVAL_NS (30ull * NS_PER_SEC)
#define CFG_PROBE_TIMEOUT_NS  (10ull * NS_PER_SEC)
#define CFG_PROBE_GRACE_NS    (15ull * NS_PER_SEC)
#define CFG_PROBE_FAIL_LIMIT  3
#define CFG_PROC_SAMPLE_NS    (5ull * NS_PER_SEC)

#define CFG_WDOG_TIMEOUT_SEC  30
#define CFG_WDOG_PET_NS       (10ull * NS_PER_SEC)

#define CFG_SHUTDOWN_GRACE_NS (5ull * NS_PER_SEC)
#ifndef CFG_RESTART_GRACE_NS
  #define CFG_RESTART_GRACE_NS CFG_SHUTDOWN_GRACE_NS
#endif
#define CFG_LOOP_MAX_WAIT_NS  (1ull * NS_PER_SEC)

/* ---- disk logging ----------------------------------------------------- */

/* Flush on a timer measured in minutes. Erase blocks run to megabytes, so
 * frequent small appends are the worst possible pattern for flash. */
#ifndef CFG_LOGD_FLUSH_NS
  #define CFG_LOGD_FLUSH_NS (120ull * NS_PER_SEC)
#endif
#define CFG_LOGD_BUF_BYTES   (64u * 1024u)
#define CFG_LOGD_FLUSH_BYTES (32u * 1024u)
#define CFG_LOGD_MAX_BYTES   (2u * 1024u * 1024u)
#define CFG_LOGD_ROTATIONS   3

_Static_assert(CFG_LOGD_BUF_BYTES <= CFG_LOGD_MAX_BYTES,
               "log buffer must fit within one rotation");

/* A writer heartbeat unchanged for this long is treated as stalled */
#ifndef CFG_LOGD_STALL_NS
  #define CFG_LOGD_STALL_NS (30ull * NS_PER_SEC)
#endif

/* ---- wall clock ------------------------------------------------------- */

/* Seconds east of UTC, applied to the wall-clock schedules. Fixed: reading tzdata
 * would mean a parser, a file dependency and a DST table, and an appliance
 * knows its own offset at image-build time. 3 * 3600 is UTC+3. */
#define CFG_TZ_OFFSET_SEC 0

/* ---- time sync -------------------------------------------------------- */

/* dotted quad only, init resolves no hostnames */
#ifndef CFG_SNTP_SERVER
  #define CFG_SNTP_SERVER   "162.159.200.1"
#endif
#ifndef CFG_SNTP_PORT
  #define CFG_SNTP_PORT     123
#endif
#define CFG_SNTP_POLL_NS    (3600ull * NS_PER_SEC)
#ifndef CFG_SNTP_RETRY_NS
  #define CFG_SNTP_RETRY_NS (60ull * NS_PER_SEC)
#endif
#define CFG_SNTP_TIMEOUT_NS (5ull * NS_PER_SEC)

/* ---- output routing --------------------------------------------------- */

#define LOGP_INHERIT 0u   /* use the CFG_ default */
#define LOGP_DROP    1u   /* /dev/null, no capture */
#define LOGP_RING    2u   /* RAM only, readable through the shared ring */
#define LOGP_DISK    4u   /* batched to the log volume */
#define LOGP_BOTH    (LOGP_RING | LOGP_DISK)

/* Errors and crash output reach persistent storage, routine chatter does not.
 * A high-volume stream should be moved to LOGP_RING per task below: beyond the
 * write volume, anything recording user activity does not belong on flash by
 * accident. */
#define CFG_STDOUT_POLICY LOGP_RING
#define CFG_STDERR_POLICY LOGP_BOTH

#define LOGP_IS_ROUTE(p) \
    ((p) == LOGP_DROP || (p) == LOGP_RING || \
     (p) == LOGP_DISK || (p) == LOGP_BOTH)

_Static_assert(LOGP_IS_ROUTE(CFG_STDOUT_POLICY),
               "CFG_STDOUT_POLICY must be a concrete LOGP_ route");
_Static_assert(LOGP_IS_ROUTE(CFG_STDERR_POLICY),
               "CFG_STDERR_POLICY must be a concrete LOGP_ route");

/* ---- per-task overrides ----------------------------------------------- */

#define RULE_CRITICAL 0x1u   /* failure withholds the watchdog pet */

#define CAP_BIT(n) (1ull << (n))

typedef struct
{
    const char *name;            /* task filename, e.g. "dns" */
    u32  uid;                    /* 0 leaves it root */
    u32  gid;                    /* 0 with a uid set means "same as uid" */
    u64  capMask;                /* capabilities retained across execve */
    u32  flags;                  /* RULE_* */
    u32  maxRestarts;            /* 0 uses CFG_MAX_RESTARTS */
    u64  probeIntervalNs;        /* 0 uses the CFG_PROBE_* defaults */
    u64  probeTimeoutNs;
    u64  graceNs;
    u8   outPolicy;              /* LOGP_INHERIT uses CFG_STDOUT_POLICY */
    u8   errPolicy;              /* LOGP_INHERIT uses CFG_STDERR_POLICY */
} TaskRule;

/* Matched by name against every task, first hit wins. Tasks with no entry run
 * with the defaults above, which is most of them. NULL name terminates.
 *
 * An image build that wants its own table without forking this file can point
 * INIT_TASK_RULES_H at one; the boot tests do exactly that. */
#ifdef INIT_TASK_RULES_H
  #include INIT_TASK_RULES_H
#else
static const TaskRule TASK_RULES[] =
{
    /* CAP_BIT(10) is CAP_NET_BIND_SERVICE:
     * { .name = "example", .uid = 65534, .gid = 65534,
     *   .capMask = CAP_BIT(10), .flags = RULE_CRITICAL }, */

    { .name = NULL },
};
#endif
