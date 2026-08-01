#include "fixture.h"

static i32 FindLogWriter(void)
{
    isize proc = SysOpen("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
    if(proc < 0)
        return -1;

    u8 buf[4096] __attribute__((aligned(8)));
    for(;;)
    {
        isize n = SysGetdents64((i32)proc, buf, sizeof(buf));
        if(n <= 0)
            break;

        usize off = 0;
        while(off + sizeof(KDirent64) <= (usize)n)
        {
            const KDirent64 *d = (const KDirent64 *)(buf + off);
            if(d->reclen == 0 || off + d->reclen > (usize)n)
                break;
            off += d->reclen;

            u64 pid;
            const char *end;
            if(!ParseU64(d->name, &pid, &end) || *end != '\0' ||
               pid > 0x7fffffffull)
                continue;

            char path[64];
            Fmt(path, sizeof(path), "/proc/%s/comm", d->name);
            isize fd = SysOpen(path, O_RDONLY, 0);
            if(fd < 0)
                continue;

            char comm[32];
            isize commLen = SysRead((i32)fd, comm, sizeof(comm));
            SysClose((i32)fd);
            if(commLen >= 9 && StrNCmp(comm, "init-logd", 9) == 0)
            {
                SysClose((i32)proc);
                return (i32)pid;
            }
        }
    }

    SysClose((i32)proc);
    return -1;
}

static i32 WaitForOtherWriter(i32 excludePid, usize attempts)
{
    for(usize i = 0; i < attempts; i++)
    {
        i32 pid = FindLogWriter();
        if(pid > 0 && pid != excludePid)
            return pid;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return -1;
}

static bool Touch(const char *path)
{
    isize fd = SysOpen(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if(fd < 0)
        return false;
    SysClose((i32)fd);
    return true;
}

#ifndef FIXTURE_CAPTURE_DISABLED
static bool LogContains(const char *needle)
{
    isize fd = SysOpen("/var/log/init.log", O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    usize needleLen = StrLen(needle);
    usize matched = 0;
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

static bool LogMarkersReady(bool bWriterTransitions)
{
    if(!LogContains("ok-stderr-line") ||
       !LogContains("flap-dying") ||
       !LogContains("partial-stdout-line") ||
       !LogContains("LOGEDGE-HEAD") ||
       !LogContains("LOGEDGE-B1") ||
       !LogContains("LOGEDGE-B2") ||
       !LogContains("LOGINTERLEAVE-A-BEGIN") ||
       !LogContains("LOGINTERLEAVE-A-END") ||
       !LogContains("LOGINTERLEAVE-B"))
        return false;
    if(!bWriterTransitions)
        return true;
#if FEATURE_EXEC_PROBES
    if(!LogContains("probefail: FAILED after"))
        return false;
#endif
    return true;
}

static bool WaitForLogMarkers(bool bWriterTransitions)
{
    for(usize attempt = 0; attempt < FIXTURE_WAIT_ATTEMPTS; attempt++)
    {
        if(LogMarkersReady(bWriterTransitions))
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}

static bool WaitForPartialLine(void)
{
    for(usize attempt = 0; attempt < 12; attempt++)
    {
        if(LogContains("partial-stdout-line"))
            return true;
        FixtureSleep(250ull * NS_PER_MS);
    }
    return false;
}
#endif

void FixtureMain(void)
{
    SysWrite(1, "partial-stdout-line", sizeof("partial-stdout-line") - 1);

    /* Keep the line partial across multiple capture reads */
    FixtureSleep(1500ull * NS_PER_MS);
    SysWrite(1, "\n", 1);

#ifdef FIXTURE_CAPTURE_DISABLED
    FixtureSleep(750ull * NS_PER_MS);
#else
    if(!WaitForPartialLine())
    {
        FixtureSay("FIXTURE partial line was not persisted");
        SysExit(1);
    }
#endif

#ifndef FIXTURE_CAPTURE_DISABLED
    if(!WaitForLogMarkers(false))
    {
        FixtureSay("FIXTURE initial log markers missing");
        SysExit(1);
    }
#endif

    i32 pid = FindLogWriter();
    if(pid < 0 || SysKill(pid, SIGTERM) < 0)
    {
        FixtureSay("FIXTURE log writer signal failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE log writer signaled");

    i32 respawned = WaitForOtherWriter(pid, 20);
    if(respawned < 0)
    {
        FixtureSay("FIXTURE respawned log writer not found");
        SysExit(1);
    }

    FixtureSleep(750ull * NS_PER_MS);

    /* Exercise stalled live-writer replacement */
    if(SysKill(respawned, SIGSTOP) < 0)
    {
        FixtureSay("FIXTURE log writer stop failed");
        SysExit(1);
    }
    FixtureSay("FIXTURE log writer stopped");

    i32 replaced = WaitForOtherWriter(respawned, 40);
    if(replaced < 0)
    {
        FixtureSay("FIXTURE stalled log writer was not replaced");
        SysExit(1);
    }
    FixtureSay("FIXTURE stalled log writer replaced");

#ifndef FIXTURE_CAPTURE_DISABLED
    if(!WaitForLogMarkers(true))
    {
        FixtureSay("FIXTURE final log markers missing");
        SysExit(1);
    }
#endif

    if(!Touch("/var/log/logd-fixture-done"))
    {
        FixtureSay("FIXTURE log writer completion failed");
        SysExit(1);
    }

    /* Keep the pipe open so a blocking drain cannot reach the shutdown path */
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
