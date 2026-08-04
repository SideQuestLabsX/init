#include "fixture.h"

static bool ConsoleContains(const char *needle)
{
    isize fd = SysOpen("/dev/console", O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    usize matched = 0;
    usize needleLen = StrLen(needle);
    char buf[512];
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

static bool WaitForConsole(const char *needle)
{
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        if(ConsoleContains(needle))
            return true;
        FixtureSleep(100ull * NS_PER_MS);
    }
    return false;
}

static bool ReadCounter(const char *path, u64 *out)
{
    isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return false;

    char text[64];
    isize n = SysRead((i32)fd, text, sizeof(text) - 1);
    SysClose((i32)fd);
    if(n <= 0)
        return false;
    text[n] = '\0';

    const char *end = NULL;
    if(!ParseU64(text, out, &end) || (*end != '\n' && *end != '\0'))
        return false;
    return true;
}

static bool WaitForCounter(const char *path)
{
    u64 value = 0;
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        if(ReadCounter(path, &value))
            return true;
        FixtureSleep(100ull * NS_PER_MS);
    }
    return false;
}

static bool CounterStable(const char *path)
{
    u64 before = 0;
    u64 after = 0;
    if(!ReadCounter(path, &before))
        return false;
    FixtureSleep(500ull * NS_PER_MS);
    if(!ReadCounter(path, &after))
        return false;
    return before == after;
}

static bool CopyFile(const char *from, const char *to)
{
    isize src = SysOpen(from, O_RDONLY | O_CLOEXEC, 0);
    if(src < 0)
        return false;

    isize dst = SysOpen(to, O_WRONLY | O_TRUNC | O_CLOEXEC, 0);
    if(dst < 0)
    {
        SysClose((i32)src);
        return false;
    }

    u8 buffer[1024];
    bool bOk = true;
    for(;;)
    {
        isize n = SysRead((i32)src, buffer, sizeof(buffer));
        if(n == -EINTR)
            continue;
        if(n < 0)
        {
            bOk = false;
            break;
        }
        if(n == 0)
            break;

        usize off = 0;
        while(off < (usize)n)
        {
            isize written = SysWrite((i32)dst, buffer + off, (usize)n - off);
            if(written == -EINTR)
                continue;
            if(written <= 0)
            {
                bOk = false;
                break;
            }
            off += (usize)written;
        }
        if(!bOk)
            break;
    }

    SysClose((i32)src);
    SysClose((i32)dst);
    return bOk;
}

static NORETURN void DiscoveryFail(const char *reason)
{
    char line[128];
    Fmt(line, sizeof(line), "FIXTURE discovery failed: %s", reason);
    FixtureSay(line);
    SysExit(1);
}

void FixtureMain(void)
{
    if(!WaitForConsole("init: discovery_content: done"))
        DiscoveryFail("content task did not finish");
    if(!CopyFile("/tasks/boot/.discovery_content_v2",
                 "/tasks/boot/discovery_content"))
        DiscoveryFail("content rewrite failed");
    if(!WaitForConsole("FIXTURE discovery v2 discovery_content started"))
        DiscoveryFail("in-place content change did not start");

    if(!WaitForConsole("FIXTURE discovery v1 discovery_replace started"))
        DiscoveryFail("initial task did not start");
    if(SysRename("/tasks/always/.discovery_add", "/tasks/always/discovery_add") < 0)
        DiscoveryFail("add rename failed");
    if(!WaitForConsole("FIXTURE discovery v1 discovery_add started"))
        DiscoveryFail("added task did not start");

    if(!WaitForCounter("/dev/discovery-discovery_replace-v1"))
        DiscoveryFail("replacement heartbeat missing");
    if(SysRename("/tasks/always/.discovery_replace_v2",
                 "/tasks/always/discovery_replace") < 0)
        DiscoveryFail("replacement rename failed");
    if(!WaitForConsole("FIXTURE discovery v2 discovery_replace started"))
        DiscoveryFail("replacement did not start");
    if(!CounterStable("/dev/discovery-discovery_replace-v1"))
        DiscoveryFail("old replacement generation survived");

    if(!WaitForCounter("/dev/discovery-discovery_add-v1"))
        DiscoveryFail("added task heartbeat missing");
    if(SysUnlink("/tasks/always/discovery_add") < 0)
        DiscoveryFail("delete failed");
    if(!CounterStable("/dev/discovery-discovery_add-v1"))
        DiscoveryFail("deleted task survived");

    FixtureSay("FIXTURE discovery add removed");
    FixtureSay("FIXTURE discovery replacement complete");
    SysExit(0);
}
