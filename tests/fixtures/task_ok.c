#include "fixture.h"

void FixtureMain(void)
{
    FixtureSay("FIXTURE ok started");

    SysWrite(1, "ok-stdout-line\n", 15);
    SysWrite(2, "ok-stderr-line\n", 15);

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
