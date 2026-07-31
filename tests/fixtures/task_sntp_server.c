#define CFG_SNTP_PORT 40123
#include "fixture.h"

void FixtureMain(void)
{
    isize fd = SysSocket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    if(fd < 0)
    {
        FixtureSay("FIXTURE sntp socket failed");
        SysExit(1);
    }

    KSockAddrIn local;
    memset(&local, 0, sizeof(local));
    local.family = AF_INET;
    local.port = Hton16(CFG_SNTP_PORT);
    local.addr = Hton32(0x7f000001u);
    if(SysBind((i32)fd, &local, (u32)sizeof(local)) < 0)
    {
        FixtureSay("FIXTURE sntp bind failed");
        SysExit(1);
    }

    u8 request[SNTP_PKT_BYTES];
    KSockAddrIn peer;
    u32 peerLen = (u32)sizeof(peer);
    isize n = SysRecvFrom((i32)fd, request, sizeof(request), 0, &peer, &peerLen);
    if(n != SNTP_PKT_BYTES)
    {
        FixtureSay("FIXTURE sntp receive failed");
        SysExit(1);
    }

    u8 reply[SNTP_PKT_BYTES];
    memset(reply, 0, sizeof(reply));
    reply[0] = 0x24;
    reply[1] = 2;
    memcpy(&reply[24], &request[40], 8);
    Store64BE(&reply[40], SntpNtpFromUnixNs(SysRealNs()));
    if(SysSendTo((i32)fd, reply, sizeof(reply), 0, &peer, peerLen) != SNTP_PKT_BYTES)
    {
        FixtureSay("FIXTURE sntp reply failed");
        SysExit(1);
    }

    SysClose((i32)fd);
    FixtureSay("FIXTURE sntp reply sent");
    for(;;)
        FixtureSleep(NS_PER_SEC);
}
