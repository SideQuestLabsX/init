#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("MOUNT TABLE TASK STARTED");
    SysKill(1, SIGUSR1);
    SysExit(0);
}
