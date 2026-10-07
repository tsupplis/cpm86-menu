# Menu — CP/M-86 1.1 Application Menu

A lean interactive launch menu for CP/M-86 1.1 (BDOS 2.2; `menu.cmd` refuses
to start on any other version). Reads `menu.dat`, displays a
navigable list, and launches the chosen program via `$$$.sub` so the CCP picks
it up on exit. 
It can be considered as an invented precursor to the concurrent dos batch menu but for BDOS 2.2. The foundation is the submit management by CCP.
It is also an nice way to illustrate a solution/workaround to the lack of process management in BDOS 2.2.


## Installation

| File | Where |
|------|-------|
| `menu.cmd` | Default drive, any user area |
| `menu.dat` | Same directory as `menu.cmd` |


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
| command / file+params | 125 chars (longer entries are skipped, never cut) |

### Example

```
; Main menu
T  My CP/M-86 System

E  WordStar 4        | WS
E  SuperCalc 3       | SC
S  Run backup        | BACKUP DRIVE A
M  Utilities         | utils.dat
E! Exit to CP/M      | EXIT
```

### Notes

- If an `M`/`M!` target file is missing or empty, the switch is silently ignored
  and the current menu stays active.
- A sub-menu file — a `.dat` with an `M!` directive — that contains no `S!`
  entries is treated as exit-only: `E`, `S` and `C` entries in it behave like
  `E!`, `S!` and `C!` (no `MENU` re-launch). The main `menu.dat` (no `M!`) is
  never exit-only.
- If an `S` file is missing, empty or invalid, or `$$$.sub` cannot be written,
  the error is shown on the footer line and the menu stays active.

### Nesting (menu inside a SUBMIT job)

`$$$.sub` is used as a stack. When `MENU` is itself a line of a running
SUBMIT job (or of another menu's `S` file), what is left of that job stays in
`$$$.sub` and every launch is pushed **on top** of it:

- `E` / `S` / `C` run the entry, then `MENU` again, as usual.
- `E!` / `S!` / `C!` run the entry, then the rest of the outer job.
- `Q` leaves the menu and the outer job carries on.

`MENU` started from the keyboard starts a fresh `$$$.sub` (any stale one is
removed). Running DR `SUBMIT.CMD` as an entry still replaces the whole file
(that is how `SUBMIT` works); use an `S` entry instead to keep the stack.
`$$$.sub` holds at most 128 commands in total.

### S / S! files

Expanded by `menu.cmd` itself with the DR `SUBMIT` rules, then pushed on the
`$$$.sub` stack:

- `{file}` gets `.SUB` when it has no type; a drive prefix is allowed
  (`B:BACKUP` → `B:BACKUP.SUB`).
- `$1`..`$9` are replaced by the parameters (missing ones expand to nothing;
  each parameter is used up to 32 chars), `$$` gives `$`, `^A`..`^Z` give the
  control character.
- Leading and trailing blanks are removed; blank lines and `;` lines are
  skipped.
- Limits: 125 chars per expanded line, 64 lines per file. Exceeding either is
  reported as an error rather than truncated.
- Reading stops at the first `^Z` (CP/M end of text). `.sub` files made on a
  host and copied with `cpmcp` should end with `^Z` if DR `SUBMIT.CMD` will
  also read them: `SUBMIT` reads the whole last record, including any junk
  after the text. `menu.cmd` does not need the `^Z` itself.

### Drives and user areas

- The re-launch line written to `$$$.sub` is drive-qualified:
  `[D:]MENU D:file.dat`. The `MENU` prefix is the drive `menu.cmd` was started
  from (e.g. `B:MENU` typed at `A>`), read from the CCP command buffer; the
  `.dat` file gets the current drive when it has none.
- Programs that change drive or user themselves (BDOS 14/32) are fine: the CCP
  restores its own drive and user on warm boot before reading `$$$.sub`.
- `$$$.sub` is created on the current drive and user, and the CCP reads it from
  the drive and user it is on at that moment. An entry or `.sub` line that is
  a bare drive change (`B:`) or `USER n` therefore ends the `$$$.sub` chain:
  the remaining lines and the `MENU` re-launch are not run. Use a
  drive-prefixed command instead (`E Prog | B:PROG`), which loads from `B:`
  without changing the CCP's current drive.


## Keys

| Key | Action | Notes |
|-----|--------|-------|
| `↑` / `K` | Move selection up | |
| `↓` / `J` | Move selection down | |
| `Enter` | Launch selected entry | |
| `Q` | Quit to CP/M prompt, or back to the SUBMIT job that started `MENU` | Disabled if `Q!` present in `.dat` |
| `B` | Back to parent menu | Only shown after navigating into an `M!` sub-menu |


## Build

Requires the `cpm86-crossdev` toolchain (`aztec42_cc`, `aztec42_link`, etc.).

```
make          # builds menu.cmd and hello.cmd
make sub.lib  # builds the $$$.sub library independently
make clean    # removes all generated files
```


## sub.lib API

Reusable CP/M-86 library for writing `$$$.sub` submit files.

| Function | Signature | Description |
|----------|-----------|-------------|
| `sub_open` | `int sub_open(int flags)` | `SUB_CREATE`: delete any `$$$.sub` and create it. `SUB_APPEND`: open it and push on top of the records already there (creates it if absent). Returns 0 / -1. |
| `sub_append` | `int sub_append(char *cmd)` | Push one 128-byte record (command uppercased, max 125 chars). Fails once the file holds 128 records. Returns 0 / -1. |
| `sub_close` | `int sub_close(void)` | Close. Returns 0 / -1. |
| `sub_abort` | `int sub_abort(void)` | Undo everything since `sub_open`: delete the file if it was created, else restore its record count. Returns 0 / -1. |
| `sub_delete` | `int sub_delete(void)` | Delete `$$$.sub` if present. |
| `sub_exit` | `void sub_exit(void)` | Set the CCP submit flag and warm boot (BDOS 0): the CCP runs the top record. Does not return. |
| `p_chain` | `void p_chain(char *cmd, int submode)` | Chain to `cmd` (BDOS 47). `submode` non-zero also sets the CCP submit flag so `$$$.sub` runs afterwards. |
| `sub_active` | `int sub_active(void)` | 1 if the CCP is in submit mode (program started from `$$$.sub`), 0 if not, -1 if the CCP is not recognised. |
| `sub_cmddrv` | `int sub_cmddrv(void)` | Drive prefix of the command the CCP is running (1 = A … 16 = P), 0 if none or unknown. |

`sub_exit`, `p_chain`, `sub_active` and `sub_cmddrv` live in `os.asm` and only
touch the CCP after recognising the CP/M-86 1.1 CCP (version 22h and the
`MENUSIG`/`SUBNAM` signature).

Records are written last-command-first (stack order) so the CCP executes them top-down.
See [submit-flow.md](submit-flow.md) for the full hand-over between `menu.cmd`,
the BDOS and the CCP.


## License

MIT — see [LICENSE](LICENSE).
