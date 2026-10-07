/*
 * msub.c -- MSUB, a SUBMIT replacement built on sub.lib.
 * Licensed under the MIT license. See LICENSE file in the project root.
 *
 * MSUB file[.SUB] [parm1 parm2 ...]
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

    cputs("\r\nMSUB SUB ORCHESTRATOR VER 1.0\r\n\r\n");

    if ((bdos(12, 0) & 0xFF) != 0x22) {
        cputs("CP/M-86 1.1 required\r\n");
        return 1;
    }
    if (argc < 2) {
        cputs("USAGE: MSUB file[.SUB] [parm1 parm2 ...]\r\n");
        return 0;
    }

    /* rebuild "file parm1 parm2 ..." from the command tail */
    j = 0;
    for (i = 1; i < argc; i++) {
        if (i > 1 && j < 127)
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
