#include <stdio.h>
#include "conio.h"
#include "menu.h"
#include "menukey.h"
extern int ostype(void);

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



/* ------------------------------------------------------------------ */

/* BDOS 9 batches text; conio cputs otherwise calls BDOS once per byte.
   Split at literal '$' so labels and error messages retain their text. */
static void screen_puts(const char *text)
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

static void draw_entry(menu_t *m, int idx, int selected)
{
    int n;
    int k;
    int col;
    const char *p;

    gotoxy(FIRST_ROW + idx, 0);
    cputc(CH_WALL);
    cputc(CH_SPACE);                        /* 1 normal space before highlight */

    if (selected) { setfg(0);  setbg(7); }
    else          { setfg(m->_def.item_color); setbg(0); }

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

    for (p = m->_def.entries[idx].label; *p != '\0' && col < INNER_WIDTH - 2; p++) {
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
static int slen(const char *p)
{
    int n;
    for (n = 0; p[n] != '\0'; n++) ;
    return n;
}

/* ------------------------------------------------------------------ */
/* has_back: 1 = show [B] Back in footer; 0 = don't                    */
static void draw_screen(menu_t *m, int sel, int has_back)
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
    tlen = slen(m->_def.title);
    gotoxy(TITLE_ROW, 0);
    pad = (INNER_WIDTH - tlen) / 2;
    rowbuf[0] = CH_WALL;
    for (i = 1; i <= pad; i++) rowbuf[i] = CH_SPACE;
    rowbuf[i] = '\0';
    screen_puts(rowbuf);
    setfg(m->_def.title_color);
    screen_puts(m->_def.title);
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
    for (i = 0; i < m->_def.count; i++)
        draw_entry(m, i, i == sel);
    for (row = FIRST_ROW + m->_def.count; row < FOOTER_ROW; row++)
        draw_blank_row(row);

    /* Row 22: | footer | */
    foot[0] = '\0';
    sappend(foot, "[Up/'K'] Previous  [Down/'J'] Next  [Enter] Launch", INNER_WIDTH);
    if ((m->_def.flags & MENU_ALLOW_CANCEL))
        sappend(foot, "  [Q] Quit", INNER_WIDTH);
    if (has_back)
        sappend(foot, "  [B] Back", INNER_WIDTH);
    draw_textrow(FOOTER_ROW, foot);

    /* Row 23: c---d  content box bottom */
    draw_hline(BOX_BOT_ROW, CH_DASH, CH_BL, CH_BR);
}


int menu_init(menu_t *m, const menu_def_t *def)
{
    unsigned i;
    int version;
    if (!m) return MENU_ERROR;
    m->_valid = 0;
    if (!def || !def->title || !def->entries || !def->count ||
        def->count > 15 || slen(def->title) > 70 ||
        def->title_color > 15 || def->item_color > 15 ||
        (def->flags & ~(MENU_ALLOW_CANCEL | MENU_ALLOW_BACK))) return MENU_ERROR;
    for (i = 0; i < def->count; i++)
        if (!def->entries[i].label || def->entries[i].id < 0) return MENU_ERROR;
    version = bdos(12, 0) & 255;
    m->_concurrent = version >= 0x30 && (ostype() & 0x05);
    if (version != 0x22 && !m->_concurrent) return MENU_ERROR;
    m->_def = *def;
    m->_selected = 0;
    m->_valid = 1;
    return 0;
}

int menu_select(menu_t *m, unsigned index)
{
    if (!m || !m->_valid || index >= m->_def.count) return MENU_ERROR;
    m->_selected = index;
    return 0;
}

int menu_choose(menu_t *m)
{
    int c, old, result;
    if (!m || !m->_valid) return MENU_ERROR;
    menu_keys_init(m->_concurrent);
    draw_screen(m, m->_selected, (m->_def.flags & MENU_ALLOW_BACK) != 0);
    for (;;) {
        c = menu_key_read();
        old = m->_selected;
        if (c == 'k' || c == 'K') {
            if (m->_selected) m->_selected--;
        } else if (c == 'j' || c == 'J') {
            if (m->_selected + 1 < m->_def.count) m->_selected++;
        } else if (c == '\r' || c == '\n') {
            result = m->_def.entries[m->_selected].id;
            break;
        } else if ((c == 'q' || c == 'Q') && (m->_def.flags & MENU_ALLOW_CANCEL)) {
            result = MENU_CANCEL;
            break;
        } else if ((c == 'b' || c == 'B') && (m->_def.flags & MENU_ALLOW_BACK)) {
            result = MENU_BACK;
            break;
        }
        if (old != m->_selected) {
            draw_entry(m, old, 0);
            draw_entry(m, m->_selected, 1);
        }
    }
    leave_screen();
    return result;
}

void menu_close(menu_t *m)
{
    if (m) m->_valid = 0;
}
