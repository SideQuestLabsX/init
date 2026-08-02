#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("WATCHDOG task started");

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
