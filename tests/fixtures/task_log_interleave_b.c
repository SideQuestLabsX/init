#include "fixture.h"

static bool WaitForFile(const char *path)
{
    for(usize i = 0; i < 200; i++)
    {
        isize fd = SysOpen(path, O_RDONLY | O_CLOEXEC, 0);
        if(fd >= 0)
        {
            SysClose((i32)fd);
            return true;
        }
        FixtureSleep(25ull * NS_PER_MS);
    }
    return false;
}

void FixtureMain(void)
{
    if(!WaitForFile("/dev/log-interleave-ready"))
    {
        FixtureSay("FIXTURE log interleave B failed");
        SysExit(1);
    }

    SysWrite(2, "LOGINTERLEAVE-B\n", sizeof("LOGINTERLEAVE-B\n") - 1);
    isize fd = SysOpen("/dev/log-interleave-done",
                       O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if(fd < 0)
    {
        FixtureSay("FIXTURE log interleave B failed");
        SysExit(1);
    }
    SysClose((i32)fd);
    FixtureSay("FIXTURE log interleave B sent");

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
