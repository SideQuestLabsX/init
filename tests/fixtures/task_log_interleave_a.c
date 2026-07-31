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

static bool Touch(const char *path)
{
    isize fd = SysOpen(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if(fd < 0)
        return false;
    SysClose((i32)fd);
    return true;
}

void FixtureMain(void)
{
    char line[CFG_LINE_MAX];
    memset(line, '.', sizeof(line));
    memcpy(line, "LOGINTERLEAVE-A-BEGIN", sizeof("LOGINTERLEAVE-A-BEGIN") - 1);
    SysWrite(2, line, sizeof(line));

    if(!Touch("/dev/log-interleave-ready") ||
       !WaitForFile("/dev/log-interleave-done"))
    {
        FixtureSay("FIXTURE log interleave A failed");
        SysExit(1);
    }

    FixtureSleep(500ull * NS_PER_MS);
    SysWrite(2, "LOGINTERLEAVE-A-END\n", sizeof("LOGINTERLEAVE-A-END\n") - 1);
    if(!Touch("/dev/log-interleave-complete"))
    {
        FixtureSay("FIXTURE log interleave A failed");
        SysExit(1);
    }
    FixtureSay("FIXTURE log interleave A sent");

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
