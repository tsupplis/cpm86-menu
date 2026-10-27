#include <stdio.h>
#include "conio.h"
#include "menudat.h"
#include "menuexec.h"
#include "menukey.h"

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
 *  Row 22 : | [Up/'K'] ... [Q] Quit |   footer / messages, inside the box
 *  Row 23 : c----[78 dashes]----d       content box bottom
 *
 * Each row: col 0 = '|', cols 1..78 = content (78 chars), col 79 = '|'
 * Total chars per row = 80. Writing col 79 of row 23 would wrap and scroll
 * the screen, so line wrap is off (ESC w) while the menu is shown and back
 * on (ESC v) whenever the menu leaves the screen (leave_screen).
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
#define FOOTER_ROW    22  /* |   |  footer / messages   */
#define BOX_BOT_ROW   23  /* c---d  content box bottom */

static char CH_WALL  = (char)179;
static char CH_DASH  = (char)196;
static char CH_EQUAL = (char)205;
static char CH_EQL   = (char)198;
static char CH_EQR   = (char)181;
static char CH_SPACE = ' ';
static char CH_TL    = (char)218;   /* top-left corner */
static char CH_TR    = (char)191;   /* top-right corner */
static char CH_BL    = (char)192;   /* bottom-left corner */
static char CH_BR    = (char)217;   /* bottom-right corner */

static menu_item_t items[MAX_ENTRIES];
static int      count = 0;

/* ------------------------------------------------------------------ */

/* BDOS 9 batches text; conio cputs otherwise calls BDOS once per byte.
   Split at literal '$' so labels and error messages retain their text. */
static void screen_puts(char *text)
{
    static char out[128];
    int n;
    while (*text) {
        n = 0;
        while (*text && *text != '$' && n < sizeof(out) - 1)
            out[n++] = *text++;
        if (n) {
            out[n] = '$';
            bdos(9, out);
        }
        if (*text == '$') {
            cputc((char)'$');
            text++;
        }
    }
}

static void setfg(int fg)
{
    cputc((char)27); cputc((char)'b'); cputc((char)fg);
}

static void setbg(int bg)
{
    cputc((char)27); cputc((char)'c'); cputc((char)bg);
}

/* ------------------------------------------------------------------ */
/* Rows are built in rowbuf and sent with one screen_puts (BDOS 9) per row,
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
    screen_puts(rowbuf);
}

static void draw_blank_row(int row)
{
    draw_hline(row, CH_SPACE, CH_WALL, CH_WALL);
}

/* "| text<pad>|" on row, text cut to fit the box */
static void draw_textrow(int row, char *text)
{
    int k;

    gotoxy(row, 0);
    k = 0;
    rowbuf[k++] = CH_WALL;
    rowbuf[k++] = CH_SPACE;
    while (*text != '\0' && k < INNER_WIDTH + 1)
        rowbuf[k++] = *text++;
    while (k < INNER_WIDTH + 1)
        rowbuf[k++] = CH_SPACE;
    rowbuf[k++] = CH_WALL;
    rowbuf[k] = '\0';
    screen_puts(rowbuf);
}

/* append src to dst, dst holding at most max chars */
static void sappend(char *dst, char *src, int max)
{
    int n;

    for (n = 0; dst[n] != '\0'; n++) ;
    while (*src != '\0' && n < max)
        dst[n++] = *src++;
    dst[n] = '\0';
}

/* hand the screen back: clear, cursor on, line wrap on again */
static void leave_screen()
{
    menu_keys_leave();
    clrscr();
    cursor(CURSOR_ON);
    wrapline(WRAPLINE_ON);
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
    else          { setfg(menu_item_fg); setbg(0); }

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
        rowbuf[k++] = CH_SPACE;             /* pad so "." aligns with 10+ */
        rowbuf[k++] = (char)('0' + n);
        col += 2;
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
    screen_puts(rowbuf);

    setfg(7); setbg(0);
    cputc(CH_SPACE);                        /* 1 normal space after highlight */
    cputc(CH_WALL);
}

