/* Drives the boot test from inside the guest.
 *
 * Waits long enough for the other fixtures to have run, dumps the on-disk log
 * so the harness can see the capture -> ring -> forked writer -> disk path
 * really reached the filesystem, then asks PID 1 to shut down. */

#include "fixture.h"

#define SETTLE_NS (6ull * NS_PER_SEC)

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
    FixtureSleep(SETTLE_NS);

    DumpLog();
    FixtureSay("FIXTURE selftest done");

    SysKill(1, SIGUSR1);

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
