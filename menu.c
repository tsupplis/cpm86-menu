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
/* Rows are built in rowbuf and sent with one cputs (BDOS 9) per row,
   instead of one BDOS call per character. */
static char rowbuf[SCREEN_COLS + 1];

/* Draw a horizontal border row with given fill, left and right chars  */
static void draw_hline(int row, char mid, char left, char right)
{
    int i;
    gotoxy(row, 0);
    rowbuf[0] = left;
    for (i = 1; i <= INNER_WIDTH; i++) rowbuf[i] = mid;
    rowbuf[INNER_WIDTH + 1] = right;
    rowbuf[INNER_WIDTH + 2] = '\0';
    cputs(rowbuf);
}

static void draw_blank_row(int row)
{
    draw_hline(row, CH_SPACE, CH_WALL, CH_WALL);
}

/* ------------------------------------------------------------------ */

static void draw_entry(int idx, int selected)
{
    int n;
    int k;
    int col;
    char *p;

    gotoxy(FIRST_ROW + idx, 0);
    cputc(CH_WALL);
    cputc(CH_SPACE);                        /* 1 normal space before highlight */

    if (selected) { setfg(0);  setbg(7); }
    else          { setfg(14); setbg(0); }

    /* 1 highlighted space + N. label + fill + 1 highlighted space,
       built in rowbuf (k) while col tracks the screen column */
    k = 0;
    rowbuf[k++] = CH_SPACE;
    col = 2; /* 1 normal + 1 highlighted so far */

    n = idx + 1;
    if (n >= 10) {
        rowbuf[k++] = (char)('0' + n / 10);
        rowbuf[k++] = (char)('0' + n % 10);
        col += 2;
    } else {
        rowbuf[k++] = (char)('0' + n);
        col += 1;
    }
    rowbuf[k++] = '.';
    rowbuf[k++] = ' ';
    col += 2;

    for (p = items[idx].label; *p != '\0' && col < INNER_WIDTH - 2; p++) {
        rowbuf[k++] = *p;
        col++;
    }

    /* fill up to 1 char before the trailing normal space and right wall */
    while (col < INNER_WIDTH - 2) {
        rowbuf[k++] = CH_SPACE;
        col++;
    }
    rowbuf[k++] = CH_SPACE;                 /* 1 highlighted space after label */
    rowbuf[k] = '\0';
    cputs(rowbuf);

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
    int k;
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
    pad = (INNER_WIDTH - tlen) / 2;
    rowbuf[0] = CH_WALL;
    for (i = 1; i <= pad; i++) rowbuf[i] = CH_SPACE;
    rowbuf[i] = '\0';
    cputs(rowbuf);
    setfg(14);
    cputs(menu_title);
    setfg(7);
    i = pad + tlen;
    k = 0;
    while (i < INNER_WIDTH) { rowbuf[k++] = CH_SPACE; i++; }
    rowbuf[k++] = CH_WALL;
    rowbuf[k] = '\0';
    cputs(rowbuf);

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
/* Show an error on the footer row, wait for a key, redraw the menu.    */
static void menu_error(msg, arg, sel, has_back)
    char *msg;
    char *arg;
    int   sel;
    int   has_back;
{
    draw_screen(sel, has_back);
    gotoxy(FOOTER_ROW, 0);
    clreol();
    cputs(" ");
    cputs(msg);
    if (arg != 0) cputs(arg);
    cputs(" -- press a key");
    getch();
    draw_screen(sel, has_back);
}

/* ------------------------------------------------------------------ */

/*
 * Build the re-launch command "[D:]MENU D:datfile /n[ /P]".
 * cmddrv: drive MENU.CMD was run from (sub_cmddrv(), 0 = none), so that
 *         "B:MENU" started from A> is re-launched from B.
 * datfile gets the current drive (BDOS 25) when it has no drive, so the
 * line is valid wherever the CCP runs it from.
 * /n brings the menu back on entry n (1-based); /P makes it wait for a key
 * first so the output of the entry stays on screen.
 */
static void build_menucmd(menucmd, datfile, cmddrv, sel, pause)
    char *menucmd;
    char *datfile;
    int   cmddrv;
    int   sel;
    int   pause;
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
    menucmd[j++] = '/';
    if (sel + 1 >= 10)
        menucmd[j++] = (char)('0' + (sel + 1) / 10);
    menucmd[j++] = (char)('0' + (sel + 1) % 10);
    if (pause) {
        menucmd[j++] = ' '; menucmd[j++] = '/'; menucmd[j++] = 'P';
    }
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
    char menucmd[96];   /* "[D:]MENU D:datfile /n /P" -- re-launch command */
    int  startsel;      /* /n: entry to select on the first load, 0 = none */
    int  pause;         /* /P: wait for a key before drawing */
    int  cmddrv;
    int  subact;        /* started from $$$.SUB: 1 yes, 0 no, -1 unknown */
    int  type;

    /* $$$.SUB hand-over relies on the CP/M-86 1.1 (BDOS 2.2) CCP */
    if ((bdos(12, 0) & 0xFF) != 0x22) {
        cputs("CP/M-86 1.1 required\r\n");
        return 1;
    }

    /* MENU [datfile] [/n] [/P] */
    datfile[0]='m'; datfile[1]='e'; datfile[2]='n';
    datfile[3]='u'; datfile[4]='.'; datfile[5]='d';
    datfile[6]='a'; datfile[7]='t'; datfile[8]='\0';
    startsel = 0;
    pause    = 0;
    {
        int a;
        int i;
        char *p;
        for (a = 1; a < argc; a++) {
            p = argv[a];
            if (p[0] == '/') {
                if (p[1] == 'P' || p[1] == 'p') {
                    pause = 1;
                } else if (p[1] >= '0' && p[1] <= '9') {
                    startsel = 0;
                    for (i = 1; p[i] >= '0' && p[i] <= '9'; i++)
                        if (startsel < 1000)    /* no int wrap-around */
                            startsel = startsel * 10 + (p[i] - '0');
                }
            } else {
                for (i = 0; p[i] != '\0' && i < 64; i++)
                    datfile[i] = p[i];
                datfile[i] = '\0';
            }
        }
    }

    if (pause) {
        cursor(CURSOR_ON);
        cputs("\r\nPress any key to return to the menu");
        getch();
    }

    cmddrv = sub_cmddrv();
    subact = sub_active();

reload:
    sel = 0;

    count = load_menu(items, MAX_ENTRIES, datfile);
    if (count == 0) {
        cputs("No entries found in: "); cputs(datfile); cputs("\r\n");
        cputs("Usage: MENU [datafile.dat] [/n] [/P]\r\n");
        return 1;
    }

    /* /n applies to the first load only */
    if (startsel >= 1 && startsel <= count)
        sel = startsel - 1;
    startsel = 0;

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

            /* E / E! / S / S! / C / C! : hand over to the CCP via $$$.SUB */
            {
                int remenu;
                int n;
                int i;
                int bad;
                int opened;

                /* E, S and C re-launch MENU, except in an exit-only
                   sub-menu: a .dat with an M! directive and no S! entry */
                remenu = (type == MTYPE_E || type == MTYPE_S || type == MTYPE_C)
                         && !(has_back && !menu_has_snr);

                /* read and check the .sub file before touching $$$.SUB */
                n = 0;
                if (type == MTYPE_S || type == MTYPE_SNR) {
                    n = sub_load(items[sel].cmd);
                    if (n <= 0) {
                        menu_error(sub_errmsg(n), sub_name(), sel, has_back);
                        continue;
                    }
                }

                /* built here so it follows M sub-menu navigation */
                build_menucmd(menucmd, datfile, cmddrv, sel, items[sel].pause);

                if ((type == MTYPE_C || type == MTYPE_CNR) && !remenu) {
                    /* chain without coming back: inside a SUBMIT job the
                       rest of the job runs after it, otherwise drop any
                       stale $$$.SUB and leave MDSUBE alone */
                    if (subact != 1)
                        sub_delete();
                    clrscr();
                    cursor(CURSOR_ON);
                    p_chain(items[sel].cmd, subact == 1); /* no return */
                    return 0;
                }

                /* records are a stack: first written = bottom = runs last.
                   Inside a SUBMIT job push on top of what is left of it. */
                opened = (sub_open(subact == 1 ? SUB_APPEND : SUB_CREATE) == 0);
                bad = !opened;
                if (!bad && remenu)
                    bad = (sub_append(menucmd) != 0);
                if (!bad && (type == MTYPE_S || type == MTYPE_SNR)) {
                    bad = (sub_pushall() != 0);
                } else if (!bad && (type == MTYPE_E || type == MTYPE_ENR)) {
                    bad = (sub_append(items[sel].cmd) != 0);
                }
                if (!bad)
                    bad = (sub_close() != 0);
                if (bad) {
                    if (opened)
                        sub_abort();     /* outer job left as it was */
                    menu_error("Cannot write $$$.SUB", (char *)0, sel, has_back);
                    continue;
                }

                clrscr();
                cursor(CURSOR_ON);
                if (type == MTYPE_C)
                    p_chain(items[sel].cmd, 1);  /* no return on success */
                else
                    sub_exit();                  /* MDSUBE + BDOS 0, no return */
                return 0;
            }

        } else if ((c == 'q' || c == 'Q') && !menu_quit_disabled) {
            /* inside a SUBMIT job the CCP carries on with the rest of it */
            if (subact != 1)
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
