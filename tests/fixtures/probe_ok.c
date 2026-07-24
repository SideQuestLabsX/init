/* A passing exec probe. init has no idea what this checks, which is the point
 * of pull-based liveness. */

#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE probe ran");
    SysExit(0);
}
