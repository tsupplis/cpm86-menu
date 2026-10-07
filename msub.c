/*
 * msub.c -- MSUB, a SUBMIT replacement built on sub.lib.
 * Licensed under the MIT license. See LICENSE file in the project root.
 *
 * MSUB [/N] file[.SUB] [parm1 parm2 ...]
 *
 * /N: check only. Runs the same load and checks, lists the expanded
 * commands and reports what a real run would push, but writes nothing and
 * does not start the CCP.
 *
 * Same .sub reader as MENU (sub_load). Unlike DR SUBMIT, when MSUB is run
 * from inside a running $$$.SUB job it pushes the new commands on top of
 * what is left of that job instead of replacing it, so jobs nest.
 */
#include "conio.h"
#include "sub.h"

static void putnum(n)
    int n;
{
    char d[6];
    int  i;

    i = 0;
    do {
        d[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0 && i < 5);
    while (i > 0)
        cputc(d[--i]);
}

#define SUB_MAX_RECS 128    /* the CCP reads $$$.SUB from extent 0 only */

static void putline(s)
    char *s;
{
    for (; *s != '\0'; s++) {
        if (*s < ' ') {                 /* ^A..^Z made by ^x in the file */
            cputc('^');
            cputc((char)(*s + '@'));
        } else {
            cputc(*s);
        }
    }
}

/* 1 if the line makes the CCP change its own drive or user: "B:" / "USER n".
   The CCP then looks for $$$.SUB there and the rest of the stack is lost. */
static int changes_drive_user(s)
    char *s;
{
    if (s[0] >= 'A' && s[0] <= 'P' && s[1] == ':' && s[2] == '\0')
        return 1;
    if (s[0] == 'U' && s[1] == 'S' && s[2] == 'E' && s[3] == 'R' &&
        (s[4] == '\0' || s[4] == ' '))
        return 1;
    return 0;
}

/* /N: report only */
static int check(n)
    int n;
{
    int i;
    int active;
    int existing;
    int bad;

    bad = 0;
    cputs("Checking ");
    cputs(sub_name());
    cputs("\r\n\r\n");

    for (i = 0; i < n; i++) {
        cputs("  line ");
        putnum(sub_lineno(i));
        cputs(": ");
        putline(sub_line(i));
        cputs("\r\n");
    }
    cputs("\r\n");

    for (i = 0; i < n; i++) {
        if (changes_drive_user(sub_line(i))) {
            cputs("WARNING: line ");
            putnum(sub_lineno(i));
            cputs(": drive/user change ends the $$$.SUB chain,\r\n"
                  "         the commands after it will not run. Use B:PROG instead.\r\n");
        }
    }

    active = sub_active();
    if (active < 0) {
        cputs("WARNING: CCP not recognised, $$$.SUB would not be run.\r\n");
    }
    existing = (active == 1) ? sub_records() : 0;
    if (existing + n > SUB_MAX_RECS) {
        cputs("ERROR: $$$.SUB would hold ");
        putnum(existing + n);
        cputs(" records, the CCP reads at most ");
        putnum(SUB_MAX_RECS);
        cputs(".\r\n");
        bad = 1;
    }

    if (!bad) {
        cputs("OK: ");
        putnum(n);
        cputs(" commands");
        if (existing > 0) {
            cputs(" on top of ");
            putnum(existing);
            cputs(" already in $$$.SUB");
        }
        cputs(" = ");
        putnum(existing + n);
        cputs(" of ");
        putnum(SUB_MAX_RECS);
        cputs(" records. Nothing written.\r\n");
    }
    return bad;
}

int main(argc, argv)
    int argc;
    char **argv;
{
    char  cmd[128];
    char *p;
    int   i;
    int   j;
    int   n;
    int   opened;
    int   first;
    int   checkonly;

    cputs("\r\nMSUB SUB ORCHESTRATOR VER 1.0\r\n\r\n");

    if ((bdos(12, 0) & 0xFF) != 0x22) {
        cputs("CP/M-86 1.1 required\r\n");
        return 1;
    }

    /* /N as first argument: check only */
    checkonly = 0;
    first = 1;
    if (argc > 1 && argv[1][0] == '/' &&
        (argv[1][1] == 'N' || argv[1][1] == 'n') && argv[1][2] == '\0') {
        checkonly = 1;
        first = 2;
    }

    if (argc <= first) {
        cputs("USAGE: MSUB [/N] file[.SUB] [parm1 parm2 ...]\r\n"
              "  /N  check only: expand and validate the file, write nothing\r\n");
        return 0;
    }

    /* rebuild "file parm1 parm2 ..." from the command tail */
    j = 0;
    for (i = first; i < argc; i++) {
        if (i > first && j < 127)
            cmd[j++] = ' ';
        for (p = argv[i]; *p != '\0' && j < 127; p++)
            cmd[j++] = *p;
    }
    cmd[j] = '\0';

    n = sub_load(cmd);
    if (n <= 0) {
        cputs("ERROR: ");
        cputs(sub_errmsg(n));
        cputs(sub_name());
        if (sub_errline() > 0) {
            cputs(", line ");
            putnum(sub_errline());
        }
        cputs("\r\n");
        return 1;
    }

    if (checkonly)
        return check(n);

    /* inside a running job: push on top of it; else start a fresh stack */
    opened = (sub_open(sub_active() == 1 ? SUB_APPEND : SUB_CREATE) == 0);
    if (!opened || sub_pushall() != 0 || sub_close() != 0) {
        if (opened)
            sub_abort();
        cputs("ERROR: Cannot write $$$.SUB\r\n");
        return 1;
    }

    sub_exit();     /* set the CCP submit flag, warm boot: no return */
    return 0;
}
