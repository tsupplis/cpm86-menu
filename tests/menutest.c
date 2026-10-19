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
#define getpriority menu_getpriority
#include "../menuexec.c"
#include "../menusub.c"
#include "../menuexe.c"
#include "../menukey.c"

char menu_title[MAX_TITLE+1] = "Test $ title";
char menu_back[MAX_CMD+1] = "";
int menu_title_fg = 14, menu_item_fg = 14, menu_quit_disabled, menu_has_snr;
static int user, keys, alt, home, base, active = 1;
static int fail_home_open, fail_home_append, fail_home_close, fail_restore;
static int console_calls, string_calls, used;
static unsigned char output[32768];
static char alternate[128][126];
static char home_records[128][126];
static int version = 0x22, system_type = 0x14;
static int entry_type = MTYPE_E, entry_user = 4, entry_pause;
static char *entry_cmd = "ERA *.DAT";
static char *script;
static int scriptpos, loads, launches, attaches, launch_rc, priority = 200;
static int parent_priority = 198, launch_user, launch_priority, drive;
static int chained, exited, chainmode, loaded_lines = -1;
static char launched[128], events[128];
static int event_count, navigation, missing_menu;
static void keyboard_tests(void);
static void require(int ok, char *message);
static void concurrent_tests(void);
static void legacy_success_tests(void);
static void event(char c) { events[event_count++] = c; events[event_count] = 0; }
static void legacy_only(void) { require(version == 0x22, "concurrent never accesses CCP submit state"); }


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
    if (fn == 12) { va_end(ap); return version; }
    if (fn == 25) { va_end(ap); return drive; }
    if (fn == 14) { drive = va_arg(ap, int); event('D'); }
    if (fn == 32) {
        n = va_arg(ap, int);
        if (n == 255) { va_end(ap); return user; }
        user = n;
        if (version != 0x22) event('U');
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
    loads++;
    if (missing_menu && (missing_menu == 1 || loads == 3)) return 0;
    if (navigation) {
        menu_back[0] = 0;
        strcpy(a[0].label, "First"); strcpy(a[0].cmd, "FIRST");
        a[0].type = MTYPE_E; a[0].user = -1; a[0].pause = 0;
        a[1] = a[0]; strcpy(a[1].cmd, "SECOND");
        if (!strcmp(file, "menu.dat")) {
            a[0].type = MTYPE_M; strcpy(a[0].cmd, "child.dat");
        } else {
            require(!strcmp(file, "child.dat"), "navigation remains on current dat");
            strcpy(menu_back, "menu.dat");
        }
        return 2;
    }
    strcpy(a[0].label, "Erase"); strcpy(a[0].cmd, entry_cmd);
    a[0].type = entry_type; a[0].user = entry_user; a[0].pause = entry_pause;
    return 1;
}
int getch(void)
{
    int k;
    if (script) {
        require(script[scriptpos] != 0, "UI consumed only expected keys");
        return script[scriptpos++];
    }
    k = keys++;
    return k == 0 ? '\r' : (k == 1 ? 'x' : 'q');
}
int cputc(char c) { console_calls++; emit(c); return 0; }
int cputs(char *s) { while (*s) cputc(*s++); return 0; }
void clrscr(void) { cputs("\033E"); }
void cursor(int on) { cputs(on ? "\033e" : "\033f"); }
void wrapline(int on) { cputs(on ? "\033v" : "\033w"); }
void gotoxy(int row, int col) { cputs("\033Y"); cputc((char)(row+32)); cputc((char)(col+32)); }
int sub_active(void) { legacy_only(); return active; }
int sub_cmddrv(void) { legacy_only(); return 0; }
int sub_records(void) { legacy_only(); return user == 4 ? alt : home; }
int sub_open(int flags)
{
    legacy_only();
    if (!user && fail_home_open) return -1;
    if (!flags) { if (user == 4) alt = 0; else home = 0; }
    base = user == 4 ? alt : home;
    return 0;
}
int sub_append(char *s)
{
    legacy_only();
    if (!user && fail_home_append) return -1;
    if (user == 4) strcpy(alternate[alt++], s); else strcpy(home_records[home++], s);
    return 0;
}
int sub_close(void) { return !user && fail_home_close ? -1 : 0; }
int sub_abort(void) { if (user == 4) alt = base; else home = base; return 0; }
int sub_delete(void) { legacy_only(); if (user == 4) alt = 0; else home = 0; return 0; }
int sub_load(char *s) { legacy_only(); return loaded_lines; }
char *sub_name(void) { return ""; }
char *sub_errmsg(int n) { return ""; }
int sub_pushall(void) { legacy_only(); sub_append("SECOND"); sub_append("FIRST"); return 0; }
void sub_exit(void) { legacy_only(); exited++; }
void p_chain(char *s, int n) { legacy_only(); chained++; chainmode = n; }
static void setup(void)
{
    menu_keys_init(0); missing_menu = 0;
    version = 0x22; system_type = 0x14;
    entry_type = MTYPE_E; entry_user = 4; entry_pause = 0;
    entry_cmd = "ERA *.DAT"; script = 0; scriptpos = loads = 0;
    launches = attaches = launch_rc = drive = event_count = 0;
    events[0] = 0; navigation = 0; menu_back[0] = 0; priority = 200; parent_priority = 198;
    chained = exited = chainmode = 0; loaded_lines = -1;
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
    legacy_success_tests();
    concurrent_tests();
    keyboard_tests();
    return 0;
}

