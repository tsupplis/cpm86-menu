# menu — CP/M-86 Launch Menu Plan

## Overview

A lean CP/M-86 1.1 C application (`menu.cmd`) that reads `menu.dat`, displays
an interactive launch menu with VT52/ANSI visuals, and either launches the chosen
entry via `$$$.sub` + restart-itself trick, or exits cleanly to the CCP.

**Compiler:** Aztec C86 (`aztec42_cc`) — already wired in the Makefile.
**Librarian:** `aztec42_lib` used as `AR` — invocation: `rm -f lib; $(AR) lib obj...`
**UI primitives:** existing `conio.c` / `conio.h` (VT52, `getch`, colour, cursor).  
**Submit format:** CP/M-86 1.1 `$$$.sub` — each record is one 128-byte sector;
within a sector: `buff[0]` = command length *n*, `buff[1..n]` = command chars,
`buff[n+1]` = `0x00`, `buff[n+2]` = `'$'`, rest zeroed.
Records are written **last command first** (stack order) so the CCP pops them top-down.  
**License:** MIT.  
**Filenames:** strict 8.3 (DOS compiler constraint).

---

## Architecture — 4 source files + two standalone libraries

| File | Role |
|------|------|
| `menu.c` | `main()` — wires all components, owns the event loop |
| `menudat.c` | `menu.dat` reader — parses entries into a fixed array |
| `menudat.h` | `MenuItem` struct and `load_menu()` prototype |
| `sub.c` | `$$$.sub` library — open/append/close API |
| `sub.h` | public API for `sub.lib` |
| `conio.c/.h` | existing, unchanged → archived into `util.lib` |
| `os.asm` | existing low-level OS glue → archived into `util.lib` |
| `menu.dat` | data file — one entry per line: `label\|command` |
| `README.md` | concise usage doc |

Two reusable libraries, both independently buildable:

| Library | Members | Purpose |
|---------|---------|---------|
| `util.lib` | `conio.o` + `os.o` | VT52 console + low-level OS glue |
| `sub.lib` | `sub.o` | `$$$.sub` create/append/close |

`menu.cmd` links: `menu.o menudat.o util.lib sub.lib -lc86`

---

## `menu.dat` Format

```
label|command
```

- Max 15 entries (constant `MAX_ENTRIES 15` in `menudat.h`).
- Label: up to 30 chars, displayed in the menu.
- Command: up to 64 chars, written into `$$$.sub`.
- Lines starting with `;` are comments; blank lines ignored.
- File lives on the default drive (same directory as `menu.cmd`).

---

## UI Design

```
╔══════════════════════════════════════════════════════╗
║         CP/M-86  Application Menu  v1.0              ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║   1. Word Processor                                  ║
║ > 2. Spreadsheet          ← reverse-video highlight  ║
║   3. Database                                        ║
║   ...                                                ║
║                                                      ║
║  [↑/k] Up  [↓/j] Down  [Enter] Launch  [Q] Quit     ║
╚══════════════════════════════════════════════════════╝
```

- Full `clrscr()` on entry.
- Banner row: colour highlight (yellow on black, VT52 colour codes via `textcolor()`).
- Entry list: centred block; selected row in reverse video (`\x1bp` / `\x1bq`).
- Keys: arrow-up / `k`, arrow-down / `j`, Enter to launch, `Q`/`q` to quit.
- No flicker: only re-draw the two affected rows when selection moves (gotoxy + reprint).
- On launch: clear screen, then exit — CCP will run `$$$.sub`.
- On quit: clear screen, exit — no `$$$.sub` written.

---

## `$$$.sub` Protocol and `sub.lib` API

### Sector layout (128 bytes each)

```
[0]       = n  (command length, 1–125)
[1..n]    = command text (upper-case)
[n+1]     = 0x00
[n+2]     = '$'
[n+3..127]= 0x00
```

Records are stored **last command first** (stack); the CCP pops from the top,
so the last `sub_append()` call is executed first.

