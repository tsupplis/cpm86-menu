# Manual test plan

Related: [README.md](README.md) (syntax), [submit-flow.md](submit-flow.md)
(how the command queue works).

## Automated checks

`make test-local` runs host fault injection for legacy rollback and stack
ordering, concurrent E/S/C and ! execution, repeated resident actions, pause,
M/Back navigation, context restoration, nested priorities and command bounds.
It also requires the existing terminal-output hash and console-call budget.

`make test-runtime` builds all CMDs and runs emu2 smoke tests in a disposable
directory: MSUB and MCLEAN reject non-2.2 versions without touching an empty
stack; MCLEAN still deletes an empty stack on 2.2; sample menu parsing and MSUB
`/N` still work. emu2 does not verify real concurrent scheduling.

## Concurrent system validation

Use a booted Concurrent CP/M-86 >= 3.0 system with MENU, native SUBMIT and the
commands referenced by the test menu available. Do not use MSUB or MCLEAN.

1. Check Up/Down with the physical arrow keys, then J/K, including after
   returning from an action. Run E twice, then C twice: the action gets the console and MENU returns to
   the same selection each time, with no MENU restart or `$$$.SUB` changes.
2. Use M to enter a submenu, launch its second entry, then Back: selection is
   retained after the action and Back still navigates normally.
3. Run S with a native script containing multiple commands and parameters:
   MENU returns after the foreground job finishes. Repeat from a MENU started
   inside an outer native SUBMIT job; Quit should let that job continue.
4. Check EP, SP and CP pause after output; E!, S! and C! exit only after the
   action and context restoration.
5. From user 0 run E@4 and S@4 with programs/scripts available in user 4.
   Verify the action sees user 4 and MENU returns in user 0. Test a native
   SUBMIT USER/$GLOBAL user/drive change as well.
6. Try a missing command, missing SUBMIT and a script error. Direct launch
   failure stays in MENU; native SUBMIT reports its own script errors.
7. Confirm MSUB and MCLEAN display `CP/M-86 1.1 required` on the concurrent
   system and leave existing submit files untouched.

The remaining sections describe **CP/M-86 1.1 legacy behaviour**.

## 0. How to read this plan

Every test has the same three lines:

- **Action**: what you type or press. Nothing else.
- **Why**: what the test is checking, in one sentence.
- **Expected**: what you should see on the screen.

Words used throughout:

| Word | Meaning |
|---|---|
| **entry** | one numbered line of a menu (`1.`, `2.`, ...). The test names the entry by its number. |
| **select entry N** | move the highlight with `j` / `k` (or the down / up arrow) until line N is highlighted, then press Enter. |
| **user** | the CP/M user area. The green status bar at the bottom of pce shows it (`U0`, `U4`, ...). |
| **echoed lines** | after you press Enter on an entry, the menu disappears and the CCP prints, one after the other, each after a `B>` prompt, the lines it takes from `$$$.SUB`. Those lines are what *Expected* lists. |

**You never type the echoed lines.** `MENU` writes them for you; the screen shows
them. If a test lists `USER 4` under *Expected*, that is something you should
*see*, not something you should type. Only type what is under *Action*.

## 1. Setup

```sh
make cpmtest.img        # builds the image and copies the test files into it
./cpm86                 # starts pce: A: = cpm86.img (system), B: = cpmtest.img
```

Files on the image (drive B:):

| User | Files |
|---|---|
| 0 | `MENU`, `MSUB`, `HELLO`, `MCLEAN`, `MCHECK` (`.CMD`); `MENU.DAT`, `MENU1.DAT`, `MENU2.DAT`; `BATCH.SUB`, `NEST.SUB`; `SOAK*.SUB`, `SOAK.DAT`; `HOP.DAT`, `HOPS.SUB`, `HOPJOB.SUB`, `BAD.DAT`, `MISS.DAT`, `USR3.DAT`, `USR3.SUB`, `FULL.SUB`, `LIMITS.DAT`; `U0MARK.TXT` |
| 4 | `HELLO.CMD`, `MENU.CMD`, `MCLEAN.CMD`, `HOP4.DAT`, `U4MARK.TXT` |

A program can only be started from the user it is in, so the programs used in
user 4 are copied there. `U4MARK.TXT` exists only in user 4 and `U0MARK.TXT`
only in user 0: a `DIR U4MARK.*` or `DIR U0MARK.*` shows which user ran it.

### Reset (do this before every test in sections 3 to 6)

