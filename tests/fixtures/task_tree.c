#include "fixture.h"

#define SIG_IGN_HANDLER 1u

static NORETURN void WorkerMain(void)
{
    KSigAction sa = { 0 };
    sa.handler = SIG_IGN_HANDLER;
    if(SysSigAction(SIGTERM, &sa, NULL) < 0)
        SysExit(1);

    for(;;)
    {
        FixtureSay("FIXTURE tree worker alive");
        FixtureSleep(100ull * NS_PER_MS);
    }
}

void FixtureMain(void)
{
    isize state = SysOpen("/tree.started", O_WRONLY | O_CREAT | O_EXCL, 0644);
    if(state == -EEXIST)
    {
        FixtureSay("FIXTURE tree replacement started");
        for(;;)
            FixtureSleep(NS_PER_SEC);
    }
    if(state < 0)
    {
        FixtureSay("FIXTURE tree state failed");
        SysExit(1);
    }
    SysClose((i32)state);

    isize pid = SysFork();
    if(pid == 0)
        WorkerMain();
    if(pid < 0)
    {
        FixtureSay("FIXTURE tree fork failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE tree leader started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
