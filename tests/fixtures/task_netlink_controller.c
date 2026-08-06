#include "fixture.h"

#define NLM_F_REQUEST 0x0001u
#define NLM_F_ACK     0x0004u
#define NLM_F_EXCL    0x0200u
#define NLM_F_CREATE  0x0400u

#define IFF_UP        0x0001u
#define IFA_LOCAL     2u
#define NLMSG_ERROR   2u

typedef struct
{
    u8 family;
    u8 pad;
    u16 type;
    i32 index;
    u32 flags;
    u32 change;
} KIfInfoMsg;

typedef struct
{
    u8 family;
    u8 prefixLen;
    u8 flags;
    u8 scope;
    u32 index;
} KIfAddrMsg;

typedef struct
{
    u16 length;
    u16 type;
} KRtAttr;

typedef struct
{
    KNlMsgHdr header;
    i32 error;
} KNlError;

static bool ReadAck(i32 fd)
{
    KNlError reply;
    isize n = SysRecvFrom(fd, &reply, sizeof(reply), 0, NULL, NULL);
    if(n < (isize)sizeof(reply) || reply.header.type != NLMSG_ERROR)
    {
        FixtureSay("FIXTURE netlink ack malformed");
        return false;
    }
    if(reply.error != 0)
    {
        char line[64];
        Fmt(line, sizeof(line), "FIXTURE netlink ack error %d", reply.error);
        FixtureSay(line);
        return false;
    }
    return true;
}

static bool SendLinkUp(i32 fd, const KSockAddrNl *kernel)
{
    struct
    {
        KNlMsgHdr header;
        KIfInfoMsg message;
    } request;
    memset(&request, 0, sizeof(request));
    request.header.length = (u32)sizeof(request);
    request.header.type = RTM_NEWLINK;
    request.header.flags = NLM_F_REQUEST | NLM_F_ACK;
    request.header.sequence = 1;
    request.message.index = 1;
    request.message.flags = IFF_UP;
    request.message.change = IFF_UP;
    return SysSendTo(fd, &request, sizeof(request), 0, kernel,
                     (u32)sizeof(*kernel)) == (isize)sizeof(request) &&
           ReadAck(fd);
}

static bool SendAddress(i32 fd, const KSockAddrNl *kernel)
{
    struct
    {
        KNlMsgHdr header;
        KIfAddrMsg message;
        KRtAttr attribute;
        u32 address;
    } request;
    memset(&request, 0, sizeof(request));
    request.header.length = (u32)sizeof(request);
    request.header.type = RTM_NEWADDR;
    request.header.flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    request.header.sequence = 2;
    request.message.family = AF_INET;
    request.message.prefixLen = 8;
    request.message.scope = 254;
    request.message.index = 1;
    request.attribute.length = (u16)(sizeof(KRtAttr) + sizeof(request.address));
    request.attribute.type = IFA_LOCAL;
    request.address = Hton32(0x7f000002u);
    return SysSendTo(fd, &request, sizeof(request), 0, kernel,
                     (u32)sizeof(*kernel)) == (isize)sizeof(request) &&
           ReadAck(fd);
}

void FixtureMain(void)
{
    isize fd = SysSocket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if(fd < 0)
    {
        FixtureSay("FIXTURE netlink socket failed");
        SysExit(1);
    }

    KSockAddrNl kernel;
    memset(&kernel, 0, sizeof(kernel));
    kernel.family = AF_NETLINK;

    bool bLink = SendLinkUp((i32)fd, &kernel);
    bool bAddress = SendAddress((i32)fd, &kernel);
    SysClose((i32)fd);
    if(!bLink || !bAddress)
    {
        FixtureSay("FIXTURE netlink request failed");
        SysExit(1);
    }

    FixtureSay("FIXTURE netlink link sent");
    FixtureSay("FIXTURE netlink address sent");
    FixtureSleep(2ull * NS_PER_SEC);
    SysExit(0);
}
