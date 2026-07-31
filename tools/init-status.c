#include "init.c"

#define STATUS_READ_ATTEMPTS 1024u

static StatusSnapshot G_SNAPSHOT;

/* libgcc's ARM division helper requires the libc raise ABI */
#if defined(__arm__)
int raise(int sig)
{
    UNUSED(sig);
    SysExit(70);
}
#endif

static bool MmapFailed(const void *p)
{
    isize value = (isize)(uintptr_t)p;
    return value < 0 && value > -4096;
}

static bool WriteAll(i32 fd, const char *text, usize len)
{
    usize off = 0;
    while(off < len)
    {
        isize n = SysWrite(fd, text + off, len - off);
        if(n == -EINTR)
            continue;
        if(n <= 0)
            return false;
        off += (usize)n;
    }
    return true;
}

static void WriteLine(const char *text)
{
    WriteAll(1, text, StrLen(text));
}

static const char *StateName(u32 state)
{
    switch(state)
    {
    case TS_PENDING: return "pending";
    case TS_RUNNING: return "running";
    case TS_BACKOFF: return "backoff";
    case TS_IDLE: return "idle";
    case TS_DONE: return "done";
    case TS_FAILED: return "failed";
    default: return "unknown";
    }
}

static bool ReadSnapshot(const StatusBlock *status, StatusSeq *sequenceOut)
{
    for(u32 attempt = 0; attempt < STATUS_READ_ATTEMPTS; attempt++)
    {
        if(StatusRead(status, &G_SNAPSHOT, sequenceOut))
            return true;
        SysSchedYield();
    }
    return false;
}

static bool NameValid(const char name[CFG_NAME_MAX])
{
    for(usize i = 0; i < CFG_NAME_MAX; i++)
    {
        if(name[i] == '\0')
            return true;
    }
    return false;
}

NORETURN void StatusReaderMain(void)
{
    isize fd = SysOpen(STATUS_PATH, O_RDONLY | O_CLOEXEC | O_NOFOLLOW, 0);
    if(fd < 0 || SysLseek((i32)fd, 0, SEEK_END) != (isize)sizeof(StatusBlock))
    {
        if(fd >= 0)
            SysClose((i32)fd);
        WriteLine("STATUS error opening snapshot\n");
        SysExit(1);
    }

    void *mapped = SysMmap(NULL, sizeof(StatusBlock), PROT_READ, MAP_SHARED,
                           (i32)fd, 0);
    SysClose((i32)fd);
    if(MmapFailed(mapped))
    {
        WriteLine("STATUS error mapping snapshot\n");
        SysExit(1);
    }

    StatusSeq sequence = 0;
    if(!ReadSnapshot((const StatusBlock *)mapped, &sequence) ||
       G_SNAPSHOT.magic != STATUS_MAGIC || G_SNAPSHOT.version != STATUS_VERSION ||
       G_SNAPSHOT.entrySize != sizeof(StatusEntry) || G_SNAPSHOT.count > CFG_MAX_TASKS)
    {
        WriteLine("STATUS error reading snapshot\n");
        SysExit(1);
    }

    char line[256];
    usize n = Fmt(line, sizeof(line), "STATUS snapshot seq=%zu tasks=%u\n",
                  sequence, G_SNAPSHOT.count);
    WriteAll(1, line, n);

    for(u32 i = 0; i < G_SNAPSHOT.count; i++)
    {
        const StatusEntry *entry = &G_SNAPSHOT.task[i];
        if(!NameValid(entry->name))
        {
            WriteLine("STATUS error invalid task name\n");
            SysExit(1);
        }
        n = Fmt(line, sizeof(line),
                "STATUS task=%s state=%s pid=%d runs=%u failures=%u exit=%d signal=%d probe=%d probe_failures=%u\n",
                entry->name, StateName(entry->state), entry->pid, entry->runs,
                entry->consecFails, entry->lastExit, entry->lastSignal,
                entry->lastProbeRc, entry->probeFails);
        WriteAll(1, line, n);
    }

    n = Fmt(line, sizeof(line), "STATUS snapshot end seq=%zu\n", sequence);
    WriteAll(1, line, n);
    SysExit(0);
}
