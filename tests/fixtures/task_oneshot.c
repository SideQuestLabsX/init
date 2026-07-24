/* Runs once and exits cleanly. Must not be respawned. */

#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE oneshot ran");
    SysExit(0);
}
