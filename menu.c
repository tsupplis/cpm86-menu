#include <stdio.h>
#include "conio.h"
#include "menudat.h"
#include "sub.h"

/*
 * Screen layout -- 80 cols x 24 rows, all coords 0-based (VT52 gotoxy is 0-based)
 *
 *  Row  0 : a----[78 dashes]----c       outer top border
 *  Row  1 : |  CP/M-86 Application Menu v1.0  |   title (yellow, centred)
 *  Row  2 : b====[78 equals]====d       title box bottom
 *  Row  3 : |                   |       blank
 *  Row  4 : |  1. entry 0       |
 *  Row  5 : |  2. entry 1       |
 *   ...up to 15 entries...
 *  Row 18 : | 15. entry 15      |
 *  Row 19..21: blank filler rows
 *  Row 22 : c----[78 dashes]----d       content box bottom
 *  Row 23 :    [Up/k]...               footer (unboxed)
 *
 * Each row: col 0 = '|', cols 1..78 = content (78 chars), col 79 = '|'
 * Total chars per row = 80  (fits in 80-col terminal without wrapping)
 * INNER_WIDTH = 78
 */

#define SCREEN_COLS  80
#define INNER_WIDTH  78   /* dashes between corners; total row = 1+78+1 = 80 chars */

/* All row numbers are 0-based */
#define BOX_TOP_ROW   0   /* a---c  title box top    */
#define TITLE_ROW     1   /* |   |  title             */
#define SEP1_ROW      2   /* b===d  title box bottom  */
#define BLANK1_ROW    3   /* |   |  blank             */
#define FIRST_ROW     4   /* |   |  first entry       */
#define SEP3_ROW      22  /* c---d  content box bottom */
#define FOOTER_ROW    23  /* unboxed footer            */

static char CH_WALL  = 179;
static char CH_DASH  = 196;
static char CH_EQUAL = 205;
static char CH_EQL   = 198;
static char CH_EQR   = 181;
static char CH_SPACE = ' ';
static char CH_TL    = 218;   /* top-left corner */
static char CH_TR    = 191;   /* top-right corner */
static char CH_BL    = 192;   /* bottom-left corner */
static char CH_BR    = 217;   /* bottom-right corner */

static MenuItem items[MAX_ENTRIES];
static int      count = 0;

/* ------------------------------------------------------------------ */

static void setfg(int fg)
{
    cputc((char)27); cputc((char)'b'); cputc((char)fg);
}

static void setbg(int bg)
{
    cputc((char)27); cputc((char)'c'); cputc((char)bg);
}

/* ------------------------------------------------------------------ */
/* Draw a horizontal border row with given fill, left and right chars  */
static void draw_hline(int row, char mid, char left, char right)
{
    int i;
    gotoxy(row, 0);
    cputc(left);
    for (i = 0; i < INNER_WIDTH; i++) cputc(mid);
    cputc(right);
}

static void draw_blank_row(int row)
{
    int i;
    gotoxy(row, 0);
    cputc(CH_WALL);
    for (i = 0; i < INNER_WIDTH; i++) cputc(CH_SPACE);
    cputc(CH_WALL);
}

/* ------------------------------------------------------------------ */

static void draw_entry(int idx, int selected)
{
    int n;
    int col;
    char *p;

    gotoxy(FIRST_ROW + idx, 0);
    cputc(CH_WALL);
    cputc(CH_SPACE);                        /* 1 normal space before highlight */

    if (selected) { setfg(0);  setbg(7); }
    else          { setfg(14); setbg(0); }

    /* 1 highlighted space + N. label + fill + 1 highlighted space */
    cputc(CH_SPACE);
    col = 2; /* 1 normal + 1 highlighted so far */

    n = idx + 1;
    if (n >= 10) {
        cputc((char)('0' + n / 10));
        cputc((char)('0' + n % 10));
        col += 2;
    } else {
        cputc((char)('0' + n));
        col += 1;
    }
    cputs(". ");
    col += 2;

    for (p = items[idx].label; *p != '\0'; p++) {
        cputc(*p);
        col++;
    }

    /* fill up to 1 char before the trailing normal space and right wall */
    while (col < INNER_WIDTH - 2) {
        cputc(CH_SPACE);
        col++;
    }
    cputc(CH_SPACE);                        /* 1 highlighted space after label */

    setfg(7); setbg(0);
    cputc(CH_SPACE);                        /* 1 normal space after highlight */
    cputc(CH_WALL);
}

