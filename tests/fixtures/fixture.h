#ifndef INIT_FIXTURE_H
#define INIT_FIXTURE_H

#include "init.c"

/* libgcc's ARM division helper requires the libc raise ABI */
#if defined(__arm__)
int raise(int sig)
{
    UNUSED(sig);
    SysExit(70);
}
#endif

void FixtureMain(void);

static inline void FixtureSay(const char *msg)
{
    char line[256];
    usize n = Fmt(line, sizeof(line), "%s\n", msg);

    isize fd = SysOpen("/dev/console", O_WRONLY | O_APPEND | O_NOCTTY, 0);
    if(fd < 0)
    {
        /* Namespace harness inherits the console on stderr */
        SysWrite(2, line, n);
        return;
    }
    SysWrite((i32)fd, line, n);
    SysClose((i32)fd);
}

static inline void FixtureSleep(u64 ns)
{
    KTimeSpec ts;
    ts.sec = (i64)(ns / NS_PER_SEC);
    ts.nsec = (i64)(ns % NS_PER_SEC);
    SysPpoll(NULL, 0, &ts, NULL);
}

#endif
