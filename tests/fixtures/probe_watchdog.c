#include "fixture.h"

void FixtureMain(void)
{
    isize fd = SysOpen("/var/log/watchdog-probe-state",
                       O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if(fd < 0)
        SysExit(1);

    char state = '0';
    if(SysRead((i32)fd, &state, 1) != 1)
        state = '0';

    if(state < '3')
    {
        state++;
        SysLseek((i32)fd, 0, SEEK_SET);
        SysWrite((i32)fd, &state, 1);
        SysClose((i32)fd);
        SysExit(0);
    }

    SysClose((i32)fd);
    SysExit(1);
}