At pce, type:

```
A>B:
B>USER 15
B>ERA $$$.SUB
B>USER 4
B>ERA $$$.SUB
B>USER 0
B>ERA $$$.SUB
```

`NO FILE` is the normal answer. Every test then starts at `B>` in user 0
(status bar `U0`).

## 2. Checks on emu2 (a Mac terminal, not pce)

emu2 has no user areas and no CCP loop, so it can only check the `.dat` parsing
and the `MSUB` check mode. Run these from the project directory unless the
*Action* says `cd soak`.

### M1 — menu.dat is valid

- **Action:** `emu2 mcheck.cmd menu.dat`
- **Why:** the sample menu must still be accepted.
- **Expected:** last line `INF: 11 entries, 0 warnings, 0 errors`.

### M2 — the sub-menu files are valid

- **Action:** `emu2 mcheck.cmd menu1.dat`, then `emu2 mcheck.cmd menu2.dat`
- **Why:** same check for the two sub-menus.
- **Expected:** both end with `INF: 6 entries, 0 warnings, 0 errors`.

### M3 — hop.dat is valid

- **Action:** `cd soak`, then `emu2 ../mcheck.cmd hop.dat`
- **Why:** the `@n` suffixes used by the hop tests must parse.
- **Expected:** last line `INF: 12 entries, 0 warnings, 0 errors`.

### M4 — bad @n suffixes are rejected

- **Action:** in `soak`, `emu2 ../mcheck.cmd bad.dat`
- **Why:** malformed suffixes must be reported, not run.
- **Expected:** 3 entries accepted (`E@4 good`, `E label@4`, `E!@15 good`) and 11 `ERR:` lines, one each for `@16`, bare `@`, `@x`, `@4A`, `@123`, `C@4`, `CP@4`, `C!@4`, `M@4`, `EP@`, and `@4` with no type. Exit code 1 (`echo $?`).

### M5 — missing file

- **Action:** `emu2 mcheck.cmd nofile.dat`, then `echo $?`
- **Why:** a missing file must give an error and a non-zero exit code.
- **Expected:** `ERR: cannot open the menu file`, then `1`.

### M6 — size limits

- **Action:** in `soak`, `emu2 ../mcheck.cmd limits.dat`
- **Why:** `limits.dat` holds a 71-character label, a 126-character command and 16 entries in all; the limits are 70, 125 and 15.
- **Expected:** `WRN: line 2: label longer than 70 characters, cut`; `ERR: line 3: command longer than 125 characters`; `ERR: line 18: too many entries (maximum 15), entry ignored`; last line `INF: 15 entries, 1 warnings, 2 errors`.

### S1 — MSUB check mode

- **Action:** in `soak`, `EMU2_CPM_VER=2.2 emu2 ../msub.cmd /N soak`
- **Why:** `/N` checks `SOAK.SUB` without writing anything.
- **Expected:** lines 2 to 13 are listed, then `WARNING: CCP not recognised` (normal on emu2), then `OK: 12 commands = 12 of 128 records. Nothing written.`

## 3. Regression on pce: menu entries without a hop

These test what existed before the user hop. They must behave as before.
All of them use `MENU.DAT`. Its entries are:

| Entry | Type | Label | Runs |
|---|---|---|---|
| 1 | `E` | Program with return menu | `HELLO` |
| 2 | `EP` | Program, pause, return | `HELLO` |
| 3 | `E!` | Progam exit after | `HELLO` |
| 4 | `S` | Batch with menu | `BATCH PARAM1` (`HELLO PARAM1`, then `DIR`) |
| 5 | `SP` | Batch, pause, return | `BATCH PARAM3` |
| 6 | `S!` | Batch exit after | `BATCH PARAM2` |
| 7 | `C` | Chain with return menu | `HELLO` |
| 8 | `CP` | Chain, pause, return | `HELLO` |
| 9 | `C!` | Chain exit after | `HELLO` |
| 10 | `M` | Sub-menu 1 (no quit) | loads `MENU1.DAT` |
| 11 | `M` | Sub-menu 2 | loads `MENU2.DAT` |

`HELLO` prints `Hello! Press any key to continue...` and waits for a key.

### R1 — entry 1, program then back to the menu

- **Action:** `MENU`. Select entry 1. When `HELLO` waits, press a key.
- **Why:** type `E` runs the command and re-launches the menu.
- **Expected:** `HELLO` runs. After the key the menu is drawn again with entry 1 highlighted.

