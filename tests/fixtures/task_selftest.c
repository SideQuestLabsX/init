/* Dumps the persisted log before requesting shutdown */

#include "fixture.h"

#ifndef FIXTURE_SETTLE_NS
  /* Covers both log-writer replacements */
  #define FIXTURE_SETTLE_NS (12ull * NS_PER_SEC)
#endif

static bool FileExists(const char *path)
{
    isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;
    SysClose((i32)fd);
    return true;
}

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

#if !defined(FIXTURE_CAPTURE_DISABLED) && !defined(FIXTURE_LOG_DISK_DISABLED)
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
    FixtureSleep(FIXTURE_SETTLE_NS);

    if(FileExists("/var/log/logd-fixture-expected") &&
       !WaitForFile("/var/log/logd-fixture-done", 80))
        FixtureSay("FIXTURE log writer fixture incomplete");

#if !defined(FIXTURE_CAPTURE_DISABLED) && !defined(FIXTURE_LOG_DISK_DISABLED)
    if(!FileExists("/dev/log-symlink-test"))
    {
        if(WaitForFile("/dev/log-interleave-complete", 40) &&
           VerifyInterleavedLog())
            FixtureSay("FIXTURE interleaved log verified");
        else
            FixtureSay("FIXTURE interleaved log invalid");
    }
#endif

    DumpLog();
    FixtureSay("FIXTURE selftest done");

    SysKill(1, SIGUSR1);

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
