#include <stdio.h>
#include "conio.h"
#include "menudat.h"
#include "menuexec.h"
#include "menukey.h"

#include "menu.h"

static menu_item_t items[MAX_ENTRIES];
static int count;
static menu_entry_t choices[MAX_ENTRIES];
static menu_t selector;

static void app_puts(char *text)
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

static int choose(int sel, int has_back)
{
    menu_def_t def;
    int i, result;
    for (i = 0; i < count; i++) {
        choices[i].label = items[i].label;
        choices[i].id = i;
    }
    def.title = menu_title;
    def.entries = choices;
    def.count = count;
    def.flags = (menu_quit_disabled ? 0 : MENU_ALLOW_CANCEL) |
                (has_back ? MENU_ALLOW_BACK : 0);
    def.title_color = menu_title_fg;
    def.item_color = menu_item_fg;
    if (menu_init(&selector, &def) < 0) return MENU_ERROR;
    if (menu_select(&selector, sel) < 0) menu_select(&selector, 0);
    result = menu_choose(&selector);
    menu_close(&selector);
    return result;
}

static void menu_error(char *msg, char *arg)
{
    app_puts(msg);
    if (arg) app_puts(arg);
    app_puts(" -- press a key");
    getch();
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
    app_puts(b + i);
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
            app_puts("\r\nuser "); put_num(user);
            app_puts(": $$$.SUB records="); put_num(recs);
            app_puts(rc == 0xFF ? " not found" : " deleted");
        }
    }
    bdos(32, home);
}

int main(int argc, char **argv)
{
    int  sel;
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
        app_puts(error); app_puts("\r\n");
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
            app_puts("\r\nMENU reset: submit flag=");
            put_num(exec.subact < 0 ? 9 : exec.subact);
        }
        reset_stacks(reset == 2);
        exec.subact = 0;
        if (reset == 2) {
            app_puts("\r\nPress any key...");
            getch();
        }
    }

    if (pause) {
        cursor(CURSOR_ON);
        app_puts("\r\nPress any key to return to the menu...");
        getch();
    }

reload:
    sel = 0;

    count = load_menu(items, MAX_ENTRIES, datfile);
    if (count == 0) {
        menu_keys_leave();
        wrapline(WRAPLINE_ON);      /* may come from B after a draw */
        cursor(CURSOR_ON);
        app_puts("\r\nNo entries found in: "); app_puts(datfile); app_puts("\r\n");
        app_puts("Usage: MENU [datafile.dat] [[nPRV]]\r\n");
        return 1;
    }

    /* /n applies to the first load only */
    if (startsel >= 1 && startsel <= count)
        sel = startsel - 1;
    startsel = 0;

    /* has_back driven by M! directive in the loaded dat */
    has_back = (menu_back[0] != '\0') ? 1 : 0;


    for (;;) {
        c = choose(sel, has_back);
        if (c == MENU_ERROR) return 1;
        if (c >= 0) {
            sel = c;
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
                continue;
            }

            error = menu_exec_prepare(&exec, &items[sel], datfile, sel);
            if (error) {
                menu_error(error, exec.error_arg);
                continue;
            }
            result = menu_exec_run(&exec, &items[sel]);
            if (result == MEXEC_ERROR) {
                menu_error("Cannot launch command: ", exec.command);
                continue;
            }
            if (result == MEXEC_EXIT) return 0;
            if (items[sel].pause) {
                app_puts("\r\nPress any key to return to the menu...");
                getch();
            }

        } else if (c == MENU_CANCEL) {
            menu_exec_quit(&exec);
            return 0;

        } else if (c == MENU_BACK) {
            /* Back: reload dat named by M! directive */
            int i;
            for (i = 0; menu_back[i] != '\0' && i < 64; i++)
                datfile[i] = menu_back[i];
            datfile[i] = '\0';
            goto reload;
        }
    }
}