### R2 — entry 2, same with a pause

- **Action:** `MENU`. Select entry 2. Press a key at `HELLO`, then a key at the next prompt.
- **Why:** type `EP` keeps the output on screen until a key is pressed.
- **Expected:** after `HELLO`, the text `Press any key to return to the menu` appears. After the key the menu is back with entry 2 highlighted.

### R3 — entry 3, program then no menu

- **Action:** `MENU`. Select entry 3. Press a key at `HELLO`.
- **Why:** type `E!` does not re-launch the menu.
- **Expected:** `HELLO` runs, then the `B>` prompt. No menu.

### R4 — entry 4, batch then back to the menu

- **Action:** `MENU`. Select entry 4. Press a key at `HELLO`.
- **Why:** type `S` runs every line of `BATCH.SUB`, then re-launches the menu.
- **Expected:** the lines `HELLO PARAM1` and `DIR` are echoed in that order (with the `DIR` listing), then the menu is back with entry 4 highlighted.

### R5 — entry 5, batch with a pause

- **Action:** `MENU`. Select entry 5. Press a key at `HELLO`, then a key at the next prompt.
- **Why:** type `SP` is `S` plus the pause.
- **Expected:** as R4, with `Press any key to return to the menu` before the menu returns (entry 5 highlighted).

### R6 — entry 6, batch then no menu

- **Action:** `MENU`. Select entry 6. Press a key at `HELLO`.
- **Why:** type `S!` does not re-launch the menu.
- **Expected:** `HELLO PARAM2`, `DIR` listing, then the `B>` prompt. No menu.

### R7 — entry 7, chain then back to the menu

- **Action:** `MENU`. Select entry 7. Press a key at `HELLO`.
- **Why:** type `C` starts the program directly (no `$$$.SUB` line for it) and re-launches the menu afterwards.
- **Expected:** `HELLO` runs, then the menu is back (entry 7 highlighted).

### R8 — entry 8, chain with a pause

- **Action:** `MENU`. Select entry 8. Press a key at `HELLO`, then a key at the next prompt.
- **Why:** type `CP` is `C` plus the pause.
- **Expected:** after `HELLO`, `Press any key to return to the menu`. After the key the menu is back (entry 8 highlighted).

### R9 — entry 9, chain then no menu

- **Action:** `MENU`. Select entry 9. Press a key at `HELLO`.
- **Why:** type `C!` does not re-launch the menu.
- **Expected:** `HELLO` runs, then the `B>` prompt. No menu.

### R10 — sub-menus

- **Action:** `MENU`. Select entry 10. Press `Q`. Press `B`. Select entry 11. Press `Q`.
- **Why:** sub-menus load without leaving `MENU`. `Q` is disabled in sub-menu 1 (it has `Q!`), `B` goes back to the main menu, and `Q` works in sub-menu 2.
- **Expected:** entry 10 shows the screen titled `Sub-menu 1`. `Q` does nothing there. `B` shows the main menu again. Entry 11 shows `Sub-menu 2`. `Q` ends at the `B>` prompt.

### R11 — quit leaves nothing behind

- **Action:** `MENU`, press `Q`, then type `DIR $$$.SUB`.
- **Why:** quitting the main menu must not leave a command file.
- **Expected:** back at `B>` after `Q`; `DIR` answers `NO FILE`.

### R12 — menu inside a SUBMIT job, return to the job

- **Action:** type `SUBMIT NEST`. (`NEST.SUB` is `MENU B:MENU.DAT`, then `DIR B:*.SUB`.) When the menu opens, select entry 1, press a key at `HELLO`, then press `Q`.
- **Why:** `MENU` must keep the rest of a running job.
- **Expected:** the menu opens; entry 1 runs; the menu returns; after `Q` the job carries on and its second line, `DIR B:*.SUB`, runs and lists the `.SUB` files.

### R13 — leaving to the prompt inside a job

- **Action:** `SUBMIT NEST`. When the menu opens, select entry 3 and press a key at `HELLO`.
- **Why:** type `E!` runs the program but the job must still finish.
- **Expected:** `HELLO` runs, no menu, then `DIR B:*.SUB` (the rest of the job) runs, then `B>`.

### R14 — MSUB with nested rounds

- **Action:** `MSUB SOAK`
- **Why:** ten nested `MSUB` rounds must all run, in order.
- **Expected:** ten rounds of `DIR`/`STAT` output, and the last line run is `DIR B:SOAK99.*`, which answers `NO FILE`. Then `B>`.

