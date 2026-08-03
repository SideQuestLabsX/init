#include "fixture.h"

static void ReadTaskName(char *out, usize cap)
{
    isize fd = SysOpen("/proc/self/cmdline", O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
    {
        StrCopy(out, cap, "unknown");
        return;
    }

    char cmdline[CFG_PATH_MAX];
    isize n = SysRead((i32)fd, cmdline, sizeof(cmdline));
    SysClose((i32)fd);
    if(n <= 0)
    {
        StrCopy(out, cap, "unknown");
        return;
    }

    usize start = 0;
    for(usize i = 0; i < (usize)n; i++)
    {
        if(cmdline[i] == '/')
            start = i + 1;
        if(cmdline[i] == '\0')
            break;
    }
    StrCopyN(out, cap, cmdline + start, (usize)n - start);
}

static void WriteHeartbeat(const char *task, u64 count)
{
    char path[CFG_PATH_MAX];
    char text[64];
    Fmt(path, sizeof(path), "/dev/discovery-%s-v1", task);
    usize n = Fmt(text, sizeof(text), "%llu\n", count);
    isize fd = SysOpen(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if(fd < 0)
        return;
    SysWrite((i32)fd, text, n);
    SysClose((i32)fd);
}

void FixtureMain(void)
{
    char task[CFG_NAME_MAX];
    ReadTaskName(task, sizeof(task));

    char marker[128];
    Fmt(marker, sizeof(marker), "FIXTURE discovery v1 %s started", task);
    FixtureSay(marker);

    u64 count = 0;
    for(;;)
    {
        WriteHeartbeat(task, count++);
        FixtureSleep(100ull * NS_PER_MS);
    }
}