### Public API (`sub.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `sub_open` | `int sub_open(int flags)` | Create or open `$$$.sub`. `flags`: `SUB_CREATE` (delete+make) or `SUB_APPEND` (open existing, seek end). Returns 0 on success, -1 on error. |
| `sub_append` | `int sub_append(char *cmd)` | Write one command sector (uppercased, zero-padded). Must be called after `sub_open`. Returns 0 / -1. |
| `sub_close` | `int sub_close(void)` | Flush and close the file. Returns 0 / -1. |

**Flag constants** (in `sub.h`):

```c
#define SUB_CREATE  0   /* delete any existing $$$.sub, start fresh */
#define SUB_APPEND  1   /* open existing $$$.sub, append new records (future) */
```

`SUB_APPEND` is defined in the API for future use but **not exercised by `menu.cmd`**,
which always uses `SUB_CREATE`. The implementation must accept the flag without
crashing; a stub returning -1 is acceptable for v1.

**Usage pattern in `menu.c`:**
```c
sub_open(SUB_CREATE);
sub_append("MENU");        /* written first = bottom of stack = runs last  */
sub_append(items[sel].cmd);/* written second = top of stack   = runs first */
sub_close();
```

Future additions that do not change the API surface:
- `sub_prepend()` — read existing file, insert at position 0, rewrite.
- `SUB_APPEND` full implementation — open existing FCB, seek to last record.

Uses raw `bdos()` calls only — no `<stdio.h>`. Fully independent of conio and menudat.

---

## Sub-Tasks

---

### Sub-task 1 — `menudat.h` / `menudat.c`: menu.dat reader

**Intent:** Parse `menu.dat` into a fixed array of `MenuItem` structs.

**Expected Outcomes:**
- `load_menu(items, max)` fills the array and returns the count (0 on failure).
- Skips blank lines and `;` comments.
- Truncates label/command silently if over limit.
- No dynamic allocation — fixed-size structs only.

**Todo List:**
- [ ] Define `MenuItem` struct (`label[31]`, `cmd[65]`) in `menudat.h`.
- [ ] Define `MAX_ENTRIES 15`, `MAX_LABEL 30`, `MAX_CMD 64` in `menudat.h`.
- [ ] Declare `int load_menu(MenuItem *items, int max)` in `menudat.h`.
- [ ] Implement `load_menu` in `menudat.c`: open `menu.dat` via `fopen`, read
      lines with `fgets`, split on `|`, strip `\r\n`, store into array.
- [ ] Close file and return count.

**Relevant Context:**
- Aztec C86 has `fopen`/`fgets`/`fclose` in `<stdio.h>` — safe to use here.
- 8.3 filename: `menudat.c`, `menudat.h`.

**Status:** `[ ] pending`

---

### Sub-task 2 — `sub.h` / `sub.c` + `sub.lib`: `$$$.sub` library

**Intent:** Implement the three-function `sub` library as a reusable, independently
buildable CP/M-86 component. Produce `sub.lib` for linking by any program.

**Expected Outcomes:**
- `sub.h` exposes `SUB_CREATE`, `SUB_APPEND`, `sub_open()`, `sub_append()`, `sub_close()`.
- `sub.c` implements all three with raw BDOS calls only; no external dependencies.
- `make sub.lib` builds the library independently of `menu.cmd`.
- `menu.cmd` links against `sub.lib` (and `conio.lib` / `c86.lib` as before).
- Returns -1 on any BDOS error; 0 on success throughout.

**Todo List:**
- [ ] Write `sub.h`: flag constants, three function prototypes, guard macros.
- [ ] Write `sub.c`:
  - Static FCB array initialised to `{0,"$$$     ","SUB", ...}`.
  - Static `char _buf[128]` as the sector work buffer.
  - `sub_open(flags)`: if `SUB_CREATE`, BDOS 19 (delete) then BDOS 22 (make);
    if `SUB_APPEND`, BDOS 15 (open); return -1 if BDOS returns 0xFF.
  - `sub_append(cmd)`: zero `_buf`, write length byte, upcase-copy cmd,
    write `0x00` + `'$'` trailer, BDOS 26 (set DMA to `_buf`), BDOS 21 (write).
  - `sub_close()`: BDOS 16 (close); return -1 if 0xFF.
- [ ] Add `sub.lib` Makefile target: compile `sub.c` → `sub.o` with `aztec42_cc`,
      strip with `aztec42_sqz`, archive with `rm -f sub.lib && $(AR) sub.lib sub.o`.
