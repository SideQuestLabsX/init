/* Task rules for the boot tests, selected with
 * -DINIT_TASK_RULES_H='"tests/fixtures/test_rules.h"'. */

static const TaskRule TASK_RULES[] =
{
    { .name = "ok",
      .flags = RULE_CRITICAL,
      .graceNs = NS_PER_SEC,
      .probeIntervalNs = 2ull * NS_PER_SEC,
      .probeTimeoutNs = 3ull * NS_PER_SEC,
      .outPolicy = LOGP_BOTH },

    /* three failures reaches FAILED in a few hundred milliseconds */
    { .name = "flap", .maxRestarts = 3 },

    { .name = "oneshot", .outPolicy = LOGP_BOTH },

    /* a check that never exits: timeouts have to count as failures on their
       own, or a probe that never reaps drops the task out of supervision */
    { .name = "hangcheck",
      .graceNs = 100ull * NS_PER_MS,
      .probeIntervalNs = 200ull * NS_PER_MS,
      .probeTimeoutNs = 300ull * NS_PER_MS,
      .maxRestarts = 2 },

    { .name = "caps", .capMask = CAP_BIT(10) },

    { .name = "identity",
      .uid = 65534,
      .gid = 65534,
      .capMask = CAP_BIT(10) },

    { .name = "impossible", .uid = 65534, .maxRestarts = 2 },

    { .name = NULL },
};