/* The same frontend and dispatch run with mocked concurrent BDOS calls. */
int ostype(void) { require(version != 0x22, "legacy does not inspect concurrent system type"); return system_type; }
int getpriority(void) { require(version != 0x22, "legacy does not read process priority"); return priority; }
int getppriority(void) { require(version != 0x22, "legacy does not read parent priority"); return parent_priority; }
int p_priority(int p) { event('P'); priority = p; return 0; }
int p_cli(char *s)
{
    require(version >= 0x30, "P_CLI only on concurrent backend");
    require(!raw_keys, "normal key expansion restored before P_CLI");
    event('L'); launches++; strcpy(launched, s);
    launch_user = user; launch_priority = priority;
    /* Simulate USER / $GLOBAL changing the parent's context, even on error. */
    user = 9; drive = 3;
    return launch_rc;
}
int conattach(void) { event('A'); attaches++; return 0; }

static void legacy_success_tests(void)
{
    menu_exec_t ctx;
    menu_item_t item;
    int t;
    for (t = MTYPE_E; t <= MTYPE_CNR; t++) {
        if (t == MTYPE_M) continue;
        setup(); entry_type = t; entry_user = -1; loaded_lines = 2;
        require(menu_exec_init(&ctx) == 0, "legacy init");
        load_menu(&item, 1, "menu.dat");
        require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) == 0, "legacy prepare");
        menu_exec_run(&ctx, &item);
        if (t == MTYPE_CNR) require(home == 1 && chained == 1 && chainmode == 1, "C! preserves enclosing job");
        if (t == MTYPE_C) require(home == 2 && chained == 1 && chainmode == 1, "C queues MENU before chain");
        if (t == MTYPE_E) require(home == 3 && !strcmp(home_records[2], "ERA *.DAT"), "E LIFO command");
        if (t == MTYPE_ENR) require(home == 2 && exited == 1, "E! has no restart");
        if (t == MTYPE_S) require(home == 4 && !strcmp(home_records[2], "SECOND") && !strcmp(home_records[3], "FIRST"), "S LIFO expansion");
        if (t == MTYPE_SNR) require(home == 3 && exited == 1, "S! has no restart");
        if (t == MTYPE_E || t == MTYPE_S || t == MTYPE_C)
            require(!strcmp(home_records[1], "MENU A:menu.dat /1"), "legacy restart command");
    }
    setup(); require(menu_exec_init(&ctx) == 0, "hop init");
    load_menu(&item, 1, "menu.dat");
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) == 0, "hop prepare");
    require(user == 0 && alt == 3 && home == 4, "both hop stacks prepared");
    require(!strcmp(alternate[1], "USER 0") && !strcmp(alternate[2], "ERA *.DAT"), "hop target stack");
    require(!strcmp(home_records[2], "MCLEAN 4") && !strcmp(home_records[3], "USER 4"), "hop relay stack");
    setup(); active = 0; require(menu_exec_init(&ctx) == 0, "keyboard init");
    menu_exec_quit(&ctx); require(home == 0, "keyboard quit removes stale stack");
    puts("PASS: legacy E/S/C, ! variants, restart commands, LIFO and user-hop stacks");
}

