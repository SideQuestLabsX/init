/* Covers portable probe cancellation with a killable sleeper, not D-state I/O */
#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE hangprobe started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
