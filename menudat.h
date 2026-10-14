#ifndef MENUDAT_H
#define MENUDAT_H

#define MAX_ENTRIES 15
#define MAX_LABEL   70
#define MAX_CMD     125  /* CCP command line limit */
#define MAX_TITLE   70

/* Entry types */
#define MTYPE_E     0   /* E  : run cmd, re-append MENU        */
#define MTYPE_ENR   1   /* E! : run cmd, no MENU re-append     */
#define MTYPE_S     2   /* S  : submit file, re-append MENU    */
#define MTYPE_SNR   3   /* S! : submit file, no MENU re-append */
#define MTYPE_M     4   /* M  : in-process sub-menu load       */
#define MTYPE_C     5   /* C  : chain to cmd, re-append MENU   */
#define MTYPE_CNR   6   /* C! : chain to cmd, no MENU re-append */

#define MAX_USER    15  /* highest user area for the @n suffix */
#define MAX_COLOR   15  /* highest colour for CTn / CIn */
#define DEF_COLOR   14  /* default title and item foreground */

/* Diagnostic levels for the MCHECK build of load_menu (see menudat.c) */
#define MDIAG_INF   0
#define MDIAG_WRN   1   /* accepted, but altered (cut, ignored directive) */
#define MDIAG_ERR   2   /* line not understood, skipped */

typedef struct {
    char label[71];
    char cmd[MAX_CMD + 1]; /* command / dat file / submit file+params */
    int  type;          /* MTYPE_*                                  */
    int  pause;         /* EP/SP/CP: wait for a key before the menu redraws */
    int  user;          /* @n suffix: run in user area n; -1 = none */
} MenuItem;

/*
 * All set/reset by load_menu() on every call.
 *
 * menu_title        : from T line; default if no T line found.
 * menu_quit_disabled: 1 if Q! line present, 0 otherwise.
 * menu_has_snr      : 1 if at least one S! entry found, 0 otherwise.
 * menu_back         : filename set by M! directive; empty string if absent.
 *                     If non-empty, the B key is shown and returns to this dat.
 * menu_title_fg     : foreground colour 0..15 from CTn; default 14.
 * menu_item_fg      : foreground colour 0..15 of unselected items from CIn;
 *                     default 14.
 */
extern char menu_title[MAX_TITLE + 1];
extern int  menu_title_fg;
extern int  menu_item_fg;
extern int  menu_quit_disabled;
extern int  menu_has_snr;
extern char menu_back[MAX_CMD + 1];

int load_menu(MenuItem *items, int max, char *filename);

#endif
