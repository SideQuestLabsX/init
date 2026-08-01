/* Cross both capture chunk boundaries */

#include "fixture.h"

#define EDGE_LINE_LEN (CFG_LINE_MAX * 2)

static char g_line[EDGE_LINE_LEN + 1];

static void PlaceMarkerEnd(usize endIndex, const char *marker)
{
    usize len = StrLen(marker);
    memcpy(g_line + endIndex - len + 1, marker, len);
}

void FixtureMain(void)
{
    memset(g_line, '.', EDGE_LINE_LEN);
    memcpy(g_line, "LOGEDGE-HEAD", 12);

    PlaceMarkerEnd(CFG_LINE_MAX - 1, "LOGEDGE-B1");
    PlaceMarkerEnd(EDGE_LINE_LEN - 1, "LOGEDGE-B2");

    g_line[EDGE_LINE_LEN] = '\n';

    SysWrite(2, g_line, EDGE_LINE_LEN + 1);
    FixtureSay("FIXTURE log edge line sent");

    for(;;)
        FixtureSleep(NS_PER_SEC);
}