/* ------------------------------------------------------------------ */
/* Compute length of a C string                                         */
static int slen(char *p)
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
    char foot[INNER_WIDTH + 1];

    menu_keys_enter();
    clrscr();
    cursor(CURSOR_OFF);
    wrapline(WRAPLINE_OFF);     /* row 23 col 79 must not scroll */
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
    screen_puts(rowbuf);
    setfg(menu_title_fg);
    screen_puts(menu_title);
    setfg(7);
    i = pad + tlen;
    k = 0;
    while (i < INNER_WIDTH) { rowbuf[k++] = CH_SPACE; i++; }
    rowbuf[k++] = CH_WALL;
    rowbuf[k] = '\0';
    screen_puts(rowbuf);

    /* Row 2: b===d  title box bottom */
    draw_hline(SEP1_ROW, CH_EQUAL, CH_EQL, CH_EQR);

    /* Row 3: blank */
    draw_blank_row(BLANK1_ROW);

    /* Rows 4..N: entries + blank filler */
    for (i = 0; i < count; i++)
        draw_entry(i, i == sel);
    for (row = FIRST_ROW + count; row < FOOTER_ROW; row++)
        draw_blank_row(row);

    /* Row 22: | footer | */
    foot[0] = '\0';
    sappend(foot, "[Up/'K'] Previous  [Down/'J'] Next  [Enter] Launch", INNER_WIDTH);
    if (!menu_quit_disabled)
        sappend(foot, "  [Q] Quit", INNER_WIDTH);
    if (has_back)
        sappend(foot, "  [B] Back", INNER_WIDTH);
    draw_textrow(FOOTER_ROW, foot);

    /* Row 23: c---d  content box bottom */
    draw_hline(BOX_BOT_ROW, CH_DASH, CH_BL, CH_BR);
}

/* ------------------------------------------------------------------ */
/* Show an error on the footer row, wait for a key, redraw the menu.    */
static void menu_error(char *msg, char *arg, int sel, int has_back)
{
    char text[INNER_WIDTH + 1];

    draw_screen(sel, has_back);
    text[0] = '\0';
    sappend(text, msg, INNER_WIDTH);
    if (arg != 0) sappend(text, arg, INNER_WIDTH);
    sappend(text, " -- press a key", INNER_WIDTH);
    draw_textrow(FOOTER_ROW, text);
    getch();
    draw_screen(sel, has_back);
}

/* ------------------------------------------------------------------ */

static void put_num(int n)
{
    char b[8];
    int i;

    i = 7;
    b[i] = '\0';
    do {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);
    screen_puts(b + i);
}

/* BDOS 19 on "$$$     SUB" (current drive) in each user area 0-15;
   verbose [V]: report records found and the delete result for each user */
static void reset_stacks(int verbose)
{
    static char fcb[36];
    int home;
    int user;
    int i;
    int recs;
    int rc;

    home = bdos(32, 0xFF) & 0xFF;
    for (user = 0; user < 16; user++) {
        for (i = 0; i < 36; i++)
            fcb[i] = 0;
        for (i = 1; i <= 11; i++)
            fcb[i] = ' ';
        fcb[1] = fcb[2] = fcb[3] = '$';
        fcb[9] = 'S'; fcb[10] = 'U'; fcb[11] = 'B';
        bdos(32, user);
        recs = sub_records();
        rc = bdos(19, fcb) & 0xFF;
        if (verbose && (recs > 0 || rc != 0xFF)) {
            screen_puts("\r\nuser "); put_num(user);
            screen_puts(": $$$.SUB records="); put_num(recs);
            screen_puts(rc == 0xFF ? " not found" : " deleted");
        }
    }
    bdos(32, home);
}

