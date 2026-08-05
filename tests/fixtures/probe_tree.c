#include "fixture.h"

static NORETURN void WorkerMain(void)
{
    for(;;)
    {
        FixtureSay("FIXTURE probe tree worker alive");
        FixtureSleep(100ull * NS_PER_MS);
    }
}

void FixtureMain(void)
{
    isize pid = SysFork();
    if(pid == 0)
        WorkerMain();
    if(pid < 0)
    {
        FixtureSay("FIXTURE probe tree fork failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE probe tree started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
