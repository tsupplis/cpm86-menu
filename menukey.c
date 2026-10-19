/* MENU keyboard handling; all state belongs to the resident frontend. */
#include "conio.h"
#include "menukey.h"

static int concurrent_keys;
static int raw_keys;

void menu_keys_init(int concurrent)
{
    concurrent_keys = concurrent;
    raw_keys = 0;
}

void menu_keys_enter(void)
{
    if (concurrent_keys && !raw_keys) {
        cputc((char)27); cputc((char)'6');
        raw_keys = 1;
    }
}

void menu_keys_leave(void)
{
    if (raw_keys) {
        cputc((char)27); cputc((char)'7');
        raw_keys = 0;
    }
}

int menu_key_read(void)
{
    int c;

    c = getch() & 0xFF;
    /* XIOS pfk_code_tbl: Up = 'H' | 80h; Down = 'P' | 80h.
       Mask getch's result because buffered chars may be sign-extended. */
    if (concurrent_keys) {
        if (c == 0xC8) return 'k';
        if (c == 0xD0) return 'j';
    }
    if (c == 27) {
        c = getch() & 0xFF;
        if (c == 'A') return 'k';
        if (c == 'B') return 'j';
        return 0;               /* ignore unsupported escape sequences */
    }
    return c;
}
