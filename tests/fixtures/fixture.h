#ifndef INIT_FIXTURE_H
#define INIT_FIXTURE_H

/* INIT_FIXTURE cuts init.c down to the syscall wrappers and string helpers, so
 * the fixtures exercise the same syscall layer init does. */
#include "init.c"

/* Helpers for the fixture tasks the boot tests supervise.
 *
 * They write to /dev/console rather than stdout, because stdout goes into the
 * shared ring by policy. The console is the out-of-band channel the harness
 * reads. */

void FixtureMain(void);

static inline void FixtureSay(const char *msg)
{
    char line[256];
    usize n = Fmt(line, sizeof(line), "%s\n", msg);

    isize fd = SysOpen("/dev/console", O_WRONLY | O_APPEND | O_NOCTTY, 0);
    if(fd < 0)
    {
        /* no devtmpfs, so inherited stderr is the console (namespace harness) */
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