### R15 — the soak menu

- **Action:** `MENU SOAK.DAT`. Select each of entries 1 to 7 in turn (the menu returns after each). Then select entry 8.
- **Why:** nested `S` entries, `MSUB`, chains and a leaving entry in one menu.
- **Expected:** entries 1 to 7 each run their command without an error and the menu returns. Entry 8 (`E!`) runs `DIR B:SOAKX.*` (`NO FILE`) and ends at `B>`.

### R16 — missing .sub file

- **Action:** `MENU MISS.DAT`. Select entry 1. Press a key when asked.
- **Why:** an `S` entry whose file does not exist must not crash or change anything.
- **Expected:** an error message on the bottom line, ending with `-- press a key`. After the key the same menu is shown again.

## 4. User hop on pce

`HOP.DAT` holds the entries below. The `@4` after the type means "run this
entry in user 4". Always start with `B>MENU HOP.DAT` in user 0.

| Entry | Type | Label | Runs |
|---|---|---|---|
| 1 | `E@4` | Hop: DIR in user 4 | `DIR U4MARK.*` |
| 2 | `EP@4` | Hop, pause | `DIR U4MARK.*` |
| 3 | `E!@4` | Hop, leave to the prompt | `DIR U4MARK.*` |
| 4 | `S@4` | Hop with an S file | `HOPS.SUB` = `DIR U4MARK.*`, `DIR U0MARK.*` |
| 5 | `SP@4` | Hop with an S file, pause | `HOPS.SUB` |
| 6 | `S!@4` | Hop with an S file, leave | `HOPS.SUB` |
| 7 | `E@4` | Program in user 4 | `HELLO` |
| 8 | `E@4` | Nested: menu in user 4 | `B:MENU B:HOP4.DAT` |
| 9 | `E@0` | Same user as home: no hop | `DIR U0MARK.*` |
| 10 | `E@15` | Hop to user 15 | `DIR` |
| 11 | `E@4` | Missing program in user 4 | `NOSUCH` |
| 12 | `E` | Plain entry, no hop | `DIR U0MARK.*` |

In every test below, the *Expected* list shows the echoed lines in the order
they appear. After them the menu returns with the same entry highlighted,
unless the test says otherwise. The status bar shows `U0` again at that point.

### H1 — entry 1, a command in user 4

- **Action:** `MENU HOP.DAT`. Select entry 1.
- **Why:** a basic hop: the command runs in user 4, then everything comes back to user 0.
- **Expected:** `USER 4`; `DIR U4MARK.*` listing `U4MARK TXT`; `USER 0`; `MCLEAN 4`; `MENU B:HOP.DAT /1`. Then the menu, entry 1 highlighted, status `U0`.

### H2 — entry 2, hop with a pause

- **Action:** `MENU HOP.DAT`. Select entry 2. Press a key at the pause.
- **Why:** type `EP@4` is H1 plus the pause.
- **Expected:** the same lines as H1 (the last one is `MENU B:HOP.DAT /2 /P`). `Press any key to return to the menu`, and after the key the menu with entry 2 highlighted.

### H3 — entry 3, hop and leave

- **Action:** `MENU HOP.DAT`. Select entry 3.
- **Why:** type `E!@4` must not re-launch the menu.
- **Expected:** `USER 4`; `DIR U4MARK.*` listing `U4MARK TXT`; `USER 0`; `MCLEAN 4`; then the `B>` prompt in user 0 (status `U0`). No `MENU` line, no menu.

### H4 — entry 4, an S file in user 4

- **Action:** `MENU HOP.DAT`. Select entry 4.
- **Why:** every line of `HOPS.SUB` must run in user 4.
- **Expected:** `USER 4`; `DIR U4MARK.*` listing `U4MARK TXT`; `DIR U0MARK.*` answering `NO FILE` (that file is only in user 0); `USER 0`; `MCLEAN 4`; `MENU B:HOP.DAT /4`. Menu back on entry 4.

### H5 — entry 5, S file with a pause

- **Action:** `MENU HOP.DAT`. Select entry 5. Press a key at the pause.
- **Why:** type `SP@4` is H4 plus the pause.
- **Expected:** as H4, then `Press any key to return to the menu`, then the menu on entry 5.

### H6 — entry 6, S file and leave

- **Action:** `MENU HOP.DAT`. Select entry 6.
- **Why:** type `S!@4` must not re-launch the menu.
- **Expected:** as H4 without the `MENU` line: `USER 4`, two `DIR` answers, `USER 0`, `MCLEAN 4`, then the `B>` prompt. No menu.

