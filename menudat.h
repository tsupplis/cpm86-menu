#ifndef MENUDAT_H
#define MENUDAT_H

#define MAX_ENTRIES 15
#define MAX_LABEL   70
#define MAX_CMD     64
#define MAX_TITLE   70

/* Entry types */
#define MTYPE_E     0   /* E  : run cmd, re-append MENU        */
#define MTYPE_ENR   1   /* E! : run cmd, no MENU re-append     */
#define MTYPE_S     2   /* S  : submit file, re-append MENU    */
#define MTYPE_SNR   3   /* S! : submit file, no MENU re-append */
#define MTYPE_M     4   /* M  : in-process sub-menu load       */
#define MTYPE_C     5   /* C  : chain to cmd, re-append MENU   */
#define MTYPE_CNR   6   /* C! : chain to cmd, no MENU re-append */

typedef struct {
    char label[71];
    char cmd[65];       /* command / dat file / submit file+params */
    int  type;          /* MTYPE_*                                  */
} MenuItem;

/*
 * All set/reset by load_menu() on every call.
 *
 * menu_title        : from T line; default if no T line found.
 * menu_quit_disabled: 1 if Q! line present, 0 otherwise.
 * menu_has_snr      : 1 if at least one S! entry found, 0 otherwise.
 * menu_back         : filename set by M! directive; empty string if absent.
 *                     If non-empty, the B key is shown and returns to this dat.
 */
extern char menu_title[MAX_TITLE + 1];
extern int  menu_quit_disabled;
extern int  menu_has_snr;
extern char menu_back[MAX_CMD + 1];

int load_menu(MenuItem *items, int max, char *filename);

#endif