int main(int argc, char **argv)
{
    int  sel;
    int  prev;
    int  c;
    int  has_back;
    char datfile[65];
    int  startsel;      /* /n: entry to select on the first load, 0 = none */
    int  pause;         /* /P: wait for a key before drawing */
    int  reset;         /* [R]: delete $$$.SUB before starting, 2 = verbose [RV] */
    menu_exec_t exec;
    char *error;
    int result;
    int  type;

    error = menu_exec_init(&exec);
    if (error) {
        screen_puts(error); screen_puts("\r\n");
        return 1;
    }

    menu_keys_init(exec.mode == MEXEC_CLI);

    /* MENU [datfile] [[nPR]]: n = start entry, P = pause, R = reset */
    datfile[0]='m'; datfile[1]='e'; datfile[2]='n';
    datfile[3]='u'; datfile[4]='.'; datfile[5]='d';
    datfile[6]='a'; datfile[7]='t'; datfile[8]='\0';
    startsel = 0;
    pause    = 0;
    reset    = 0;
    {
        int a;
        int i;
        char *p;
        for (a = 1; a < argc; a++) {
            p = argv[a];
            if (p[0] == '[') {
                /* CP/M options: [nPR] or [nPR, the ] is optional */
                for (i = 1; p[i] != '\0' && p[i] != ']'; i++) {
                    if (p[i] == 'P' || p[i] == 'p') {
                        pause = 1;
                    } else if (p[i] == 'R' || p[i] == 'r') {
                        if (!reset) reset = 1;
                    } else if (p[i] == 'V' || p[i] == 'v') {
                        reset = 2;
                    } else if (p[i] >= '0' && p[i] <= '9') {
                        if (i == 1 || p[i-1] < '0' || p[i-1] > '9')
                            startsel = 0;
                        if (startsel < 1000)    /* no int wrap-around */
                            startsel = startsel * 10 + (p[i] - '0');
                    }
                }
            } else {
                for (i = 0; p[i] != '\0' && i < 64; i++)
                    datfile[i] = p[i];
                datfile[i] = '\0';
            }
        }
    }

    /* [R]: drop $$$.SUB in every user area (0-15) of the current drive,
       even the rest of an outer job, and start as from the keyboard */
    if (reset && exec.mode == MEXEC_SUB) {
        if (reset == 2) {
            screen_puts("\r\nMENU reset: submit flag=");
            put_num(exec.subact < 0 ? 9 : exec.subact);
        }
        reset_stacks(reset == 2);
        exec.subact = 0;
        if (reset == 2) {
            screen_puts("\r\nPress any key...");
            getch();
        }
    }

    if (pause) {
        cursor(CURSOR_ON);
        screen_puts("\r\nPress any key to return to the menu...");
        getch();
    }

reload:
    sel = 0;

    count = load_menu(items, MAX_ENTRIES, datfile);
    if (count == 0) {
        menu_keys_leave();
        wrapline(WRAPLINE_ON);      /* may come from B after a draw */
        cursor(CURSOR_ON);
        screen_puts("\r\nNo entries found in: "); screen_puts(datfile); screen_puts("\r\n");
        screen_puts("Usage: MENU [datafile.dat] [[nPRV]]\r\n");
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
        c = menu_key_read();

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

            error = menu_exec_prepare(&exec, &items[sel], datfile, sel);
            if (error) {
                menu_error(error, exec.error_arg, sel, has_back);
                continue;
            }
            leave_screen();
            result = menu_exec_run(&exec, &items[sel]);
            if (result == MEXEC_ERROR) {
                menu_error("Cannot launch command: ", exec.command, sel, has_back);
                continue;
            }
            if (result == MEXEC_EXIT) return 0;
            if (items[sel].pause) {
                screen_puts("\r\nPress any key to return to the menu...");
                getch();
            }
            draw_screen(sel, has_back);

        } else if ((c == 'q' || c == 'Q') && !menu_quit_disabled) {
            menu_exec_quit(&exec);
            leave_screen();
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
