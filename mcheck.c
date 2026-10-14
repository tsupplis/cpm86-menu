/*
 * MCHECK [datfile] - load and validate a MENU .dat file, change nothing.
 *
 * Uses the same parser as MENU (menudat.c, built with -DMCHECK so that it
 * reports what it skips or alters). Output, one message per line:
 *
 *   INF: ...   what was understood
 *   WRN: ...   accepted, but altered (cut, ignored directive)
 *   ERR: ...   line not understood, skipped by MENU
 *
 * Exit code: 1 if any ERR (or the file cannot be opened), else 0.
 */
#include <stdio.h>
#include "menudat.h"

extern int (*menu_diag)(int lineno, int level, char *msg, char *text);

static menu_item_t items[MAX_ENTRIES];
static int nwrn;
static int nerr;

static char *typename(int t)
{
    switch (t) {
    case MTYPE_E:   return "E";
    case MTYPE_ENR: return "E!";
    case MTYPE_S:   return "S";
    case MTYPE_SNR: return "S!";
    case MTYPE_M:   return "M";
    case MTYPE_C:   return "C";
    case MTYPE_CNR: return "C!";
    }
    return "?";
}

/* called by load_menu for every skipped (ERR) or altered (WRN) line */
static int report(int lineno, int level, char *msg, char *text)
{
    if (level == MDIAG_ERR) {
        nerr++;
        printf("ERR: ");
    } else {
        nwrn++;
        printf("WRN: ");
    }
    if (lineno > 0)
        printf("line %d: %s\n", lineno, msg);
    else
        printf("%s\n", msg);
    if (text[0] != '\0')
        printf("     %s\n", text);
    return 0;
}

int main(int argc, char **argv)
{
    char *datfile;
    int   count;
    int   i;
    int   nsnr;

    datfile = "menu.dat";
    if (argc > 1)
        datfile = argv[1];

    menu_diag = report;
    nwrn = 0;
    nerr = 0;

    printf("INF: checking %s\n", datfile);
    count = load_menu(items, MAX_ENTRIES, datfile);

    printf("INF: title: %s\n", menu_title);
    printf("INF: colours: title %d, items %d\n", menu_title_fg, menu_item_fg);
    if (menu_quit_disabled)
        printf("INF: Q! quit disabled\n");
    if (menu_back[0] != '\0')
        printf("INF: M! back to %s\n", menu_back);

    nsnr = 0;
    for (i = 0; i < count; i++) {
        printf("INF: entry %d: %s", i + 1, typename(items[i].type));
        if (items[i].pause)
            printf("P");
        if (items[i].user >= 0)
            printf("@%d", items[i].user);
        printf(" %s | %s\n", items[i].label, items[i].cmd);
        if (items[i].type == MTYPE_SNR)
            nsnr++;
    }

    if (count == 0 && nerr == 0)
        printf("WRN: no entries found\n"), nwrn++;
    if (count == 0 && nerr > 0)
        printf("ERR: no usable entries\n");
    printf("INF: %d entries, %d warnings, %d errors\n", count, nwrn, nerr);

    return nerr > 0 ? 1 : 0;
}
