#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE midprobe task started");
    FixtureSleep(250ull * NS_PER_MS);
    SysExit(4);
}
