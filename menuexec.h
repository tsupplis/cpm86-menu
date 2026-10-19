#ifndef MENUEXEC_H
#define MENUEXEC_H

#include "menudat.h"

#define MEXEC_SUB     0
#define MEXEC_CLI     1
#define MEXEC_ERROR  -1
#define MEXEC_RESUME  0
#define MEXEC_EXIT    1
#define MEXEC_CMDLEN 127

typedef struct {
    int mode;
    int cmddrv;
    int subact;
    int priority;
    int wait_priority;
    char command[MEXEC_CMDLEN + 1];
    char *error_arg;
} menu_exec_t;

/* Prepare validates/builds the action before the UI releases the screen.
   Run returns only on concurrent systems (or failed legacy handover).
   No screen I/O is performed by either backend. */
char *menu_exec_init(menu_exec_t *ctx);
char *menu_exec_prepare(menu_exec_t *ctx, menu_item_t *item, char *datfile, int sel);
int menu_exec_run(menu_exec_t *ctx, menu_item_t *item);
void menu_exec_quit(menu_exec_t *ctx);

/* Backend entry points, used by the dispatcher. */
char *menusub_prepare(menu_exec_t *ctx, menu_item_t *item, char *datfile, int sel);
void menusub_run(menu_exec_t *ctx, menu_item_t *item);
char *menuexe_prepare(menu_exec_t *ctx, menu_item_t *item);
int menuexe_run(menu_exec_t *ctx, menu_item_t *item);

#endif
