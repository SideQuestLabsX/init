/* Host unit tests for the parts that touch no syscalls.
 *
 * These run anywhere a C compiler does, with no target device, no root and no
 * qemu. The arithmetic deciding when a task respawns and the parser eating
 * untrusted network input are the parts worth testing off-target. The
 * syscall-bound half is covered by the boot tests in tools/. */

#include <stdio.h>
#include <string.h>

/* the shipped source, cut down by INIT_HOSTED. Including it keeps the tests
 * pinned to what actually ships. */
#include "init.c"

static u32 G_CHECKS = 0;
static u32 G_FAILS = 0;
static const char *G_GROUP = "";

#define CHECK(cond)                                                            \
    do {                                                                       \
        G_CHECKS++;                                                            \
        if(!(cond))                                                            \
        {                                                                      \
            G_FAILS++;                                                         \
            printf("  FAIL [%s] %s:%d: %s\n", G_GROUP, __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while(0)

#define GROUP(name) G_GROUP = (name)

/* ------------------------------------------------------------------ string */

static void TestStrings(void)
{
    GROUP("str");

    char buf[8];
    CHECK(StrCopy(buf, sizeof(buf), "abc") == 3);
    CHECK(StrEq(buf, "abc"));

    CHECK(StrCopy(buf, sizeof(buf), "0123456789") == 7);
    CHECK(StrEq(buf, "0123456"));

    CHECK(StrCopyOk(buf, sizeof(buf), "0123456"));
    CHECK(!StrCopyOk(buf, sizeof(buf), "01234567"));
    CHECK(StrCopyNOk(buf, sizeof(buf), "01234567", 7));
    CHECK(!StrCopyNOk(buf, sizeof(buf), "01234567", 8));

    StrCopy(buf, sizeof(buf), "ab");
    CHECK(StrCat(buf, sizeof(buf), "cd") == 4);
    CHECK(StrEq(buf, "abcd"));
    CHECK(StrCat(buf, sizeof(buf), "efghijk") == 7);
    CHECK(StrEq(buf, "abcdefg"));
    CHECK(!StrCatOk(buf, sizeof(buf), "x"));

    char path[32];
    CHECK(PathJoin(path, sizeof(path), "/tasks/always", "resolver") == 22);
    CHECK(StrEq(path, "/tasks/always/resolver"));
    CHECK(PathJoinOk(path, sizeof(path), "/tasks/always/", "resolver"));
    CHECK(StrEq(path, "/tasks/always/resolver"));

    char nameLimit[CFG_NAME_MAX + 1];
    memset(nameLimit, 'n', CFG_NAME_MAX);
    nameLimit[CFG_NAME_MAX] = '\0';
    char identity[CFG_NAME_MAX];
    CHECK(!StrCopyOk(identity, sizeof(identity), nameLimit));
    nameLimit[CFG_NAME_MAX - 1] = '\0';
    CHECK(StrCopyOk(identity, sizeof(identity), nameLimit));

    char pathLimit[CFG_PATH_MAX];
    memset(pathLimit, 'p', sizeof(pathLimit) - 1);
    pathLimit[sizeof(pathLimit) - 1] = '\0';
    char checkPath[CFG_PATH_MAX];
    CHECK(StrCopyOk(checkPath, sizeof(checkPath), pathLimit));
    CHECK(!StrCatOk(checkPath, sizeof(checkPath), ".check"));
    CHECK(!PathJoinOk(checkPath, sizeof(checkPath), pathLimit, "task"));

    CHECK(StrStartsWith("resolver", "res"));
    CHECK(!StrStartsWith("resolver", "met"));
    CHECK(StrEndsWith("resolver.check", ".check"));
    CHECK(!StrEndsWith("check", "resolver.check"));
    CHECK(StrEndsWith("abc", ""));

    CHECK(StrChr("abc", 'b') != NULL);
    CHECK(StrChr("abc", 'z') == NULL);
    CHECK(StrRChr("a/b/c", '/') != NULL && *(StrRChr("a/b/c", '/') + 1) == 'c');

    CHECK(StrCmp("a", "b") < 0);
    CHECK(StrCmp("b", "a") > 0);
    CHECK(StrCmp("ab", "ab") == 0);
    CHECK(StrNCmp("abcd", "abzz", 2) == 0);
}

static void TestNumbers(void)
{
    GROUP("numbers");

    u64 v = 0;
    const char *end = NULL;

    CHECK(ParseU64("1234x", &v, &end) && v == 1234 && *end == 'x');
    CHECK(!ParseU64("x", &v, NULL));
    CHECK(!ParseU64("", &v, NULL));
    CHECK(ParseU64("18446744073709551615", &v, NULL) && v == 18446744073709551615ull);
    CHECK(!ParseU64("18446744073709551616", &v, NULL));

    u64 ns = 0;
    CHECK(ParseDuration("300s", &ns) && ns == 300ull * NS_PER_SEC);
    CHECK(ParseDuration("300", &ns) && ns == 300ull * NS_PER_SEC);
    CHECK(ParseDuration("500ms", &ns) && ns == 500ull * NS_PER_MS);
    CHECK(ParseDuration("5m", &ns) && ns == 300ull * NS_PER_SEC);
    CHECK(ParseDuration("24h", &ns) && ns == 86400ull * NS_PER_SEC);
    CHECK(ParseDuration("7d", &ns) && ns == 604800ull * NS_PER_SEC);
    CHECK(!ParseDuration("7w", &ns));
    CHECK(!ParseDuration("weekly", &ns));
    CHECK(!ParseDuration("18446744073709551615d", &ns));

    /* every accepted directory spelling, in one place. An interval and a
       calendar recurrence are told apart by the trailing -HH-MM: 3d counts from
       boot, 3d-03-30 counts epoch days and fires at a wall-clock time. */
    CalSpec c;
    CHECK(ParseDuration("500ms", &ns) && ns == 500ull * NS_PER_MS);
    CHECK(ParseDuration("30s", &ns) && ns == 30ull * NS_PER_SEC);
    CHECK(ParseDuration("5m", &ns) && ns == 300ull * NS_PER_SEC);
    CHECK(ParseDuration("1h", &ns) && ns == 3600ull * NS_PER_SEC);
    CHECK(ParseDuration("3d", &ns) && ns == 3ull * 86400ull * NS_PER_SEC);
    CHECK(!ParseCalendar("3d", &c));              /* no time, so it is an interval */

    CHECK(ParseCalendar("1d-03-30", &c) &&
          c.kind == CAL_EVERY_NDAY && c.param == 1 && c.daySec == 3 * 3600 + 30 * 60);
    CHECK(ParseCalendar("4d-03-30", &c) && c.param == 4);
    CHECK(ParseCalendar("366d-00-00", &c) && c.param == 366 && c.daySec == 0);
    CHECK(ParseCalendar("sun-03-00", &c) && c.kind == CAL_WEEKDAY && c.param == 0);
    CHECK(ParseCalendar("mon-00-00", &c) && c.param == 1);
    CHECK(ParseCalendar("tue-01-00", &c) && c.param == 2);
    CHECK(ParseCalendar("wed-12-00", &c) && c.param == 3);
    CHECK(ParseCalendar("thu-13-45", &c) && c.param == 4);
    CHECK(ParseCalendar("fri-18-05", &c) && c.param == 5);
    CHECK(ParseCalendar("sat-23-59", &c) &&
          c.param == 6 && c.daySec == 23 * 3600 + 59 * 60);

    CHECK(!ParseCalendar("daily-03-30", &c));     /* 1d-03-30 says it once */
    CHECK(!ParseCalendar("1d-24-00", &c));
    CHECK(!ParseCalendar("1d-12-60", &c));
    CHECK(!ParseCalendar("1d-3-30", &c));         /* both fields are two digits */
    CHECK(!ParseCalendar("1d-03-30-00", &c));
    CHECK(!ParseCalendar("1d-ab-cd", &c));
    CHECK(!ParseCalendar("0d-03-30", &c));        /* every zero days is nothing */
    CHECK(!ParseCalendar("367d-03-30", &c));      /* past CAL_MAX_PERIOD_DAYS */
    CHECK(!ParseCalendar("xyz-03-30", &c));
    CHECK(!ParseCalendar("d-03-30", &c));
    CHECK(!ParseCalendar("sunday-03-00", &c));
    CHECK(!ParseCalendar("24h", &c));

    /* intervals keep their phase: the deadline advances from the deadline, so
       a task that wakes late does not carry that lateness into the next slot */
    CHECK(IntervalAdvance(1000, 100, 1000) == 1100);
    CHECK(IntervalAdvance(1000, 100, 1050) == 1100);
    /* a whole slot went by, so it is dropped rather than run back to back */
    CHECK(IntervalAdvance(1000, 100, 1150) == 1200);
    CHECK(IntervalAdvance(1000, 100, 5000) == 5100);
    CHECK(IntervalAdvance(1000, 0, 4242) == 4242);

    /* the point of all of it: a thousand cycles, every one of them seven
       nanoseconds late, and the phase has not moved */
    u64 dl = 1000;
    for(u32 cycle = 0; cycle < 1000; cycle++)
        dl = IntervalAdvance(dl, 100, dl + 7);
    CHECK(dl == 1000 + 1000 * 100);

    /* falling far behind realigns to the grid instead of accumulating */
    dl = IntervalAdvance(1000, 100, 999999);
    CHECK(dl > 999999 && (dl - 1000) % 100 == 0);

    /* 1970-01-01 was a Thursday */
    CHECK(LocalWeekday(0) == 4);
    CHECK(LocalWeekday(3) == 0);                  /* 1970-01-04, a Sunday */
    CHECK(LocalDayNum(0, 0) == 0);
    CHECK(LocalDayNum(86400, 0) == 1);
    CHECK(LocalDaySec(3 * 3600 + 30 * 60, 0) == 3 * 3600 + 30 * 60);
    CHECK(LocalDaySec(0, 3 * 3600) == 3 * 3600);
    /* west of UTC against an epoch clock would go negative */
    CHECK(LocalDaySec(0, -5 * 3600) == 0);
    CHECK(LocalDaySec(86400, -3600) == 23 * 3600);

    CalSpec everyday = { CAL_EVERY_NDAY, 1, 3600 };
    CHECK(SecsUntilCalendar(&everyday, 0, 0) == 3600);
    /* landing on the target waits for the next one rather than refiring */
    CHECK(SecsUntilCalendar(&everyday, 3600, 0) == 86400);
    CHECK(SecsUntilCalendar(&everyday, 3601, 0) == 86400 - 1);

    /* epoch-phased, so day 0 matches and the next is day 3 */
    CalSpec every3 = { CAL_EVERY_NDAY, 3, 0 };
    CHECK(SecsUntilCalendar(&every3, 1, 0) == 3 * 86400 - 1);
    CHECK(SecsUntilCalendar(&every3, 86400, 0) == 2 * 86400);

    /* Sunday is day 3, so from day 0 at midnight that is three days out */
    CalSpec sun = { CAL_WEEKDAY, 0, 0 };
    CHECK(SecsUntilCalendar(&sun, 1, 0) == 3 * 86400 - 1);
    CHECK(SecsUntilCalendar(&sun, 3 * 86400, 0) == 7 * 86400);

    u32 ip = 0;
    CHECK(ParseIPv4("192.168.1.10", &ip) && ip == 0xc0a8010au);
    CHECK(ParseIPv4("0.0.0.0", &ip) && ip == 0);
    CHECK(!ParseIPv4("256.1.1.1", &ip));
    CHECK(!ParseIPv4("1.2.3", &ip));
    CHECK(!ParseIPv4("1.2.3.4.5", &ip));
    CHECK(!ParseIPv4("time.example.com", &ip));

    char out[32];
    CHECK(FmtU64(out, sizeof(out), 0) == 1 && StrEq(out, "0"));
    CHECK(FmtI64(out, sizeof(out), -42) == 3 && StrEq(out, "-42"));
    FmtI64(out, sizeof(out), (i64)(-9223372036854775807LL - 1));
    CHECK(StrEq(out, "-9223372036854775808"));
    CHECK(FmtHex(out, sizeof(out), 0xdeadu, 8) == 8 && StrEq(out, "0000dead"));

    CHECK(Fmt(out, sizeof(out), "%s=%d", "x", -7) == 4 && StrEq(out, "x=-7"));
    Fmt(out, sizeof(out), "%u %x %c%%", 42u, 255u, 'z');
    CHECK(StrEq(out, "42 ff z%"));

    /* length modifiers have to stay distinguishable, or every %zu on a 32-bit
     * target reads eight bytes off a four-byte argument */
    Fmt(out, sizeof(out), "%llu", 4294967296ull);
    CHECK(StrEq(out, "4294967296"));
    Fmt(out, sizeof(out), "%zu|%lu", (usize)7, (unsigned long)8);
    CHECK(StrEq(out, "7|8"));
    Fmt(out, sizeof(out), "%lld", (i64)-4294967296ll);
    CHECK(StrEq(out, "-4294967296"));
    Fmt(out, sizeof(out), "%u", 4294967295u);
    CHECK(StrEq(out, "4294967295"));

    char tiny[5];
    Fmt(tiny, sizeof(tiny), "%s", "abcdefgh");
    CHECK(StrLen(tiny) == 4);

    CHECK(Hash64("abc", 3) == Hash64("abc", 3));
    CHECK(Hash64("abc", 3) != Hash64("abd", 3));
}

static void TestNames(void)
{
    GROUP("names");

    char *names[CFG_MAX_TASKS];
    char storage[CFG_MAX_TASKS][16];
    usize count = 0;

    for(u32 i = 0; i < CFG_MAX_TASKS + 5; i++)
    {
        u32 value = (i * 17u) % (CFG_MAX_TASKS + 5u);
        char candidate[16];
        Fmt(candidate, sizeof(candidate), "task-%02u", value);
        NameSetInsert(names, (char *)storage, &count, CFG_MAX_TASKS,
                      sizeof(storage[0]), candidate);
    }

    CHECK(count == CFG_MAX_TASKS);
    for(u32 i = 0; i < CFG_MAX_TASKS; i++)
    {
        char expected[16];
        Fmt(expected, sizeof(expected), "task-%02u", i);
        CHECK(StrEq(names[i], expected));
    }

    char tooLong[17];
    memset(tooLong, 'x', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = '\0';
    CHECK(!NameSetInsert(names, (char *)storage, &count, CFG_MAX_TASKS,
                         sizeof(storage[0]), tooLong));
}

/* ------------------------------------------------------------------- arena */

static void TestArena(void)
{
    GROUP("arena");

    static u8 mem[256];
    Arena a;
    ArenaInit(&a, mem, sizeof(mem));

    void *p1 = ArenaAlloc(&a, 1, 1);
    void *p8 = ArenaAlloc(&a, 8, 8);
    CHECK(p1 != NULL && p8 != NULL);
    CHECK(((usize)(uintptr_t)p8 % 8) == 0);

    usize mark = ArenaMark(&a);
    char *dup = ArenaStrDup(&a, "hello");
    CHECK(dup != NULL && StrEq(dup, "hello"));
    ArenaReset(&a, mark);
    CHECK(ArenaMark(&a) == mark);

    /* exhaustion is a defined, counted, non-fatal condition */
    CHECK(ArenaAlloc(&a, 1024, 1) == NULL);
    CHECK(a.exhaustions == 1);
    CHECK(ArenaAlloc(&a, 4, 1) != NULL);
    CHECK(a.peak >= a.used);
}

/* -------------------------------------------------------------------- ring */

#define TEST_RING_SLOTS 8
static u8 G_RING_MEM[sizeof(LogRing) + TEST_RING_SLOTS * LOG_SLOT_BYTES]
    __attribute__((aligned(16)));

static void TestRing(void)
{
    GROUP("ring");

    LogRing *r = (LogRing *)G_RING_MEM;
    CHECK(RingInit(r, sizeof(G_RING_MEM)));
    CHECK(r->slots == TEST_RING_SLOTS);

    LogSlot slot;
    u64 lost = 0;
    CHECK(!RingRead(r, &slot, &lost));

    RingWrite(r, LOG_SRC_ERR, 3, LOG_F_DISK, "hello", 5);
    CHECK(RingPending(r) == 1);
    CHECK(RingRead(r, &slot, &lost));
    CHECK(lost == 0 && slot.len == 5 && slot.stream == LOG_SRC_ERR && slot.task == 3);
    CHECK(memcmp(slot.text, "hello", 5) == 0);
    CHECK((slot.flags & LOG_F_DISK) != 0);
    CHECK(!RingRead(r, &slot, &lost));

    /* overflow drops the oldest, and says how many */
    for(u32 i = 0; i < TEST_RING_SLOTS + 3; i++)
    {
        char line[16];
        usize n = Fmt(line, sizeof(line), "m%u", i);
        RingWrite(r, LOG_SRC_OUT, 0, 0, line, n);
    }
    CHECK(RingRead(r, &slot, &lost));
    CHECK(lost == 3);
    CHECK(StrEq(slot.text, "m3"));

    u32 seen = 1;
    while(RingRead(r, &slot, &lost))
        seen++;
    CHECK(seen == TEST_RING_SLOTS);
    CHECK(r->dropped == 3);

    /* an oversized line splits into flagged continuations rather than truncating */
    char big[LOG_TEXT_MAX * 2 + 10];
    memset(big, 'x', sizeof(big));
    RingWrite(r, LOG_SRC_OUT, 1, 0, big, sizeof(big));
    u32 parts = 0;
    u32 conts = 0;
    while(RingRead(r, &slot, &lost))
    {
        parts++;
        if((slot.flags & LOG_F_CONT) != 0)
            conts++;
    }
    CHECK(parts == 3);
    CHECK(conts == 2);
}

/* ------------------------------------------------------------------- rules */

static void TestRules(void)
{
    GROUP("rules");

    /* the shipped table is empty, so everything falls through to defaults */
    CHECK(TaskRuleFind("anything") == NULL);

    static const TaskRule rules[] =
    {
        { .name = "dns", .uid = 65534, .capMask = CAP_BIT(10),
          .flags = RULE_CRITICAL, .maxRestarts = 3 },
        { .name = "metrics", .outPolicy = LOGP_DROP },
        { .name = NULL },
    };

    const TaskRule *r = NULL;
    for(const TaskRule *it = rules; it->name != NULL; it++)
    {
        if(StrEq(it->name, "dns"))
            r = it;
    }
    CHECK(r != NULL);
    CHECK(r->uid == 65534);
    CHECK(r->capMask == (1ull << 10));
    CHECK((r->flags & RULE_CRITICAL) != 0);
    CHECK(r->maxRestarts == 3);

    /* inherit keeps the table sparse without sharing drop's value */
    CHECK(r->probeIntervalNs == 0);
    CHECK(r->outPolicy == LOGP_INHERIT);
    CHECK(rules[1].outPolicy == LOGP_DROP);

    static const u8 combinations[][4] =
    {
        { LOGP_INHERIT, LOGP_INHERIT, CFG_STDOUT_POLICY, CFG_STDERR_POLICY },
        { LOGP_DROP,    LOGP_INHERIT, LOGP_DROP,          CFG_STDERR_POLICY },
        { LOGP_INHERIT, LOGP_DROP,    CFG_STDOUT_POLICY, LOGP_DROP },
        { LOGP_DROP,    LOGP_DROP,    LOGP_DROP,          LOGP_DROP },
    };
    for(usize i = 0; i < sizeof(combinations) / sizeof(combinations[0]); i++)
    {
        CHECK(LogPolicyResolve(combinations[i][0], CFG_STDOUT_POLICY) ==
              combinations[i][2]);
        CHECK(LogPolicyResolve(combinations[i][1], CFG_STDERR_POLICY) ==
              combinations[i][3]);
    }
    CHECK(LogPolicyResolve(LOGP_RING, CFG_STDOUT_POLICY) == LOGP_RING);
    CHECK(LogPolicyResolve(LOGP_DISK, CFG_STDOUT_POLICY) == LOGP_DISK);
    CHECK(LogPolicyResolve(LOGP_BOTH, CFG_STDOUT_POLICY) == LOGP_BOTH);
}

/* --------------------------------------------------------------- schedules */

static void TestSchedules(void)
{
    GROUP("schedules");

    /* directory names are the whole schedule vocabulary */
    u64 ns = 0;
    CHECK(!ParseDuration("always", &ns));
    CHECK(!ParseDuration("once", &ns));
    CHECK(ParseDuration("2s", &ns) && ns == 2ull * NS_PER_SEC);
    CHECK(ParseDuration("24h", &ns) && ns == 86400ull * NS_PER_SEC);
    CHECK(!ParseDuration("hourly", &ns));
}

/* ----------------------------------------------------------------- backoff */

static void TestBackoff(void)
{
    GROUP("backoff");

    u64 b = BackoffNext(0);
    CHECK(b == CFG_BACKOFF_MIN_NS);

    u32 steps = 0;
    while(b < CFG_BACKOFF_MAX_NS && steps < 64)
    {
        u64 prev = b;
        b = BackoffNext(b);
        CHECK(b > prev);
        steps++;
    }
    CHECK(b == CFG_BACKOFF_MAX_NS);
    CHECK(BackoffNext(b) == CFG_BACKOFF_MAX_NS);

    CHECK(BackoffStable(0, CFG_STABLE_NS));
    CHECK(!BackoffStable(0, CFG_STABLE_NS - 1));
    CHECK(!BackoffStable(100, 50));

    CHECK(RestartFailuresNext(4, 0, CFG_STABLE_NS - 1) == 5);
    CHECK(RestartFailuresNext(4, 0, CFG_STABLE_NS) == 0);

    /* shipped probe timings put a restart past CFG_STABLE_NS, so consecFails resets */
    u64 shippedProbeFailureNs = CFG_PROBE_GRACE_NS +
        (u64)(CFG_PROBE_FAIL_LIMIT - 1) * CFG_PROBE_INTERVAL_NS;
    CHECK(shippedProbeFailureNs > CFG_STABLE_NS);
}

/* ---------------------------------------------------------------- procstat */

static void TestProcStat(void)
{
    GROUP("procstat");

    ProcStat ps;

    static const char simple[] =
        "1234 (dns_blocker) S 1 1234 1234 0 -1 4194560 100 0 0 0 "
        "77 88 0 0 20 0 4 0 900 0 0 0 0";
    CHECK(ProcStatParse(simple, sizeof(simple) - 1, &ps));
    CHECK(ps.state == 'S');
    CHECK(ps.utime == 77 && ps.stime == 88);
    CHECK(ps.threads == 4);

    /* comm can contain spaces and parens, so splitting starts at the last ')' */
    static const char nasty[] =
        "7 (weird ) name (x)) D 1 7 7 0 -1 0 0 0 0 0 "
        "11 22 0 0 20 0 2 0 5 0 0 0 0";
    CHECK(ProcStatParse(nasty, sizeof(nasty) - 1, &ps));
    CHECK(ps.state == 'D');
    CHECK(ps.utime == 11 && ps.stime == 22);
    CHECK(ps.threads == 2);

    static const char zomb[] = "9 (x) Z 1 9 9 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 0";
    CHECK(ProcStatParse(zomb, sizeof(zomb) - 1, &ps));
    CHECK(ps.state == 'Z');

    static const char truncated[] = "9 (x)";
    CHECK(!ProcStatParse(truncated, sizeof(truncated) - 1, &ps));
    CHECK(!ProcStatParse("garbage", 7, &ps));
}

/* -------------------------------------------------------------------- sntp */

static void TestSntp(void)
{
    GROUP("sntp");

    /* Asserted through memory, so a target whose native order already matches
     * the wire is held to the same result. */
    u8 wire[4];
    u32 n32 = Hton32(0x01020304u);
    memcpy(wire, &n32, sizeof(n32));
    CHECK(wire[0] == 1 && wire[1] == 2 && wire[2] == 3 && wire[3] == 4);

    u16 n16 = Hton16(0x0102u);
    memcpy(wire, &n16, sizeof(n16));
    CHECK(wire[0] == 1 && wire[1] == 2);

    u64 unixNs = 1700000000ull * NS_PER_SEC + 500000000ull;
    u64 ntp = SntpNtpFromUnixNs(unixNs);
    u64 back = SntpUnixNsFromNtp(ntp);
    CHECK(back / NS_PER_SEC == unixNs / NS_PER_SEC);
    CHECK((back % NS_PER_SEC) / 1000000 == 500);

    u8 req[SNTP_PKT_BYTES];
    SntpBuildRequest(req, ntp);
    CHECK(req[0] == 0x23);

    u8 reply[SNTP_PKT_BYTES];
    memset(reply, 0, sizeof(reply));
    reply[0] = 0x24;           /* LI 0, VN 4, mode 4 (server) */
    reply[1] = 2;              /* stratum */
    memcpy(&reply[24], &req[40], 8);   /* originate echoes our transmit */
    memcpy(&reply[40], &req[40], 8);   /* server transmit */

    u64 got = 0;
    CHECK(SntpParseReply(reply, sizeof(reply), ntp, &got));
    CHECK(got / NS_PER_SEC == unixNs / NS_PER_SEC);

    /* every rejection path bounds-checking untrusted input */
    CHECK(!SntpParseReply(reply, SNTP_PKT_BYTES - 1, ntp, &got));

    u8 bad[SNTP_PKT_BYTES];

    memcpy(bad, reply, sizeof(bad));
    bad[1] = 0;                                  /* kiss-of-death */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    bad[1] = 16;                                 /* stratum out of range */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    bad[0] = 0x23;                               /* mode 3, not a server reply */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    bad[0] = 0xe4;                               /* LI 3, unsynchronised */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    bad[0] = 0x0c;                               /* VN 1 */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    memset(&bad[24], 0, 8);                      /* originate does not match */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    memset(&bad[40], 0, 8);                      /* zero transmit timestamp */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));

    memcpy(bad, reply, sizeof(bad));
    memset(&bad[40], 0, 4);                      /* pre-1970 seconds */
    CHECK(!SntpParseReply(bad, sizeof(bad), ntp, &got));
}

int main(void)
{
    TestStrings();
    TestNumbers();
    TestNames();
    TestArena();
    TestRing();
    TestRules();
    TestSchedules();
    TestBackoff();
    TestProcStat();
    TestSntp();

    printf("%u checks, %u failures\n", G_CHECKS, G_FAILS);
    return G_FAILS == 0 ? 0 : 1;
}
