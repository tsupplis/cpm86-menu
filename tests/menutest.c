/* Host-side checks; run in a disposable directory. No CP/M files are touched. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
int bdos(int fn, ...);
/* Exercise the actual shared helper while retaining fault-injection stubs
   for MENU's other submit calls. The header snapshot is refreshed by hand. */
#include "../../cpm86-hacking/sub.h"
#define sub_open lib_sub_open
#define sub_append lib_sub_append
#define sub_close lib_sub_close
#define sub_records lib_sub_records
#define sub_abort lib_sub_abort
#define sub_delete lib_sub_delete
#include "../../cpm86-hacking/sub.c"
#undef sub_open
#undef sub_append
#undef sub_close
#undef sub_records
#undef sub_abort
#undef sub_delete
#define main menu_main
#ifndef MENU_SOURCE
#define MENU_SOURCE "../menu.c"
#endif
#include MENU_SOURCE
#undef main

char menu_title[MAX_TITLE+1] = "Test $ title";
char menu_back[MAX_CMD+1] = "";
int menu_title_fg = 14, menu_item_fg = 14, menu_quit_disabled, menu_has_snr;
static int user, keys, alt, home, base, active = 1;
static int fail_home_open, fail_home_append, fail_home_close, fail_restore;
static int console_calls, string_calls, used;
static unsigned char output[32768];
static char alternate[128][126];

static void require(int ok, char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static void emit(int c)
{
    require(used < sizeof(output), "capture buffer capacity");
    output[used++] = (unsigned char)c;
}
int bdos(int fn, ...)
{
    va_list ap;
    char *fcb;
    int n;
    va_start(ap, fn);
    if (fn == 12) { va_end(ap); return 0x22; }
    if (fn == 25) { va_end(ap); return 0; }
    if (fn == 32) {
        n = va_arg(ap, int);
        if (n == 255) { va_end(ap); return user; }
        user = n;
    } else if (fn == 9) {
        fcb = va_arg(ap, char *);
        console_calls++; string_calls++;
        while (*fcb != '$') emit(*fcb++);
    } else if (fn == 15 || fn == 16 || fn == 19) {
        fcb = va_arg(ap, char *);
        require(user == 4 && !memcmp(fcb + 1, "$$$     SUB", 11), "rollback addresses target stack");
        if (fn == 15) fcb[15] = (char)alt;
        else {
            if (fail_restore) { va_end(ap); return 255; }
            alt = fn == 19 ? 0 : (fcb[15] & 255);
        }
    }
    va_end(ap);
    return 0;
}
int load_menu(menu_item_t *a, int max, char *file)
{
    strcpy(a[0].label, "Erase"); strcpy(a[0].cmd, "ERA *.DAT");
    a[0].type = MTYPE_E; a[0].user = 4; a[0].pause = 0;
    return 1;
}
int getch(void) { int k = keys++; return k == 0 ? '\r' : (k == 1 ? 'x' : 'q'); }
int cputc(char c) { console_calls++; emit(c); return 0; }
int cputs(char *s) { while (*s) cputc(*s++); return 0; }
void clrscr(void) { cputs("\033E"); }
void cursor(int on) { cputs(on ? "\033e" : "\033f"); }
void wrapline(int on) { cputs(on ? "\033v" : "\033w"); }
void gotoxy(int row, int col) { cputs("\033Y"); cputc((char)(row+32)); cputc((char)(col+32)); }
int sub_active(void) { return active; }
int sub_cmddrv(void) { return 0; }
int sub_records(void) { return user == 4 ? alt : home; }
int sub_open(int flags)
{
    if (!user && fail_home_open) return -1;
    if (!flags) { if (user == 4) alt = 0; else home = 0; }
    base = user == 4 ? alt : home;
    return 0;
}
int sub_append(char *s)
{
    if (!user && fail_home_append) return -1;
    if (user == 4) strcpy(alternate[alt++], s); else home++;
    return 0;
}
int sub_close(void) { return !user && fail_home_close ? -1 : 0; }
int sub_abort(void) { if (user == 4) alt = base; else home = base; return 0; }
int sub_delete(void) { if (user == 4) alt = 0; else home = 0; return 0; }
int sub_load(char *s) { return -1; }
char *sub_name(void) { return ""; }
char *sub_errmsg(int n) { return ""; }
int sub_pushall(void) { return 0; }
void sub_exit(void) { }
void p_chain(char *s, int n) { }
static void setup(void)
{
    user = keys = used = console_calls = string_calls = 0;
    alt = home = active = 1;
    fail_home_open = fail_home_append = fail_home_close = fail_restore = 0;
    strcpy(alternate[0], "ORIGINAL");
}
static int contains(char *text)
{
    int i, n = strlen(text);
    for (i = 0; i + n <= used; i++) if (!memcmp(output+i, text, n)) return 1;
    return 0;
}
int main(int argc, char **argv)
{
    char *args[1];
    FILE *fp;
    int i;
    args[0] = "menu";
    setup();
    if (argc > 1) {
        count = 15;
        for (i = 0; i < count; i++) strcpy(items[i].label, "A $ label");
        draw_screen(0, 1);
        printf("initial_console_calls=%d initial_bytes=%d\n", console_calls, used);
        draw_entry(0, 0); draw_entry(1, 1);
        fp = fopen(argv[1], "wb");
        require(fp != NULL, "open screen capture");
        fwrite(output, 1, used, fp); fclose(fp);
        printf("console_calls=%d string_calls=%d bytes=%d\n", console_calls, string_calls, used);
        return 0;
    }
    require(sub_restore(-1) == -1 && sub_restore(129) == -1,
            "invalid record counts rejected without BDOS calls");
    for (i = 0; i < 3; i++) {
        setup();
        if (i == 0) fail_home_open = 1;
        if (i == 1) fail_home_append = 1;
        if (i == 2) fail_home_close = 1;
        menu_main(1, args);
        require(alt == 1 && home == 1 && user == 0, "both stacks restored after home write failure");
        require(!strcmp(alternate[0], "ORIGINAL"), "original target command preserved");
        require(contains("Cannot write $$$.SUB"), "literal dollars retained in error message");
    }
    setup(); alt = 0; fail_home_open = 1;
    menu_main(1, args);
    require(!alt && user == 0, "new target stack removed on rollback");
    setup(); fail_home_open = fail_restore = 1;
    menu_main(1, args);
    require(user == 0 && contains("Submit rollback failed"), "rollback failure reported after restoring home user");
    puts("PASS: cross-user rollback, original records, new stack cleanup and rollback errors");
    return 0;
}
