#ifndef MENU_H
#define MENU_H

#define MENU_CANCEL (-1)
#define MENU_BACK   (-2)
#define MENU_ERROR  (-3)
#define MENU_ALLOW_CANCEL 1
#define MENU_ALLOW_BACK   2

typedef struct {
    const char *label;
    int id;
} menu_entry_t;

typedef struct {
    const char *title;
    const menu_entry_t *entries;
    unsigned count, flags, title_color, item_color;
} menu_def_t;

/* Caller-owned context. Fields are private to menu.lib. */
typedef struct menu_ctx {
    menu_def_t _def;
    unsigned _selected;
    int _valid, _concurrent;
} menu_t;

int menu_init(menu_t *menu, const menu_def_t *definition);
int menu_select(menu_t *menu, unsigned index);
int menu_choose(menu_t *menu);
void menu_close(menu_t *menu);
#endif
