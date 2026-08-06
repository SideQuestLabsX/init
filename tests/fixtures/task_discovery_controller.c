#define INIT_STATUS_READER 1
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

static bool StatusHasTombstones(void)
{
    isize fd = SysOpen(STATUS_PATH, O_RDONLY | O_CLOEXEC | O_NOFOLLOW, 0);
    if(fd < 0 || SysLseek((i32)fd, 0, SEEK_END) != (isize)sizeof(StatusBlock))
    {
        if(fd >= 0)
            SysClose((i32)fd);
        return false;
    }

    void *mapped = SysMmap(NULL, sizeof(StatusBlock), PROT_READ, MAP_SHARED,
                           (i32)fd, 0);
    SysClose((i32)fd);
    isize mappedValue = (isize)(uintptr_t)mapped;
    if(mappedValue < 0 && mappedValue > -4096)
        return false;

    StatusSnapshot snapshot;
    StatusSeq sequence;
    bool bRead = StatusRead((const StatusBlock *)mapped, &snapshot, &sequence);
    usize removedIndex = CFG_MAX_TASKS;
    usize newIndex = CFG_MAX_TASKS;
    if(bRead && snapshot.magic == STATUS_MAGIC &&
       snapshot.version == STATUS_VERSION && snapshot.count <= CFG_MAX_TASKS)
    {
        for(u32 i = 0; i < snapshot.count; i++)
        {
            const StatusEntry *entry = &snapshot.task[i];
            if(StrEq(entry->name, "discovery_add") && entry->state == TS_REMOVED)
                removedIndex = i;
            if(StrEq(entry->name, "discovery_new"))
                newIndex = i;
        }
    }

    SysMunmap(mapped, sizeof(StatusBlock));
    return removedIndex != CFG_MAX_TASKS && newIndex != CFG_MAX_TASKS &&
           removedIndex != newIndex;
}

static bool WaitForStatusTombstones(void)
{
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        if(StatusHasTombstones())
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
    for(usize i = 0; i < FIXTURE_WAIT_ATTEMPTS; i++)
    {
        u64 before = 0;
        u64 after = 0;
        if(!ReadCounter(path, &before))
            return false;
        FixtureSleep(500ull * NS_PER_MS);
        if(!ReadCounter(path, &after))
            return false;
        if(before == after)
            return true;
    }
    return false;
}

