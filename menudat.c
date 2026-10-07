#include <stdio.h>
#include "menudat.h"

/* Definitions for the globals declared extern in menudat.h */
char menu_title[MAX_TITLE + 1]    = "CP/M-86 application menu version 1.0";
int  menu_quit_disabled           = 0;
int  menu_has_snr                 = 0;
char menu_back[MAX_CMD + 1]       = "";

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

/* ------------------------------------------------------------------ */

int load_menu(items, max, filename)
    MenuItem *items;
    int max;
    char *filename;
{
    FILE *fp;
    char  buf[160];
    char *p;
    char *pipe;
    char *label;
    char *cmd;
    int   count;
    int   type;

    /* Reset globals for this load */
    menu_quit_disabled = 0;
    menu_has_snr       = 0;
    menu_back[0]       = '\0';
    /* menu_title keeps its current value until a T line overrides it */

    fp = fopen(filename, "r");
    if (fp == 0)
        return 0;

    count = 0;
    while (fgets(buf, sizeof(buf), fp) != 0) {

        /* strip trailing \r and \n */
        for (p = buf; *p != '\0'; p++) {
            if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        }

        /* skip blank lines and ; comments */
        p = ltrim(buf);
        if (*p == '\0' || *p == ';')
            continue;

        /* ---- T : title ------------------------------------------- */
        if (p[0] == 'T' && (p[1] == ' ' || p[1] == '\t')) {
            label = ltrim(p + 2);
            rtrim(label);
            if (*label != '\0')
                scopy(menu_title, label, MAX_TITLE);
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
            if (*cmd != '\0')
                scopy(menu_back, cmd, MAX_CMD);
            continue;
        }

        /* ---- Entry directives: determine type --------------------- */
        if      (p[0]=='E' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_ENR; p+=3; }
        else if (p[0]=='E' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_E;   p+=2; }
        else if (p[0]=='S' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_SNR; p+=3; }
        else if (p[0]=='S' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_S;   p+=2; }
        else if (p[0]=='M' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_M;   p+=2; }
        else if (p[0]=='C' && p[1]=='!' && (p[2]==' '||p[2]=='\t')) { type=MTYPE_CNR; p+=3; }
        else if (p[0]=='C' &&              (p[1]==' '||p[1]=='\t')) { type=MTYPE_C;   p+=2; }
        else continue;  /* unknown prefix -- skip */

        /* ---- find pipe separator ---------------------------------- */
        pipe = p;
        while (*pipe != '\0' && *pipe != '|')
            pipe++;
        if (*pipe != '|')
            continue;   /* malformed -- skip */

        /* split at pipe, trim both sides */
        *pipe = '\0';
        label = ltrim(p);    rtrim(label);
        cmd   = ltrim(pipe + 1); rtrim(cmd);

        if (*label == '\0' || *cmd == '\0')
            continue;   /* empty label or cmd -- skip */

        if (count >= max)
            break;      /* array full */

        scopy(items[count].label, label, MAX_LABEL);
        scopy(items[count].cmd,   cmd,   MAX_CMD);
        items[count].type = type;

        if (type == MTYPE_SNR)
            menu_has_snr = 1;

        count++;
    }

    fclose(fp);
    return count;
}
