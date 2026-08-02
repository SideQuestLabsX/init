static const TaskRule TASK_RULES[] =
{
    { .name = "watchdog",
      .flags = RULE_CRITICAL,
      .graceNs = 500ull * NS_PER_MS,
      .probeIntervalNs = 200ull * NS_PER_MS,
      .probeTimeoutNs = 500ull * NS_PER_MS,
      .maxRestarts = 100 },

    { .name = NULL },
};