### H7 — entry 7, a program started in user 4

- **Action:** `MENU HOP.DAT`. Select entry 7. Press a key at `HELLO`.
- **Why:** an interactive program also works after the hop (`HELLO.CMD` is copied into user 4 for this; F1 covers a program that is missing there).
- **Expected:** `USER 4`; `HELLO` runs and waits; after the key `USER 0`; `MCLEAN 4`; `MENU B:HOP.DAT /7`. Menu back on entry 7.

### H8 — entry 9, `@0` while already in user 0

- **Action:** `MENU HOP.DAT`. Select entry 9.
- **Why:** a suffix equal to the current user is ignored: no hop.
- **Expected:** `DIR U0MARK.*` listing `U0MARK TXT`; then `MENU B:HOP.DAT /9`. **No** `USER` line and **no** `MCLEAN` line. Menu back on entry 9.

### H9 — entry 10, user 15

- **Action:** `MENU HOP.DAT`. Select entry 10.
- **Why:** the highest user number works.
- **Expected:** `USER 15`; `DIR` answering `NO FILE`; `USER 0`; `MCLEAN 15`; `MENU B:HOP.DAT /10`. Menu back on entry 10.

### H10 — entry 12, plain entry

- **Action:** `MENU HOP.DAT`. Select entry 12.
- **Why:** an entry without `@n` must be unchanged by the new code.
- **Expected:** `DIR U0MARK.*` listing `U0MARK TXT`; `MENU B:HOP.DAT /12`. No `USER` or `MCLEAN` line.

### L1 — no leftover $$$.SUB files

- **Action:** run H1, H3, H4 and H9 (any order), leave the menu with `Q` (or you are already at `B>` after H3), then type, one line at a time: `USER 4`, `DIR $$$.SUB`, `USER 15`, `DIR $$$.SUB`, `USER 0`, `DIR $$$.SUB`.
- **Why:** `MCLEAN` must have deleted the empty command file that the CCP leaves in the user it hopped to.
- **Expected:** `NO FILE` three times.

## 5. Nested hop: a menu running in user 4

`HOP4.DAT` (in user 4) holds three entries:

| Entry | Type | Label | Runs |
|---|---|---|---|
| 1 | `E` | Local entry, user 4 | `DIR U4MARK.*` |
| 2 | `E@4` | Same user as home: no hop | `DIR U4MARK.*` |
| 3 | `E@0` | Nested hop back to user 0 | `DIR U0MARK.*` |

Tests N1 to N6 are one sequence: do them in order, without resetting in between.

### N1 — start the menu in user 4

- **Action:** `MENU HOP.DAT`. Select entry 8.
- **Why:** a hop whose command is itself a `MENU`.
- **Expected:** `USER 4`; `B:MENU B:HOP4.DAT`; a menu titled `Menu running in user 4`; status `U4`.

### N2 — plain entry in the user-4 menu

- **Action:** select entry 1.
- **Why:** an ordinary entry of a menu running in user 4.
- **Expected:** `DIR U4MARK.*` listing `U4MARK TXT`; `MENU B:HOP4.DAT /1`; the user-4 menu is back on entry 1, status `U4`.

### N3 — `@4` inside user 4

- **Action:** select entry 2.
- **Why:** the home user is 4, so `@4` is "same user": no hop.
- **Expected:** `DIR U4MARK.*`; `MENU B:HOP4.DAT /2`. **No** `USER` or `MCLEAN` line. Menu back on entry 2.

### N4 — a hop from user 4 to user 0

- **Action:** select entry 3.
- **Why:** a second hop while the first one is still open. `MCLEAN 0` must not delete the command file of user 0, because it still holds the records of the first hop.
- **Expected:** `USER 0`; `DIR U0MARK.*` listing `U0MARK TXT`; `USER 4`; `MCLEAN 0`; `MENU B:HOP4.DAT /3`. The user-4 menu is back on entry 3, status `U4`.

### N5 — leave the user-4 menu

- **Action:** press `Q`.
- **Why:** the records of the first hop (still in user 0) must run now.
- **Expected:** `USER 0`; `MCLEAN 4`; `MENU B:HOP.DAT /8`. The main hop menu is back on entry 8, status `U0`.

### N6 — nothing left over

