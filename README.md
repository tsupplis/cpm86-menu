# menu — CP/M-86 Application Menu

A lean interactive launch menu for CP/M-86 1.1. Reads `menu.dat`, displays a
navigable list, and launches the chosen program via `$$$.sub` so the CCP picks
it up on exit. 
It can be considered as a precursor to the concurrent dos batch menu but for BDOS 2.2. The foundation is the submit management by CCP.

---

## Installation

| File | Where |
|------|-------|
| `menu.cmd` | Default drive, any user area |
| `menu.dat` | Same directory as `menu.cmd` |

---

## menu.dat Format

Each line is a **directive**. Lines starting with `;` are comments; blank lines
are ignored. Leading and trailing whitespace around labels and commands is trimmed.
Maximum 15 entries per file.

### Directives

| Directive | Syntax | Description |
|-----------|--------|-------------|
| `T` | `T {title}` | Set the title bar text. Optional; default title used if absent. |
| `Q!` | `Q!` | Disable the `Q`/Quit key entirely. |
| `E` | `E {label} \| {command}` | Launch command; re-launch `MENU` afterwards (stays in menu loop). |
| `E!` | `E! {label} \| {command}` | Launch command; exit cleanly — no `MENU` re-launch. |
| `S` | `S {label} \| {file} [{params}]` | Run a SUBMIT file; re-launch `MENU` afterwards. |
| `S!` | `S! {label} \| {file} [{params}]` | Run a SUBMIT file; exit cleanly — no `MENU` re-launch. |
| `M` | `M {label} \| {menu.dat}` | Selectable entry: load a sub-menu in-process. |
| `M!` | `M! {menu.dat}` | Directive (not an entry): set the `B`/Back destination for this dat file. |
| `C` | `C {label} \| {command}` | Chain directly to command via `P_CHAIN`; `MENU datfile` queued in `$$$.sub` so CCP returns to menu after. |
| `C!` | `C! {label} \| {command}` | Chain directly to command via `P_CHAIN`; exits cleanly with no re-launch. |

### Field limits

| Field | Max length |
|-------|-----------|
| title (`T`) | 70 chars |
| label | 70 chars |
| command / file+params | 64 chars |

### Example

```
; Main menu
T  My CP/M-86 System

E  WordStar 4        | WS
E  SuperCalc 3       | SC
S  Run backup        | BACKUP DRIVE A
M! Utilities         | utils.dat
E! Exit to CP/M      | EXIT
```

### Notes

- If an `M`/`M!` target file is missing or empty, the switch is silently ignored
  and the current menu stays active.
- A sub-menu file that contains no `S!` entries is treated as exit-only
  (no `MENU` re-append on any launch path within it).

---

## Keys

| Key | Action | Notes |
|-----|--------|-------|
| `↑` / `K` | Move selection up | |
| `↓` / `J` | Move selection down | |
| `Enter` | Launch selected entry | |
| `Q` | Quit to CP/M prompt | Disabled if `Q!` present in `.dat` |
| `B` | Back to parent menu | Only shown after navigating into an `M!` sub-menu |

---

## Build

Requires the `cpm86-crossdev` toolchain (`aztec42_cc`, `aztec42_link`, etc.).

```
make          # builds menu.cmd and hello.cmd
make sub.lib  # builds the $$$.sub library independently
make clean    # removes all generated files
```

---

## sub.lib API

Reusable CP/M-86 library for writing `$$$.sub` submit files.

| Function | Signature | Description |
|----------|-----------|-------------|
| `sub_open` | `int sub_open(int flags)` | Create (`SUB_CREATE`) or open (`SUB_APPEND`) `$$$.sub`. Returns 0 / -1. |
| `sub_append` | `int sub_append(char *cmd)` | Write one 128-byte sector (command uppercased). Returns 0 / -1. |
| `sub_close` | `int sub_close(void)` | Flush and close. Returns 0 / -1. |

Records are written last-command-first (stack order) so the CCP executes them top-down.

---

## License

MIT — see [LICENSE](LICENSE).
