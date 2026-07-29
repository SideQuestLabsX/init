#include "fixture.h"

static bool IsStatusSpace(char c)
{
    return c == ' ' || c == '\t';
}

static void ReadTaskName(char *out, usize cap)
{
    isize fd = SysOpen("/proc/self/cmdline", O_RDONLY, 0);
    if(fd < 0)
    {
        StrCopy(out, cap, "unknown");
        return;
    }

    char cmdline[CFG_PATH_MAX];
    isize n = SysRead((i32)fd, cmdline, sizeof(cmdline));
    SysClose((i32)fd);
    if(n <= 0)
    {
        StrCopy(out, cap, "unknown");
        return;
    }

    usize end = 0;
    usize start = 0;
    while(end < (usize)n && cmdline[end] != '\0')
    {
        if(cmdline[end] == '/')
            start = end + 1;
        end++;
    }
    StrCopyN(out, cap, cmdline + start, end - start);
}

static void EmitStatusField(const char *task, const char *status, usize len,
                            const char *field)
{
    usize fieldLen = StrLen(field);

    for(usize lineStart = 0; lineStart < len;)
    {
        usize lineEnd = lineStart;
        while(lineEnd < len && status[lineEnd] != '\n')
            lineEnd++;

        if(lineEnd > lineStart + fieldLen &&
           StrNCmp(status + lineStart, field, fieldLen) == 0 &&
           status[lineStart + fieldLen] == ':')
        {
            char line[256];
            usize n = Fmt(line, sizeof(line), "FIXTURE %s status %s", task, field);
            usize pos = lineStart + fieldLen + 1;
            bool bHasValue = false;

            while(pos < lineEnd)
            {
                while(pos < lineEnd && IsStatusSpace(status[pos]))
                    pos++;
                if(pos == lineEnd)
                    break;

                usize tokenStart = pos;
                while(pos < lineEnd && !IsStatusSpace(status[pos]))
                    pos++;

                if(n + 1 < sizeof(line))
                {
                    line[n++] = ' ';
                    line[n] = '\0';
                    n += StrCopyN(line + n, sizeof(line) - n,
                                  status + tokenStart, pos - tokenStart);
                }
                bHasValue = true;
            }

            if(!bHasValue)
                StrCat(line, sizeof(line), " none");
            FixtureSay(line);
            return;
        }

        lineStart = lineEnd + 1;
    }
}

void FixtureMain(void)
{
    char task[CFG_NAME_MAX];
    ReadTaskName(task, sizeof(task));

    isize fd = SysOpen("/proc/self/status", O_RDONLY, 0);
    if(fd < 0)
    {
        FixtureSay("FIXTURE status unavailable");
        SysExit(1);
    }

    char status[4096];
    isize n = SysRead((i32)fd, status, sizeof(status));
    SysClose((i32)fd);
    if(n <= 0)
    {
        FixtureSay("FIXTURE status unreadable");
        SysExit(1);
    }

    EmitStatusField(task, status, (usize)n, "Uid");
    EmitStatusField(task, status, (usize)n, "Gid");
    EmitStatusField(task, status, (usize)n, "Groups");
    EmitStatusField(task, status, (usize)n, "CapInh");
    EmitStatusField(task, status, (usize)n, "CapPrm");
    EmitStatusField(task, status, (usize)n, "CapEff");
    EmitStatusField(task, status, (usize)n, "CapBnd");
    EmitStatusField(task, status, (usize)n, "CapAmb");
    EmitStatusField(task, status, (usize)n, "NoNewPrivs");
    SysExit(0);
}