/* ------------------------------------------------------------------ */
/* Compute length of a C string                                         */
static int slen(p)
    char *p;
{
    int n;
    for (n = 0; p[n] != '\0'; n++) ;
    return n;
}

/* ------------------------------------------------------------------ */
/* has_back: 1 = show [B] Back in footer; 0 = don't                    */
static void draw_screen(int sel, int has_back)
{
    int i;
    int pad;
    int tlen;
    int row;

    clrscr();
    cursor(CURSOR_OFF);
    setfg(7); setbg(0);

    /* Row 0: a---c  title box top */
    draw_hline(BOX_TOP_ROW, CH_DASH, CH_TL, CH_TR);

    /* Row 1: | title | */
    tlen = slen(menu_title);
    gotoxy(TITLE_ROW, 0);
    cputc(CH_WALL);
    pad = (INNER_WIDTH - tlen) / 2;
    for (i = 0; i < pad; i++) cputc(CH_SPACE);
    setfg(14);
    cputs(menu_title);
    setfg(7);
    i = pad + tlen;
    while (i < INNER_WIDTH) { cputc(CH_SPACE); i++; }
    cputc(CH_WALL);

    /* Row 2: b===d  title box bottom */
    draw_hline(SEP1_ROW, CH_EQUAL, CH_EQL, CH_EQR);

    /* Row 3: blank */
    draw_blank_row(BLANK1_ROW);

    /* Rows 4..N: entries + blank filler */
    for (i = 0; i < count; i++)
        draw_entry(i, i == sel);
    for (row = FIRST_ROW + count; row < SEP3_ROW; row++)
        draw_blank_row(row);

    /* Row 22: c---d  content box bottom */
    draw_hline(SEP3_ROW, CH_DASH, CH_BL, CH_BR);

    /* Row 23: unboxed footer */
    gotoxy(FOOTER_ROW, 0);
    cputs(" [Up/'K'] Previous  [Down/'J'] Next  [Enter] Launch");
    if (!menu_quit_disabled)
        cputs("  [Q] Quit");
    if (has_back)
        cputs("  [B] Back");
}

/* ------------------------------------------------------------------ */
/*
 * run_submit: read a .sub file, substitute $1..$9 from params[], and
 * append each resulting command to $$$.sub in reverse order (last line
 * first so the CCP executes them top-down).
 *
 * cmd format: "FILENAME [param1 [param2 ...]]"
 * The .sub file is opened as "FILENAME.SUB".
 *
 * Returns number of lines appended, or 0 on error.
 */
#define SUB_MAX_LINES  32
#define SUB_LINE_LEN   (MAX_CMD + 1)

