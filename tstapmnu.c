/* Minimal application-owned actions: menu.lib only returns a value. */
#include <stdio.h>
#include "conio.h"
#include "menu.h"

static const menu_entry_t entries[] = {
    { "First action (returns 10)", 10 },
    { "Second action (returns 20)", 20 },
    { "Finish (returns 99)", 99 }
};
static const menu_def_t definition = {
    "Application menu test", entries, 3, MENU_ALLOW_CANCEL, 14, 14
};

int main(void)
{
    menu_t menu;
    int value;
    if (menu_init(&menu, &definition) < 0) {
        puts("Cannot initialize menu");
        return 1;
    }
    while ((value = menu_choose(&menu)) >= 0) {
        printf("Application received: %d\n", value);
        if (value == 99) break;
        puts("Press a key to return to the menu");
        getch();
    }
    menu_close(&menu);
    puts("Application finished");
    return value == MENU_ERROR ? 1 : 0;
}
