#include <stdio.h>
#include "menudat.h"
#include "sub.h"

/* Definitions for the globals declared extern in menudat.h */
static char default_title[]       = "CP/M-86 application menu version 1.0";
char menu_title[MAX_TITLE + 1];
int  menu_title_fg                = DEF_COLOR;
int  menu_item_fg                 = DEF_COLOR;
int  menu_quit_disabled           = 0;
int  menu_has_snr                 = 0;
char menu_back[MAX_CMD + 1]       = "";

/*
 * MCHECK build (mcheck.cmd): the same parser, plus a diagnostics hook.
 * menu_diag(lineno, level, message, text) is called for every line that
 * is skipped (MDIAG_ERR) or accepted in altered form (MDIAG_WRN).
 * The normal MENU build compiles all of this away.
 */
#ifdef MCHECK
int (*menu_diag)() = 0;
static char raw[256];
#define DIAG(lv, msg)  if (menu_diag) (*menu_diag)(lineno, lv, msg, raw)
#define SAVELINE()     scopy(raw, buf, 255)
#define NOLINE()       raw[0] = '\0'
#define FULL           continue
#else
#define DIAG(lv, msg)
#define SAVELINE()
#define NOLINE()
#define FULL           break
#endif

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/* Return pointer to first non-space/tab character */
static char *ltrim(p)
    char *p;
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* Trim trailing whitespace in-place */
static void rtrim(p)
    char *p;
{
    int len;
    len = 0;
    while (p[len] != '\0')
        len++;
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t'))
        len--;
    p[len] = '\0';
}

