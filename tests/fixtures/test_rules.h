static const TaskRule TASK_RULES[] =
{
    { .name = "ok",
      .flags = RULE_CRITICAL,
      .graceNs = NS_PER_SEC,
      .probeIntervalNs = 2ull * NS_PER_SEC,
      .probeTimeoutNs = 3ull * NS_PER_SEC,
      .outPolicy = LOGP_BOTH },

    /* Reach FAILED within the boot harness timeout */
    { .name = "flap", .maxRestarts = 3 },

    { .name = "oneshot", .outPolicy = LOGP_BOTH },

    { .name = "stable",
      .stableNs = 100ull * NS_PER_MS,
      .maxRestarts = 2 },

    { .name = "status-reader", .outPolicy = LOGP_BOTH },

    { .name = "signal_logd", .outPolicy = LOGP_BOTH },

    /* Probe timeouts must count toward restart failure */
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

    { .name = "midprobe",
      .graceNs = 50ull * NS_PER_MS,
      .probeIntervalNs = 2ull * NS_PER_SEC,
      .probeTimeoutNs = 30ull * NS_PER_SEC,
      .maxRestarts = 2 },

    { .name = "ignoreterm",
      .graceNs = 50ull * NS_PER_MS,
      .probeIntervalNs = 50ull * NS_PER_MS,
      .probeTimeoutNs = NS_PER_SEC,
      .maxRestarts = 1 },

    { .name = "probefail",
      .stableNs = 500ull * NS_PER_MS,
      .graceNs = 700ull * NS_PER_MS,
      .probeIntervalNs = 50ull * NS_PER_MS,
      .probeTimeoutNs = NS_PER_SEC,
      .maxRestarts = 2 },

    { .name = "tree",
      .graceNs = 50ull * NS_PER_MS,
      .probeIntervalNs = 50ull * NS_PER_MS,
      .probeTimeoutNs = NS_PER_SEC,
      .maxRestarts = 2 },

    { .name = NULL },
};
