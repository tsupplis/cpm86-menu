/* CP/M-86 1.1 CCP stack backend. */
#include <stdio.h>
#include "util.h"
#include "sub.h"
#include "menuexec.h"

/*
 * Build the re-launch command "[D:]MENU D:datfile [nP]".
 * cmddrv: drive MENU.CMD was run from (sub_cmddrv(), 0 = none), so that
 *         "B:MENU" started from A> is re-launched from B.
 * datfile gets the current drive (BDOS 25) when it has no drive, so the
 * line is valid wherever the CCP runs it from.
 * n brings the menu back on entry n (1-based); P makes it wait for a key
 * first so the output of the entry stays on screen.
 */
static void build_menucmd(char *menucmd, char *datfile, int cmddrv, int sel, int pause)
{
    int i;
    int j;

    j = 0;
    if (cmddrv > 0) {
        menucmd[j++] = (char)('A' + cmddrv - 1);
        menucmd[j++] = ':';
    }
    menucmd[j++]='M'; menucmd[j++]='E'; menucmd[j++]='N'; menucmd[j++]='U';
    menucmd[j++]=' ';
    if (datfile[0] == '\0' || datfile[1] != ':') {
        menucmd[j++] = (char)('A' + (bdos(25, 0) & 0x0F));
        menucmd[j++] = ':';
    }
    for (i = 0; datfile[i] != '\0' && i < 64; i++)
        menucmd[j++] = datfile[i];
    menucmd[j++] = ' ';
    menucmd[j++] = '[';
    if (sel + 1 >= 10)
        menucmd[j++] = (char)('0' + (sel + 1) / 10);
    menucmd[j++] = (char)('0' + (sel + 1) % 10);
    if (pause)
        menucmd[j++] = 'P';
    menucmd[j++] = ']';
    menucmd[j] = '\0';
}

/*
 * Build "[D:]text n" for a stack record, e.g. "USER 4" or "B:MCLEAN 4".
 * drv: drive prefix (1=A..16=P), 0 = none. n is 0..99.
 */
static void numrec(char *buf, int drv, char *text, int n)
{
    int i;
    int j;

    j = 0;
    if (drv > 0) {
        buf[j++] = (char)('A' + drv - 1);
        buf[j++] = ':';
    }
    for (i = 0; text[i] != '\0'; i++)
        buf[j++] = text[i];
    buf[j++] = ' ';
    if (n >= 10)
        buf[j++] = (char)('0' + n / 10);
    buf[j++] = (char)('0' + n % 10);
    buf[j] = '\0';
}

/* ------------------------------------------------------------------ */

char *menusub_prepare(menu_exec_t *ctx, menu_item_t *item, char *datfile, int sel)
{
    int type = item->type;
    char menucmd[96];
    int remenu;
    int n;
    int bad;
    int opened;
    int rollbackbad;
    int home;       /* user area MENU runs in */
    int hop;        /* run the entry in user area item->user */
    int altbefore;  /* records already in $$$.SUB of user n */
    int need;       /* records to push, for the capacity check */
    char rec[32];   /* USER / MCLEAN stack record */

    /* @n equal to the current user is no hop at all */
    home = bdos(32, 0xFF) & 0xFF;
    hop  = (item->user >= 0 && item->user != home);

    /* E, S and C re-launch MENU (on the current dat, so also
       from a sub-menu); E!, S! and C! do not */
    remenu = (type == MTYPE_E || type == MTYPE_S || type == MTYPE_C);

    /* read and check the .sub file before touching $$$.SUB */
    n = 0;
    if (type == MTYPE_S || type == MTYPE_SNR) {
        n = sub_load(item->cmd);
        if (n <= 0) {
            ctx->error_arg = sub_name();
            return sub_errmsg(n);
        }
    }

    /* built here so it follows M sub-menu navigation */
    build_menucmd(menucmd, datfile, ctx->cmddrv, sel, item->pause);

    if (type == MTYPE_CNR) {
        if (ctx->subact != 1) sub_delete();
        return 0;
    }

    /* User hop: the CCP only reads $$$.SUB of its own user, and
       "USER n" does not warm boot, so the stack of user n is read
       next. Write that stack first (in user n):
           USER home (bottom), then the command or the S file.
       The home stack below ends with MCLEAN n, USER n (top). */
    if (hop) {
        /* Both stacks must fit in 128 records before anything is
           written: a failure after the first one cannot always be
           undone. sub_records() writes nothing. */
        need = (remenu ? 1 : 0) + 2;
        if (ctx->subact == 1 && sub_records() + need > 128) {
            return "Too many commands in $$$.SUB";
        }
        bdos(32, item->user);
        altbefore = sub_records();
        need = 1 + ((type == MTYPE_S || type == MTYPE_SNR) ? n : 1);
        if (ctx->subact == 1 && altbefore + need > 128) {
            bdos(32, home);
            return "Too many commands in $$$.SUB (user area)";
        }
        numrec(rec, 0, "USER", home);
        opened = (sub_open(ctx->subact == 1 ? SUB_APPEND : SUB_CREATE) == 0);
        bad = !opened;
        if (!bad)
            bad = (sub_append(rec) != 0);
        if (!bad && (type == MTYPE_S || type == MTYPE_SNR))
            bad = (sub_pushall() != 0);
        else if (!bad)
            bad = (sub_append(item->cmd) != 0);
        if (!bad)
            bad = (sub_close() != 0);
        if (bad && opened)
            sub_abort();     /* before leaving user n */
        bdos(32, home);
        if (bad) {
            return "Cannot write $$$.SUB in user area";
        }
    }

    /* records are a stack: first written = bottom = runs last.
       Inside a SUBMIT job push on top of what is left of it. */
    opened = (sub_open(ctx->subact == 1 ? SUB_APPEND : SUB_CREATE) == 0);
    bad = !opened;
    if (!bad && remenu)
        bad = (sub_append(menucmd) != 0);
    if (!bad && hop) {
        numrec(rec, ctx->cmddrv, "MCLEAN", item->user);
        bad = (sub_append(rec) != 0);
        if (!bad) {
            numrec(rec, 0, "USER", item->user);
            bad = (sub_append(rec) != 0);
        }
    } else if (!bad && (type == MTYPE_S || type == MTYPE_SNR)) {
        bad = (sub_pushall() != 0);
    } else if (!bad && (type == MTYPE_E || type == MTYPE_ENR)) {
        bad = (sub_append(item->cmd) != 0);
    }
    if (!bad)
        bad = (sub_close() != 0);
    if (bad) {
        rollbackbad = opened && sub_abort() != 0;
        if (hop) {
            bdos(32, item->user);
            if (sub_restore(ctx->subact == 1 ? altbefore : 0) != 0)
                rollbackbad = 1;
            bdos(32, home);
        }
        return rollbackbad ? "Submit rollback failed; inspect both stacks" :
                             "Cannot write $$$.SUB";
    }
    return 0;
}

void menusub_run(menu_exec_t *ctx, menu_item_t *item)
{
    if (item->type == MTYPE_CNR)
        p_chain(item->cmd, ctx->subact == 1);
    else if (item->type == MTYPE_C)
        p_chain(item->cmd, 1);
    else
        sub_exit();
}
