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

void FixtureMain(void)
{
    /* Wait past the first boot-test flush so earlier records survive the kill */
    FixtureSleep(1500ull * NS_PER_MS);
    i32 pid = FindLogWriter();
    if(pid < 0 || SysKill(pid, SIGTERM) < 0)
    {
        FixtureSay("FIXTURE log writer signal failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE log writer signaled");
    SysExit(0);
}
