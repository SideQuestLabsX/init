#include "fixture.h"

#define STABLE_COUNT_PATH "/var/log/stable-count"

static u32 LoadCount(void)
{
    isize fd = SysOpen(STABLE_COUNT_PATH, O_RDONLY | O_CLOEXEC, 0);
    if(fd < 0)
        return 0;

    char value = 0;
    isize n = SysRead((i32)fd, &value, 1);
    SysClose((i32)fd);
    return n == 1 && value >= '0' && value <= '9' ? (u32)(value - '0') : 0;
}

static bool SaveCount(u32 count)
{
    isize fd = SysOpen(STABLE_COUNT_PATH,
                       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if(fd < 0)
        return false;

    char value = (char)('0' + count);
    bool bWritten = SysWrite((i32)fd, &value, 1) == 1;
    SysClose((i32)fd);
    return bWritten;
}

void FixtureMain(void)
{
    u32 count = LoadCount() + 1;
    if(!SaveCount(count))
    {
        FixtureSay("FIXTURE stable state failed");
        SysExit(126);
    }

    if(count <= 2)
    {
        FixtureSleep(250ull * NS_PER_MS);
        SysExit(3);
    }

    SysExit(0);
}
