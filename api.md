# menu.lib API Reference

`menu.lib` displays a menu, handles keyboard navigation, and returns an
application-defined value. It never launches commands or processes submit
files. All declarations are in `menu.h`.

---

## Table of Contents

1. [Definitions and Context (`menu.h`)](#1-definitions-and-context-menuh)
2. [Functions](#2-functions)
3. [Ownership and Console Behavior](#3-ownership-and-console-behavior)
4. [Application Example](#4-application-example)
5. [Building and Testing](#5-building-and-testing)

---

## 1. Definitions and Context (`menu.h`)

| Type | Fields | Description |
|------|--------|-------------|
| `menu_entry_t` | `const char *label; int id;` | Display label and nonnegative application-defined return value. |
| `menu_def_t` | `title`, `entries`, `count`, `flags`, `title_color`, `item_color` | Menu definition; see limits below. |
| `menu_t` | Private fields | Caller-owned context retaining the current selection. Do not modify its fields. |

- `title`: non-null string of at most 70 characters.
- `entries`: non-null array of 1–15 entries. Labels must be non-null;
  long labels are clipped to the existing display width.
- `flags`: `MENU_ALLOW_CANCEL`, `MENU_ALLOW_BACK`, both, or zero.
- `title_color`, `item_color`: CP/M-86 color indices 0–15; 14 is the existing
  yellow default. Selection uses black on gray, as in `MENU.CMD`.

The display retains the existing 80-column, 24-row layout and CP/M-86 console
sequences. Supported systems are CP/M-86 1.1 and Concurrent CP/M-86 with BDOS
version 3.0 or later; unsupported systems return `MENU_ERROR` at initialization.

## 2. Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `menu_init` | `int menu_init(menu_t *menu, const menu_def_t *definition)` | Validate and initialize a context with the first entry selected. Returns 0 or `MENU_ERROR`; does not draw. |
| `menu_select` | `int menu_select(menu_t *menu, unsigned index)` | Set the initial/current selection by **zero-based array index**, not action ID. Returns 0 or `MENU_ERROR`. |
| `menu_choose` | `int menu_choose(menu_t *menu)` | Show the menu and block until a selection, enabled Cancel, or enabled Back. Releases the console before returning. |
| `menu_close` | `void menu_close(menu_t *menu)` | Invalidate the context. Repeated calls and `menu_close(NULL)` are harmless. |

`menu_choose` returns:

| Value | Meaning |
|-------|---------|
| `0` or greater | The selected entry's `id`. IDs need not be consecutive or equal to array indices. |
| `MENU_CANCEL` | User pressed Q with `MENU_ALLOW_CANCEL` enabled. |
| `MENU_BACK` | User pressed B with `MENU_ALLOW_BACK` enabled. |
| `MENU_ERROR` | Invalid, closed or unsuccessfully initialized context. |

Up/K and Down/J move the selection without wrapping; Enter selects.
CP/M VT52 arrows and Concurrent raw function keys retain their existing
handling. Q and B are ignored when their corresponding flags are disabled.

## 3. Ownership and Console Behavior

- The definition is copied, but its strings and entry array are borrowed.
  Keep those alive and unchanged until `menu_close` or successful reinitialization.
- No heap allocation is used. Each context retains its own selection between
  `menu_choose` calls. Submenus can use separate contexts owned by the application.
- Failed initialization leaves the context closed. An invalid `menu_select`
  leaves the current selection unchanged. Initialize a context before choosing,
  selecting, or closing it; `menu_close` can also initialize a context as closed.
- Menus run one at a time on the console. Rendering scratch space and keyboard
  mode are shared internally; the library is not reentrant.
- Before returning a choice, Cancel or Back, the library clears the menu,
  shows the cursor, enables wrapping and releases Concurrent raw-key mode.
  The existing renderer leaves normal foreground/background colors (7/0).
  It does **not** save or restore the application's previous screen contents.
- Application actions, errors, pauses, file loading and submenu decisions belong
  to the caller. Returning a choice never invokes a callback or executes a command.

## 4. Application Example

```c
#include <stdio.h>
#include "menu.h"

void edit_settings(void);
void show_report(void);

static const menu_entry_t entries[] = {
    { "Edit settings", 10 },
    { "Show report",   20 }
};
static const menu_def_t definition = {
    "My application", entries, 2, MENU_ALLOW_CANCEL, 14, 14
};

int main(void)
{
    menu_t menu;
    int value;

    if (menu_init(&menu, &definition) < 0) return 1;
    while ((value = menu_choose(&menu)) >= 0) {
        switch (value) {
        case 10: edit_settings(); break;
        case 20: show_report();   break;
        }
    }
    menu_close(&menu);
    return value == MENU_ERROR ? 1 : 0;
}
```

`edit_settings` and `show_report` are application functions. The complete
buildable example is `tstapmnu.c`: its choices return 10, 20 and 99. It prints
the received value, pauses before redisplaying, and exits on 99 or Q.

## 5. Building and Testing

```sh
make menu.lib tstapmnu.cmd
aztec43_acc -I. -squeeze -c myapp.c
aztec43_acc -o myapp.cmd myapp.o menu.lib util.lib
```

Link `menu.lib` before `util.lib`. The archive contains the selector/renderer
(`menulib.o`) and existing keyboard handler (`menukey.o`). It does not contain
`main`, the `.DAT` parser, or command-launch code. `MENU.CMD` uses this same
library and retains its existing launcher/parser modules.

```sh
make test-local
make test-runtime
```

Local regressions cover returned IDs, context independence, retained selection,
flags, bounds, keyboard cleanup, existing launcher behavior and the original
renderer byte-stream checksum. Runtime checks exercise the built CP/M commands
in emu2. These targets do not launch PCE.

The demo and MENU quit/launch paths were also checked separately under actual
CP/M-86 1.1 and CCP/M-86 3.1 in PCE; see [test.md](test.md).
