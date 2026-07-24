/* A check that never exits. Stands in for one wedged on dead I/O, which is the
 * case init cannot resolve: SIGKILL does not lift a process out of
 * uninterruptible sleep. This one is killable, so it exercises the accounting
 * rather than the wedge itself. */

#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE hangprobe started");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