- [ ] `SUB_APPEND`: define constant, stub implementation returns -1.
- [ ] 8.3 filenames: `sub.c`, `sub.h`, `sub.lib`.

**Relevant Context:**
- Makefile toolchain: `CC = aztec42_cc`, `LIB = aztec42_lib`, `AS = aztec42_as`,
  `STRIP = aztec42_sqz`.
- BDOS fn 15 = open, 16 = close, 19 = delete, 21 = seq write, 22 = make, 26 = set DMA.
- FCB layout: byte 0 = drive (0=default), bytes 1–8 = name padded with spaces,
  bytes 9–11 = ext, bytes 12–31 = zeroed, byte 32 = current record (seq write counter).
- `sub_append` must also increment FCB record byte (`fcb[32]`) after each write,
  or rely on BDOS to do so automatically (BDOS 21 auto-advances `cr`).
- See `submit.plm` `makefile` procedure and `scd.a86` for reference.

**Status:** `[ ] pending`

---

### Sub-task 3 — `menu.c`: UI display and event loop

**Intent:** Full interactive menu using `conio`, wired to the two components above.

**Expected Outcomes:**
- Clears screen, draws banner + bordered entry list on startup.
- Arrow/j/k keys move highlight with minimal redraw (only two rows updated).
- Enter writes `$$$.sub` then exits (`return 0`) so CCP runs the submitted commands.
- `Q`/`q` exits immediately with no `$$$.sub`.
- If `menu.dat` is missing or empty, prints an error and exits.

**Todo List:**
- [ ] Include `menudat.h`, `submit.h`, `conio.h`.
- [ ] In `main`: call `load_menu`; error-exit if count == 0.
- [ ] `draw_screen()`: `clrscr()`, draw banner with `textcolor()` + `cputs()`,
      draw all entries with `gotoxy()`, draw key-help footer.
- [ ] `draw_entry(i, selected)`: `gotoxy` to row, print with/without reverse
      video (`\x1bp`/`\x1bq` via `cputc`/`cputs`).
- [ ] Event loop: `getch()`, handle up/down/enter/quit; on move redraw old +
      new row only; on Enter call `write_sub(items[sel].cmd, "MENU")` then exit.
- [ ] Handle VT52 escape sequences for arrow keys (two-byte sequences from `getch`).

**Relevant Context:**
- `conio.h`: `gotoxy(x,y)`, `textcolor(fg)`, `clrscr()`, `cputs()`, `cputc()`,
  `getch()`, `cursor()`.
- VT52 arrow keys: ESC then `A`(up) `B`(down) `C`(right) `D`(left) — read two
  `getch()` calls.
- Reverse video: `cputc('\x1b'); cputc('p')` = on, `cputc('\x1b'); cputc('q')` = off.
- 8.3 filename: `menu.c`.

**Status:** `[ ] pending`

---

### Sub-task 4 — Makefile update + `menu.dat` sample + `README.md`

**Intent:** Wire all source files into the build with two distinct targets
(`sub.lib` and `menu.cmd`), ship a sample data file, and write the README.

**Expected Outcomes:**
- `make sub.lib` builds and archives the library independently.
- `make` / `make all` builds `menu.cmd` linking against `sub.lib`.
- `menu.dat` sample with 3–5 illustrative entries ships in the repo.
- `README.md`: title, one-liner, install table, `menu.dat` format table,
  key table, build table, `sub.lib` API table, license line.
- `LICENSE` file (MIT, copyright Thierry).

**Todo List:**
- [ ] Update `Makefile`:
  - Add `AR = aztec42_lib` variable.
  - Add `util.lib` target: depends on `conio.o os.o`, `rm -f $@ && $(AR) $@ $^`.
  - Add `sub.lib` target: depends on `sub.o`, `rm -f $@ && $(AR) $@ $^`.
  - Add `menudat.o` compile rule (same `aztec42_cc` + `aztec42_sqz` pattern).
  - `conio.o` and `os.o` rules already present — no change needed.
  - Update `menu.cmd` deps/link: `menu.o menudat.o util.lib sub.lib -lc86`.
  - Update `clean` to remove `*.lib` and new `.o` files.
