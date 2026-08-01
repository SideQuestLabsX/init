#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE flap crashing");
    SysWrite(2, "flap-dying\n", 11);
    SysExit(3);
}
