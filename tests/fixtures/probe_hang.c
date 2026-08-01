/* Tests timeout accounting with a killable sleeper, not uninterruptible I/O */
#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE hangprobe started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