- **Action:** press `Q`. Then type, one line at a time: `USER 4`, `DIR $$$.SUB`, `USER 0`, `DIR $$$.SUB`.
- **Why:** both users end clean after a nested hop.
- **Expected:** `NO FILE` twice.

## 6. Hop inside a SUBMIT job

`HOPJOB.SUB` is: `MENU B:HOP.DAT`, then `DIR U0MARK.*`.

### J1 — entry 1 then quit

- **Action:** type `SUBMIT HOPJOB`. Select entry 1. When the menu returns, press `Q`.
- **Why:** a hop inside a job must keep the rest of the job.
- **Expected:** the echoed lines of H1; the menu returns; after `Q` the job's second line `DIR U0MARK.*` runs and lists `U0MARK TXT`; then `B>`.

### J2 — entry 3 (hop and leave) inside a job

- **Action:** type `SUBMIT HOPJOB`. Select entry 3.
- **Why:** with `E!@4` there is no menu, but the job must still finish.
- **Expected:** `USER 4`; `DIR U4MARK.*`; `USER 0`; `MCLEAN 4`; then `DIR U0MARK.*` (the job's second line) listing `U0MARK TXT`; then `B>`.

## 7. Failures and known limits

### F1 — the program does not exist in user 4

- **Action:** `MENU HOP.DAT`. Select entry 11.
- **Why:** the command queue must survive a failed command.
- **Expected:** `USER 4`; `NOSUCH?`; `USER 0`; `MCLEAN 4`; `MENU B:HOP.DAT /11`; menu back on entry 11. **If the queue stops early,** write down the last line you saw and the user in the status bar.

### F2 — MCLEAN.CMD missing

- **Action:** type `REN MCLEAN.OLD=MCLEAN.CMD`. Then `MENU HOP.DAT`, select entry 1, press `Q`. Then `USER 4`, `DIR $$$.SUB`. Clean up: `ERA $$$.SUB`, `USER 0`, `REN MCLEAN.CMD=MCLEAN.OLD`.
- **Why:** if the cleanup tool cannot run, the rest of the queue must still go on.
- **Expected:** the echoed lines of H1, but `MCLEAN 4` answers `MCLEAN?`; `MENU B:HOP.DAT /1` still runs and the menu returns. `DIR $$$.SUB` in user 4 **lists** the empty file (this is the known limit).

### F3 — too many records

- **Action:** type `SUBMIT FULL` (127 lines: `MENU B:HOP.DAT` then 126 times `DIR B:SOAK99.*`). If `SUBMIT` refuses, use `MSUB FULL`. When the menu opens, select entry 1. Press a key at the message.
- **Why:** with 126 records already waiting, a hop needs 3 more and would not fit in the 128 allowed. Nothing must be written.
- **Expected:** on the bottom line `Too many commands in $$$.SUB -- press a key`. After the key the menu is still shown. No `USER 4` line appears. Press `Q`: the 126 lines `DIR B:SOAK99.*` run (each `NO FILE`), then `B>`.

### F4 — Ctrl-C in the program (observation)

- **Action:** `MENU HOP.DAT`. Select entry 7. At `Hello! Press any key...` press Ctrl-C. Afterwards type `DIR $$$.SUB` in user 0, then `USER 4`, `DIR $$$.SUB`, `USER 0`.
- **Why:** nobody knows yet what the CCP does with Ctrl-C here. This test records it; there is no pass or fail.
- **Expected:** write down (a) the lines echoed after Ctrl-C, (b) whether the menu returned, (c) the two `DIR $$$.SUB` answers.

### F5 — a `USER` line inside an S file (known limit)

- **Action:** `MENU USR3.DAT`. Select entry 1. (`USR3.SUB` is `DIR U4MARK.*`, `USER 3`, `DIR U4MARK.*`.) Then clean up: `USER 4`, `ERA $$$.SUB`, `USER 0`, `ERA $$$.SUB`.
- **Why:** the documented limit: a `USER` line in an `S` file ends the queue.
- **Expected:** `USER 4`; `DIR U4MARK.*` lists `U4MARK TXT`; `USER 3`; then the `B>` prompt with status `U3`. The last `DIR`, `USER 0`, `MCLEAN` and `MENU` lines never appear.

### Not covered

Not tested here: a full disk while writing, and a full command file in the target
user. Both go through the same code as F3 and the code that reports `Cannot write
$$$.SUB`.

## 8. What to report

For a test that fails: the test ID, the lines you saw (copy the screen), and
the answers of `DIR $$$.SUB` in user 0 and in the user that was the target.
