#include "fixture.h"

#define SIG_IGN_HANDLER 1u

void FixtureMain(void)
{
    KSigAction sa = { 0 };
    sa.handler = SIG_IGN_HANDLER;
    if(SysSigAction(SIGTERM, &sa, NULL) < 0)
    {
        FixtureSay("FIXTURE ignoreterm setup failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE ignoreterm started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