- [ ] Create `menu.dat` sample.
- [ ] Write `README.md`.
- [ ] Add `LICENSE` (MIT, copyright Thierry).

**Relevant Context:**
- `AR = aztec42_lib` (librarian), `AS = aztec42_as` — both confirmed in the updated Makefile.
- Library rule pattern: `rm -f $@` then `$(AR) $@ $^` (no update mode, always rebuild clean).
- `conio.o` and `os.o` compile rules already exist — no change needed, just add `util.lib` target.
- Existing pattern: `.c` → `aztec42_cc` → `.o` → `aztec42_sqz`; `.asm` → `aztec42_as` → `aztec42_sqz`.

**Status:** `[ ] pending`

---

### Sub-task 5 — Drive/user area switch before launch

**Intent:** Allow a `.dat` entry to specify a drive and/or user area to switch
to before launching the target program, so applications that must run from their
own drive/user are handled transparently.

**Expected Outcomes:**
- New optional field on `E`, `E!`, `S`, `S!` entries: `@{drive}[:{user}]`
  appended after the command, e.g. `E WordStar | WS @B` or `E WS | WS @B:3`.
- Before writing `$$$.sub`, `menu.cmd` prepends the appropriate `B:` / `USER 3`
  CCP commands as extra sectors so the CCP switches context first.
- No change to existing entries without the `@` field — fully backward compatible.
- Drive letter: `A`–`P` (CP/M-86 supports up to 16 drives).
- User area: `0`–`15`.

**Todo List:**
- [ ] Extend `menudat.h` / `menudat.c`: parse optional `@drive[:user]` suffix
      from the command field; store `drive` (0=none, 1=A … 16=P) and `user`
      (-1=none, 0–15) in `MenuItem`.
- [ ] Update `menu.c` launch block: if `drive` or `user` set, insert the
      appropriate CCP commands (`B:`, `USER 3`) as additional `sub_append`
      calls before the main command (adjusted for stack order).
- [ ] Update `README.md` with `@drive[:user]` syntax.

**Relevant Context:**
- CCP drive switch: just submit the drive letter followed by colon, e.g. `B:`.
- CCP user switch: `USER n` command (CP/M-86 1.1 CCP supports it).
- Stack order: last `sub_append` runs first, so drive/user switch must be
  appended *after* the main command.
- **Blocker (found while fixing the `$$$.sub` trigger):** the CCP reads
  `$$$.sub` from its *current* drive and user (`RDCOMN` →
  `PATCHSUBMITSELDSK`, `SUBFCB` drive byte 0). A `B:` or `USER n` line
  changes `CURDRV`/`USRCOD`, so the next read misses the file and the rest
  of the stack (including `MENU`) is lost. Doing this through `$$$.sub` lines
  needs a CCP change. A drive-prefixed command (`B:PROG`) is safe because the
  CCP switches back to `CURDRV` before running the program. Pinning
  `SUBFCB`'s drive byte (CCP `0806h`) would cover drives only, and the CCP
  never clears it, which would break a later plain `SUBMIT`.

**Status:** `[ ] pending (blocked for user areas, see above)`

---

## Open Decisions (resolved)

| Topic | Decision |
|-------|----------|
| Max entries | 15, constant `MAX_ENTRIES` |
| `menu.dat` separator | `\|` pipe |
| Restart command in `$$$.sub` | `MENU` (re-runs `menu.cmd` via CCP) |
| File I/O for `menu.dat` | `fopen`/`fgets` (Aztec stdio, safe) |
| `sub` library API | `sub_open` / `sub_append` / `sub_close` with `SUB_CREATE`/`SUB_APPEND` flags |
| `sub` build output | `sub.lib` — standalone, linkable by any CP/M-86 program |
| File I/O for `$$$.sub` | raw `bdos()` only — no `<stdio.h>`, no Aztec runtime buffering |
| Arrow keys | VT52 two-byte ESC sequences |
| Reverse video | `\x1bp` on / `\x1bq` off |
| Dynamic alloc | None — all fixed arrays |
| Filenames | Strict 8.3 throughout |
| License | MIT |