static int run_submit(cmd, menucmd, remenu)
    char *cmd;
    char *menucmd;  /* "[D:]MENU D:datfile" -- appended at bottom of stack for S */
    int   remenu;   /* 1 = append menucmd (S), 0 = don't (S!)             */
{
    FILE *fp;
    char  fname[13];        /* 8.3 + NUL                  */
    char  params[10][13];   /* $1..$9, each up to 12 chars */
    int   nparams;
    char  lines[SUB_MAX_LINES][SUB_LINE_LEN];
    int   nlines;
    char  buf[160];
    char  out[SUB_LINE_LEN];
    char *p;
    char *q;
    int   i;
    int   j;
    int   n;
    int   pn;

    /* --- split cmd into filename and params --- */
    p = cmd;
    i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && i < 12)
        fname[i++] = *p++;
    /* append .SUB extension */
    fname[i] = '\0';
    /* add .SUB if no dot present */
    {
        int hasdot = 0;
        for (j = 0; j < i; j++) if (fname[j] == '.') { hasdot = 1; break; }
        if (!hasdot && i <= 8) {
            fname[i]='.'; fname[i+1]='S'; fname[i+2]='U'; fname[i+3]='B';
            fname[i+4]='\0';
        }
    }

    /* collect params $1..$9 */
    nparams = 0;
    while (*p != '\0' && nparams < 9) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        j = 0;
        while (*p != '\0' && *p != ' ' && *p != '\t' && j < 12)
            params[nparams][j++] = *p++;
        params[nparams][j] = '\0';
        nparams++;
    }

    /* --- open and read .sub file --- */
    fp = fopen(fname, "r");
    if (fp == 0) return 0;

    nlines = 0;
    while (nlines < SUB_MAX_LINES && fgets(buf, sizeof(buf), fp) != 0) {
        /* strip \r\n */
        for (q = buf; *q != '\0'; q++)
            if (*q == '\r' || *q == '\n') { *q = '\0'; break; }

        /* skip blank and ; lines */
        q = buf;
        while (*q == ' ' || *q == '\t') q++;
        if (*q == '\0' || *q == ';') continue;

        /* substitute $1..$9 into out[] */
        i = 0; j = 0;
        while (q[i] != '\0' && j < MAX_CMD) {
            if (q[i] == '$' && q[i+1] >= '1' && q[i+1] <= '9') {
                pn = q[i+1] - '1';  /* 0-based index */
                if (pn < nparams) {
                    char *s = params[pn];
                    while (*s != '\0' && j < MAX_CMD)
                        out[j++] = *s++;
                }
                i += 2;
            } else {
                out[j++] = q[i++];
            }
        }
        out[j] = '\0';
        if (j == 0) continue;  /* skip empty result lines */

        /* upcase */
        for (n = 0; out[n] != '\0'; n++)
            if (out[n] >= 'a' && out[n] <= 'z') out[n] = out[n] - 'a' + 'A';

        /* store */
        for (n = 0; out[n] != '\0' && n < MAX_CMD; n++)
            lines[nlines][n] = out[n];
        lines[nlines][n] = '\0';
        nlines++;
    }
    fclose(fp);

    if (nlines == 0) return 0;

    /* --- append to $$$.sub in reverse order --- */
    if (remenu)
        sub_append(menucmd);        /* bottom of stack -- runs last */

    for (i = nlines - 1; i >= 0; i--)
        sub_append(lines[i]);       /* last line first = top of stack */

    return nlines;
}

/* ------------------------------------------------------------------ */

/*
 * Build the re-launch command "[D:]MENU D:datfile".
 * cmddrv: drive MENU.CMD was run from (sub_cmddrv(), 0 = none), so that
 *         "B:MENU" started from A> is re-launched from B.
 * datfile gets the current drive (BDOS 25) when it has no drive, so the
 * line is valid wherever the CCP runs it from.
 */
static void build_menucmd(menucmd, datfile, cmddrv)
    char *menucmd;
    char *datfile;
    int   cmddrv;
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
    menucmd[j] = '\0';
}

/* ------------------------------------------------------------------ */