static void concurrent_tests(void)
{
    menu_exec_t ctx;
    menu_item_t item;
    char *args[1];
    char longcmd[126];
    int t;
    args[0] = "menu";
    for (t = MTYPE_E; t <= MTYPE_CNR; t++) {
        if (t == MTYPE_M) continue;
        setup(); version = 0x31; entry_type = t;
        script = (t == MTYPE_ENR || t == MTYPE_SNR || t == MTYPE_CNR) ? "\r" : "\r\rq";
        require(menu_main(1, args) == 0, "concurrent frontend returns cleanly");
        require(!raw_keys, "keyboard restored on normal and bang exits");
        require(loads == 1, "resident menu never restarts or reloads after action");
        require(launches == (script[1] ? 2 : 1) && attaches == launches, "repeat actions and bang exit");
        require(!strcmp(launched, (t == MTYPE_S || t == MTYPE_SNR) ? "SUBMIT ERA *.DAT" : "ERA *.DAT"), "only S uses SUBMIT");
        require(user == 0 && drive == 0 && priority == 200 && launch_user == 4 && launch_priority == 197, "launch context and restoration");
        require(alt == 1 && home == 1, "concurrent leaves stale submit stacks untouched");
    }
    setup(); version = 0x31; entry_pause = 1; script = "\rxq";
    menu_main(1, args);
    require(contains("Press any key") && launches == 1, "pause after concurrent action");
    require(!strcmp(events, "UPLAPUD"), "priority before launch, attach before restoration");
    setup(); version = 0x31; entry_type = MTYPE_ENR; entry_user = -1;
    launch_rc = 255; script = "\rxq";
    menu_main(1, args);
    require(launches == 1 && attaches == 0 && contains("Cannot launch command"), "failed bang launch stays in menu");
    require(user == 0 && drive == 0 && priority == 200, "failure restores context without suffix");
    setup(); version = 0x31; parent_priority = 197; priority = 199;
    require(menu_exec_init(&ctx) == 0, "nested init");
    load_menu(&item, 1, "menu.dat");
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) == 0, "nested prepare");
    menu_exec_run(&ctx, &item);
    require(launch_priority == 196 && priority == 199, "nested parent-relative priority and exact restore");
    parent_priority = 220;
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) == 0, "low-priority parent prepare");
    require(ctx.wait_priority == 197, "recovery priority also runs ahead of TMP");
    setup(); version = 0x31; navigation = 1; script = "\rj\r\rbq";
    menu_main(1, args);
    require(launches == 2 && !strcmp(launched, "SECOND") && loads == 3,
            "M, selected submenu entry, repeated resident action and Back");
    navigation = 0;
    memset(longcmd, 'X', 125); longcmd[125] = 0;
    entry_cmd = longcmd; entry_type = MTYPE_S; load_menu(&item, 1, "menu.dat");
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) != 0, "SUBMIT prefix overflow rejected");
    entry_type = MTYPE_E; load_menu(&item, 1, "menu.dat");
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) == 0, "full length direct command accepted");
    parent_priority = 0;
    require(menu_exec_prepare(&ctx, &item, "menu.dat", 0) != 0, "unsafe priority rejected");
    setup(); version = 0x41; system_type = 0x10;
    require(menu_exec_init(&ctx) != 0, "later non-concurrent system rejected");
    version = 0x23; system_type = 0x14;
    require(menu_exec_init(&ctx) != 0, "pre-3.0 system rejected");
    puts("PASS: resident P_CLI E/S/C, repeat/! paths, pauses, failures, context, nesting and bounds");
}

static void keyboard_tests(void)
{
    char *args[1];
    args[0] = "menu";

    setup(); menu_keys_init(1); menu_keys_enter();
    require(used == 2 && output[0] == 27 && output[1] == '6', "ESC 6 selects raw function keys");
    menu_keys_enter(); require(used == 2, "raw selection is idempotent");
    script = "\310\320\033A\033Bq";
    require(menu_key_read() == 'k' && menu_key_read() == 'j', "raw concurrent arrows, including signed bytes");
    require(menu_key_read() == 'k' && menu_key_read() == 'j', "VT52 arrows still work on concurrent systems");
    menu_keys_leave(); menu_keys_leave();
    require(used == 4 && output[2] == 27 && output[3] == '7', "ESC 7 restores expansion once");

    setup(); menu_keys_init(0); menu_keys_enter(); menu_keys_leave();
    require(used == 0, "legacy emits no function-key mode sequences");
    script = "\033A\033B";
    require(menu_key_read() == 'k' && menu_key_read() == 'j', "legacy VT52 arrows preserved");

    setup(); version = 0x31; navigation = 1;
    script = "\r\320\310\320\r\310\320\rq";
    menu_main(1, args);
    require(launches == 2 && !strcmp(launched, "SECOND"), "arrows select correct submenu item before and after action");
    require(!raw_keys && contains("\0336") && contains("\0337"), "keyboard mode restored on quit");

    setup(); version = 0x31; launch_rc = 255; script = "\rxq";
    menu_main(1, args);
    require(!raw_keys, "keyboard restored after launch failure then quit");
    setup(); version = 0x31; missing_menu = 1;
    require(menu_main(1, args) == 1 && !raw_keys, "missing menu exits without raw keyboard mode");
    setup(); version = 0x31; navigation = 1; missing_menu = 2; script = "\rb";
    require(menu_main(1, args) == 1 && !raw_keys, "failed Back reload restores active keyboard mode");
    puts("PASS: concurrent arrows, VT52 compatibility, keyboard handover and exit restoration");
}
