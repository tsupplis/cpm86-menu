#ifndef MENUKEY_H
#define MENUKEY_H

/* Concurrent XIOS uses raw high-bit function-key codes when ESC 6 disables
   programmable expansion. CP/M retains its existing VT52 input path. */
void menu_keys_init(int concurrent);
void menu_keys_enter(void);
void menu_keys_leave(void);
int menu_key_read(void);

#endif