int main(argc, argv)
    int argc;
    char **argv;
{
    int  sel;
    int  prev;
    int  c;
    int  has_back;
    char datfile[65];
    char menucmd[80];   /* "[D:]MENU D:datfile" -- re-launch command */
    int  cmddrv;
    int  type;

    /* initialise datfile from argv or default */
    if (argc > 1) {
        int i;
        for (i = 0; argv[1][i] != '\0' && i < 64; i++)
            datfile[i] = argv[1][i];
        datfile[i] = '\0';
    } else {
        datfile[0]='m'; datfile[1]='e'; datfile[2]='n';
        datfile[3]='u'; datfile[4]='.'; datfile[5]='d';
        datfile[6]='a'; datfile[7]='t'; datfile[8]='\0';
    }

    cmddrv = sub_cmddrv();

reload:
    sel = 0;

    count = load_menu(items, MAX_ENTRIES, datfile);
    if (count == 0) {
        cputs("No entries found in: "); cputs(datfile); cputs("\r\n");
        cputs("Usage: MENU [datafile.dat]\r\n");
        return 1;
    }

    /* has_back driven by M! directive in the loaded dat */
    has_back = (menu_back[0] != '\0') ? 1 : 0;

    draw_screen(sel, has_back);

    for (;;) {
        c = getch();

        /* ESC prefix: VT52 arrow key */
        if (c == 27) {
            c = getch();
            if      (c == 'A') c = 'k';  /* up   */
            else if (c == 'B') c = 'j';  /* down */
            else               continue;
        }

        if (c == 'k' || c == 'K') {
            if (sel == 0) continue;
            prev = sel; sel--;
            draw_entry(prev, 0);
            draw_entry(sel,  1);

        } else if (c == 'j' || c == 'J') {
            if (sel == count - 1) continue;
            prev = sel; sel++;
            draw_entry(prev, 0);
            draw_entry(sel,  1);

        } else if (c == '\r' || c == '\n') {
            type = items[sel].type;

            /* M : in-process sub-menu reload */
            if (type == MTYPE_M) {
                int  newcount;
                int  i;
                char newdat[65];

                /* snapshot target filename before items[] is overwritten */
                for (i = 0; items[sel].cmd[i] != '\0' && i < 64; i++)
                    newdat[i] = items[sel].cmd[i];
                newdat[i] = '\0';

                newcount = load_menu(items, MAX_ENTRIES, newdat);
                if (newcount == 0) {
                    /* empty or missing dat -- stay on current menu */
                    count = load_menu(items, MAX_ENTRIES, datfile);
                    draw_screen(sel, has_back);
                    continue;
                }
                for (i = 0; newdat[i] != '\0' && i < 64; i++)
                    datfile[i] = newdat[i];
                datfile[i] = '\0';

                /* items[] already holds the new menu from load_menu above */
                /* has_back is set after reload from menu_back directive    */
                count    = newcount;
                sel      = 0;
                has_back = (menu_back[0] != '\0') ? 1 : 0;
                draw_screen(sel, has_back);
                continue;
            }

            /* built here so it follows M sub-menu navigation */
            build_menucmd(menucmd, datfile, cmddrv);

            /* C / C! : chain directly to program via P_CHAIN */
            if (type == MTYPE_C || type == MTYPE_CNR) {
                clrscr();
                cursor(CURSOR_ON);
                if (type == MTYPE_C) {
                    /* write MENU datfile into $$$.sub then chain;
                       CCP picks up $$$SUB automatically on next warm boot */
                    sub_open(SUB_CREATE);
                    sub_append(menucmd);
                    sub_close();
                } else {
                    /* C! : delete any stale $$$.sub, then chain clean */
                    sub_delete();
                }
                p_chain(items[sel].cmd, type == MTYPE_C); /* no return on success */
                return 0;                 /* failure fallback */
            }

            /* E / E! / S / S! : write $$$.sub and exit via BDOS 0 */
            clrscr();
            cursor(CURSOR_ON);
            sub_open(SUB_CREATE);

            if (type == MTYPE_S || type == MTYPE_SNR) {
                /* read .sub file, substitute params, append lines reversed */
                run_submit(items[sel].cmd, menucmd,
                           type == MTYPE_S ? 1 : 0);
            } else {
                /* E / E! : single command */
                if (type == MTYPE_E)
                    sub_append(menucmd);  /* bottom of stack -- runs last */
                sub_append(items[sel].cmd);
            }

            sub_close();
            sub_exit();   /* set MDSUBE, BDOS 0 -- does not return */
            return 0;     /* unreachable, keeps compiler happy */

        } else if ((c == 'q' || c == 'Q') && !menu_quit_disabled) {
            sub_delete();
            clrscr();
            cursor(CURSOR_ON);
            return 0;

        } else if ((c == 'b' || c == 'B') && has_back) {
            /* Back: reload dat named by M! directive */
            int i;
            for (i = 0; menu_back[i] != '\0' && i < 64; i++)
                datfile[i] = menu_back[i];
            datfile[i] = '\0';
            goto reload;
        }
    }
}
