/* Crashes immediately, every time. Exercises exponential backoff and the
 * transition to FAILED after maxRestarts. */

#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE flap crashing");
    SysWrite(2, "flap-dying\n", 11);
    SysExit(3);
}