/* Copy at most max chars from src into dst, null-terminate */
static void scopy(dst, src, max)
    char *dst;
    char *src;
    int   max;
{
    int i;
    for (i = 0; i < max && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

#ifdef MCHECK
static int slen(p)
    char *p;
{
    int n;
    for (n = 0; p[n] != '\0'; n++)
        ;
    return n;
}
#endif

/* ------------------------------------------------------------------ */

int load_menu(items, max, filename)
    MenuItem *items;
    int max;
    char *filename;
{
    FILE *fp;
    char  buf[256];
    char *p;
    char *pipe;
    char *label;
    char *cmd;
    int   count;
    int   type;
    int   pause;
    int   user;
    int   len;
    int   n;
    int   lineno;

    /* Reset globals for this load */
    menu_quit_disabled = 0;
    menu_title_fg      = DEF_COLOR;
    menu_item_fg       = DEF_COLOR;
    menu_has_snr       = 0;
    menu_back[0]       = '\0';
    scopy(menu_title, default_title, MAX_TITLE);

    lineno = 0;
    NOLINE();

    fp = fopen(filename, "r");
    if (fp == 0) {
        DIAG(MDIAG_ERR, "cannot open the menu file");
        return 0;
    }

    count = 0;
    /* CR LF, LF or CR line ends; a line too long for buf is skipped */
    while ((len = sub_readln(fp, buf, sizeof(buf))) != SUB_RD_EOF) {
        lineno++;
        if (len == SUB_RD_LONG) {
            NOLINE();
            DIAG(MDIAG_ERR, "line longer than 255 characters, skipped");
            continue;
        }
        SAVELINE();

        /* skip blank lines and ; comments */
        p = ltrim(buf);
        if (*p == '\0' || *p == ';')
            continue;

        /* ---- T : title ------------------------------------------- */
        if (p[0] == 'T' && (p[1] == ' ' || p[1] == '\t')) {
            label = ltrim(p + 2);
            rtrim(label);
            if (*label != '\0') {
#ifdef MCHECK
                if (slen(label) > MAX_TITLE) {
                    DIAG(MDIAG_WRN, "title longer than 70 characters, cut");
                }
#endif
                scopy(menu_title, label, MAX_TITLE);
            } else {
                DIAG(MDIAG_WRN, "empty title, default title kept");
            }
            continue;
        }

        /* ---- CTn / CIn : title / item foreground colour 0..15 ------- */
        if (p[0] == 'C' && (p[1] == 'T' || p[1] == 'I')
                        && (p[2] == ' ' || p[2] == '\t'
                            || (p[2] >= '0' && p[2] <= '9'))) {
            char *q = ltrim(p + 2);
            n = -1;
            if (*q >= '0' && *q <= '9') {
                n = *q++ - '0';
                if (*q >= '0' && *q <= '9')
                    n = n * 10 + (*q++ - '0');
                if (*q != '\0' && *q != ' ' && *q != '\t')
                    n = -1;
            }
            if (n < 0 || n > MAX_COLOR) {
                DIAG(MDIAG_ERR, "bad colour: expected a number 0-15");
            } else if (p[1] == 'T') {
                menu_title_fg = n;
            } else {
                menu_item_fg = n;
            }
            continue;
        }

        /* ---- Q! : disable quit ------------------------------------ */
        if (p[0] == 'Q' && p[1] == '!' && (p[2] == '\0' || p[2] == ' ' || p[2] == '\t')) {
            menu_quit_disabled = 1;
            continue;
        }

        /* ---- M! : back directive (sets B key target, not an entry) */
        if (p[0] == 'M' && p[1] == '!' && (p[2] == ' ' || p[2] == '\t')) {
            cmd = ltrim(p + 3);
            rtrim(cmd);
            if (*cmd != '\0') {
                scopy(menu_back, cmd, MAX_CMD);
            } else {
                DIAG(MDIAG_WRN, "M! without a file name, ignored");
            }
            continue;
        }

        /* ---- @n suffix glued to the type: E@4 label | cmd ---------- */
        /* Valid: 1- or 2-char type token, '@', 1-2 digits 0..MAX_USER, */
        /* then a blank. The @n is blanked so the type tests see "E ".   */
        user = -1;
        for (n = 0; n <= 2 && p[n] != '\0' && p[n] != ' ' && p[n] != '\t' && p[n] != '@'; n++)
            ;
        if (n >= 1 && n <= 2 && p[n] == '@') {
            int d = n + 1;
            if (p[d] < '0' || p[d] > '9') {
                DIAG(MDIAG_ERR, "bad @n suffix: expected @ and a user number 0-15");
                continue;   /* bare @ or letter -- malformed, skip */
            }
            user = p[d++] - '0';
            if (p[d] >= '0' && p[d] <= '9')
                user = user * 10 + (p[d++] - '0');
            if (user > MAX_USER) {
                DIAG(MDIAG_ERR, "user number over 15 in @n suffix");
                continue;
            }
            if (p[d] != ' ' && p[d] != '\t') {
                DIAG(MDIAG_ERR, "bad @n suffix: expected a blank after the number");
                continue;   /* @4A, @123 -- malformed, skip */
            }
            while (n < d)
                p[n++] = ' ';
        }

        /* ---- Entry directives: determine type --------------------- */
        pause = 0;
        if      (p[0]=='E' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_ENR; p+=3; }
        else if (p[0]=='E' && p[1]=='P' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_E;   p+=3; pause=1; }
        else if (p[0]=='S' && p[1]=='P' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_S;   p+=3; pause=1; }
        else if (p[0]=='C' && p[1]=='P' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_C;   p+=3; pause=1; }
        else if (p[0]=='E' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_E;   p+=2; }
        else if (p[0]=='S' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_SNR; p+=3; }
        else if (p[0]=='S' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_S;   p+=2; }
        else if (p[0]=='M' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_M;   p+=2; }
        else if (p[0]=='C' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_CNR; p+=3; }
        else if (p[0]=='C' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_C;   p+=2; }
        else {
            DIAG(MDIAG_ERR, "unknown line type, skipped");
            continue;  /* unknown prefix -- skip */
        }

        /* the hop is only for E and S: chain runs before the relay, M loads in-process */
        if (user >= 0 && type != MTYPE_E && type != MTYPE_ENR
                      && type != MTYPE_S && type != MTYPE_SNR) {
            DIAG(MDIAG_ERR, "@n is only valid on E, E!, EP, S, S! and SP");
            continue;
        }

        /* ---- find pipe separator ---------------------------------- */
        pipe = p;
        while (*pipe != '\0' && *pipe != '|')
            pipe++;
        if (*pipe != '|') {
            DIAG(MDIAG_ERR, "missing '|' between label and command");
            continue;   /* malformed -- skip */
        }

        /* split at pipe, trim both sides */
        *pipe = '\0';
        label = ltrim(p);    rtrim(label);
        cmd   = ltrim(pipe + 1); rtrim(cmd);

        if (*label == '\0' || *cmd == '\0') {
            DIAG(MDIAG_ERR, "empty label or command");
            continue;   /* empty label or cmd -- skip */
        }

        /* a cut command would run something else -- skip it */
        {
            int n;
            for (n = 0; cmd[n] != '\0'; n++) ;
            if (n > MAX_CMD) {
                DIAG(MDIAG_ERR, "command longer than 125 characters");
                continue;
            }
        }

        if (count >= max) {
            DIAG(MDIAG_ERR, "too many entries (maximum 15), entry ignored");
            FULL;       /* array full */
        }

#ifdef MCHECK
        if (slen(label) > MAX_LABEL) {
            DIAG(MDIAG_WRN, "label longer than 70 characters, cut");
        }
#endif

        scopy(items[count].label, label, MAX_LABEL);
        scopy(items[count].cmd,   cmd,   MAX_CMD);
        items[count].type = type;
        items[count].pause = pause;
        items[count].user  = user;

        if (type == MTYPE_SNR)
            menu_has_snr = 1;

        count++;
    }

    fclose(fp);
    return count;
}
