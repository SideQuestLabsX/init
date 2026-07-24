/* Interval task body. Fires on CLOCK_BOOTTIME from /tasks/<interval>/. */

#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE tick fired");
    SysExit(0);
}
