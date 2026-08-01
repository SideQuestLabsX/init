#include "fixture.h"

static bool FileExists(const char *path)
{
    isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;
    SysClose((i32)fd);
    return true;
}

#if FEATURE_EXEC_PROBES
static bool FileContains(const char *path, const char *needle)
{
    isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    usize matched = 0;
    usize needleLen = StrLen(needle);
    char buf[256];
    for(;;)
    {
        isize n = SysRead((i32)fd, buf, sizeof(buf));
        if(n <= 0)
            break;
        for(isize i = 0; i < n; i++)
        {
            if(buf[i] == needle[matched])
                matched++;
            else
                matched = buf[i] == needle[0] ? 1u : 0u;
            if(matched == needleLen)
            {
                SysClose((i32)fd);
                return true;
            }
        }
    }

    SysClose((i32)fd);
    return false;
}
#endif

static bool WaitForFile(const char *path, usize attempts)
{
    for(usize i = 0; i < attempts; i++)
    {
        if(FileExists(path))
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}

#if FEATURE_LOG_CAPTURE && FEATURE_LOG_DISK
static void MatchMarker(char c, const char *marker, usize *matched, bool *bFound)
{
    if(c == marker[*matched])
        (*matched)++;
    else
        *matched = c == marker[0] ? 1u : 0u;
    if(marker[*matched] == '\0')
    {
        *bFound = true;
        *matched = 0;
    }
}

static bool VerifyInterleavedLog(void)
{
    static const char begin[] = "LOGINTERLEAVE-A-BEGIN";
    static const char end[] = "LOGINTERLEAVE-A-END";
    static const char other[] = "LOGINTERLEAVE-B";

    isize fd = SysOpen("/var/log/init.log", O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    usize beginMatched = 0;
    usize endMatched = 0;
    usize otherMatched = 0;
    bool bBegin = false;
    bool bEnd = false;
    bool bOther = false;
    bool bReconstructed = false;
    bool bMerged = false;
    char buf[256];

    for(;;)
    {
        isize n = SysRead((i32)fd, buf, sizeof(buf));
        if(n <= 0)
            break;
        for(isize i = 0; i < n; i++)
        {
            MatchMarker(buf[i], begin, &beginMatched, &bBegin);
            MatchMarker(buf[i], end, &endMatched, &bEnd);
            MatchMarker(buf[i], other, &otherMatched, &bOther);
            if(buf[i] == '\n')
            {
                bReconstructed |= bBegin && bEnd;
                bMerged |= bBegin && bOther;
                bBegin = false;
                bEnd = false;
                bOther = false;
                beginMatched = 0;
                endMatched = 0;
                otherMatched = 0;
            }
        }
    }

    bReconstructed |= bBegin && bEnd;
    bMerged |= bBegin && bOther;
    SysClose((i32)fd);
    return bReconstructed && !bMerged;
}

static bool WaitForLogCompletion(void)
{
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        if(FileExists("/dev/log-interleave-complete") &&
           VerifyInterleavedLog())
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}
#endif

#if FEATURE_EXEC_PROBES
static bool WaitForConsoleCompletion(void)
{
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        if(FileContains("/dev/console", "stable: done") &&
           FileContains("/dev/console", "flap: FAILED after") &&
           FileContains("/dev/console", "FIXTURE tick fired") &&
           FileContains("/dev/console", "probefail: FAILED after") &&
           FileContains("/dev/console", "hangcheck: FAILED after") &&
           FileContains("/dev/console", "midprobe: cancelling probe") &&
           FileContains("/dev/console", "ignoreterm: restart grace expired") &&
           FileContains("/dev/console", "FIXTURE tree replacement started"))
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}
#endif

#if !FEATURE_EXEC_PROBES && FEATURE_WATCHDOG
static bool WaitForWatchdogPets(void)
{
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS * 2u; i++)
    {
        isize fd = SysOpen("/dev/watchdog", O_RDONLY | O_CLOEXEC, 0);
        if(fd < 0)
            return true;
        isize size = SysLseek((i32)fd, 0, SEEK_END);
        SysClose((i32)fd);
        if(size < 0 || size >= 3)
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}
#endif

static void DumpLog(void)
{
    isize fd = SysOpen("/var/log/init.log", O_RDONLY, 0);
    if(fd < 0)
    {
        FixtureSay("FIXTURE logfile missing");
        return;
    }

    isize con = SysOpen("/dev/console", O_WRONLY | O_APPEND | O_NOCTTY, 0);
    if(con < 0)
        con = 2;

    SysWrite((i32)con, "FIXTURE logfile begin\n", 22);

    char buf[512];
    for(;;)
    {
        isize n = SysRead((i32)fd, buf, sizeof(buf));
        if(n <= 0)
            break;
        SysWrite((i32)con, buf, (usize)n);
    }

    SysWrite((i32)con, "\nFIXTURE logfile end\n", 21);
    if(con > 2)
        SysClose((i32)con);
    SysClose((i32)fd);
}

void FixtureMain(void)
{
    FixtureSay("FIXTURE selftest waiting");

#if FEATURE_LOG_DISK
    if(FileExists("/var/log/logd-fixture-expected") &&
       !WaitForFile("/var/log/logd-fixture-done", FIXTURE_WAIT_ATTEMPTS * 2u))
        FixtureSay("FIXTURE log writer fixture incomplete");
#endif

#if FEATURE_LOG_CAPTURE && FEATURE_LOG_DISK
    if(!FileExists("/dev/log-symlink-test"))
    {
        if(WaitForLogCompletion())
            FixtureSay("FIXTURE interleaved log verified");
        else
            FixtureSay("FIXTURE interleaved log invalid");
    }
#endif

#if FEATURE_EXEC_PROBES
    if(FileExists("/dev/log-symlink-test") && !WaitForConsoleCompletion())
        FixtureSay("FIXTURE console completion incomplete");
#endif

#if !FEATURE_EXEC_PROBES && FEATURE_WATCHDOG
    if(!WaitForWatchdogPets())
        FixtureSay("FIXTURE watchdog completion incomplete");
#endif

    DumpLog();
    FixtureSay("FIXTURE selftest done");

    SysKill(1, SIGUSR1);

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