static bool CopyFile(const char *from, const char *to, i32 mode)
{
    isize src = SysOpen(from, O_RDONLY | O_CLOEXEC, 0);
    if(src < 0)
        return false;

    isize dst = SysOpen(to, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
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

static bool LimitPath(char *path, usize cap, u32 index)
{
    char tens = (char)('0' + index / 10u);
    char ones = (char)('0' + index % 10u);
    return Fmt(path, cap, "/tasks/boot/zz-limit-%c%c", tens, ones) != 0;
}

static bool CreateLimitFiles(void)
{
    for(u32 i = 0; i < 50; i++)
    {
        char path[CFG_PATH_MAX];
        if(!LimitPath(path, sizeof(path), i) ||
           !CopyFile("/tasks/boot/oneshot", path, 0644))
            return false;
    }
    return true;
}

static bool RemoveLimitFiles(void)
{
    for(u32 i = 0; i < 50; i++)
    {
        char path[CFG_PATH_MAX];
        if(!LimitPath(path, sizeof(path), i) || SysUnlink(path) < 0)
            return false;
    }
    return true;
}

#define INOTIFY_FLOOD_WORKERS 1u
#define INOTIFY_FLOOD_ROUNDS 6000u

static bool Touch(const char *path);

static NORETURN void FloodWorker(u32 worker)
{
    for(u32 i = 0; i < INOTIFY_FLOOD_ROUNDS; i++)
    {
        char path[CFG_PATH_MAX];
        Fmt(path, sizeof(path), "/tasks/.overflow-%u-%u", worker, i);
        isize fd = SysOpen(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if(fd < 0)
        {
            SysExit(1);
        }
        SysClose((i32)fd);
        if(SysUnlink(path) < 0)
            SysExit(1);
    }
    SysExit(0);
}

static bool InotifyOverflowed(i32 fd)
{
    u8 buffer[4096] __attribute__((aligned(8)));
    for(;;)
    {
        isize n = SysRead(fd, buffer, sizeof(buffer));
        if(n == -EAGAIN)
            return false;
        if(n == -EINTR)
            continue;
        if(n <= 0)
            return false;

        usize off = 0;
        while(off < (usize)n)
        {
            usize size = 0;
            const KInotifyEvent *event = (const KInotifyEvent *)(buffer + off);
            if(!TaskWatchEventValid(event, (usize)n - off, &size))
                return false;
            if((event->mask & IN_Q_OVERFLOW) != 0)
                return true;
            off += size;
        }
    }
}

static bool FloodInotify(void)
{
    isize watchFd = SysInotifyInit1(O_NONBLOCK | O_CLOEXEC);
    if(watchFd < 0)
        return false;
    isize watch = SysInotifyAddWatch((i32)watchFd, "/tasks",
                                     IN_CREATE | IN_DELETE | IN_MOVED_FROM |
                                     IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB |
                                     IN_ONLYDIR);
    if(watch < 0)
    {
        SysClose((i32)watchFd);
        return false;
    }

    i32 pids[INOTIFY_FLOOD_WORKERS] = { 0 };
    usize spawned = 0;
    bool bOk = true;
    for(u32 i = 0; i < INOTIFY_FLOOD_WORKERS; i++)
    {
        isize pid = SysFork();
        if(pid < 0)
        {
            bOk = false;
            break;
        }
        if(pid == 0)
            FloodWorker(i);
        pids[spawned++] = (i32)pid;
    }

    for(usize i = 0; i < spawned; i++)
    {
        i32 status = 0;
        isize pid;
        do
        {
            pid = SysWait4(pids[i], &status, 0);
        } while(pid == -EINTR);
        if(pid != pids[i] || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            bOk = false;
    }
    bool bOverflowed = InotifyOverflowed((i32)watchFd);
    SysClose((i32)watchFd);
    return bOk && spawned == INOTIFY_FLOOD_WORKERS && bOverflowed;
}

static bool Touch(const char *path)
{
    isize fd = SysOpen(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if(fd < 0)
        return false;
    SysClose((i32)fd);
    return true;
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
    if(!CopyFile("/tasks/boot/.discovery_content_v1",
                 "/tasks/boot/discovery_content", 0755))
        DiscoveryFail("intermediate content rewrite failed");
    FixtureSleep(CFG_TASK_DISCOVERY_GRACE_NS / 2);
    if(!CopyFile("/tasks/boot/.discovery_content_v2",
                 "/tasks/boot/discovery_content", 0755))
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
    if(SysRename("/tasks/always/.discovery_new",
                 "/tasks/always/discovery_new") < 0)
        DiscoveryFail("tombstone task rename failed");
    if(!WaitForConsole("FIXTURE discovery v1 discovery_new started"))
        DiscoveryFail("tombstone slot task did not start");
    if(!WaitForStatusTombstones())
        DiscoveryFail("tombstone status snapshot missing");
    FixtureSay("FIXTURE discovery tombstones verified");

    if(!CreateLimitFiles())
        DiscoveryFail("directory limit setup failed");
    if(!WaitForConsole("init: zz-limit-00: not executable, skipped"))
        DiscoveryFail("directory limit scan did not run");
    FixtureSleep(500ull * NS_PER_MS);
    if(ConsoleContains("init: zz-limit-49: not executable, skipped"))
        DiscoveryFail("directory scan kept an oversized entry");
    if(!RemoveLimitFiles())
        DiscoveryFail("directory limit cleanup failed");
    FixtureSay("FIXTURE discovery directory bound verified");

    if(SysRename("/tasks", "/tasks-away") < 0)
        DiscoveryFail("task root rename failed");
    if(!WaitForConsole("task scan: cannot read /tasks, preserving current tasks"))
        DiscoveryFail("incomplete scan was not retried");
    if(!CounterStable("/dev/discovery-discovery_replace-v1"))
        DiscoveryFail("incomplete scan changed a live task");
    if(SysRename("/tasks-away", "/tasks") < 0)
        DiscoveryFail("task root restore failed");
    if(!CopyFile("/tasks/boot/.discovery_content_v1",
                 "/tasks/boot/discovery_retry", 0755))
        DiscoveryFail("retry task setup failed");
    if(!WaitForConsole("FIXTURE discovery v1 discovery_retry started"))
        DiscoveryFail("incomplete scan did not recover");
    FixtureSay("FIXTURE discovery scan retry verified");
    SysUnlink("/tasks/boot/discovery_retry");

    bool bFlooded = FloodInotify();
    if(!bFlooded)
        DiscoveryFail("inotify overflow flood failed");
    FixtureSay("FIXTURE discovery overflow verified");

    if(!Touch("/dev/discovery-complete"))
        DiscoveryFail("discovery completion marker failed");
    FixtureSay("FIXTURE discovery replacement complete");
    SysExit(0);
}
