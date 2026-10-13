# Manual test plan

What to run by hand on **pce** (the CCP loop needs a real CCP) and what can be
checked on **emu2**. Nothing here is automated. Record the result in the last
column or in your own copy.

Related: [README.md](README.md) (syntax), [submit-flow.md](submit-flow.md)
(how the stack works).

## 1. Setup

```sh
make cpmtest.img        # builds the image, copies the test files, lists them
./cpm86                 # pce: A: = cpm86.img (system), B: = cpmtest.img
```

The image holds two user areas on B:

| User | Files |
|---|---|
| 0 | `MENU` `MSUB` `HELLO` `MCLEAN` `MCHECK` `.CMD`, `MENU*.DAT`, `BATCH.SUB`, `NEST.SUB`, `SOAK*.SUB`, `SOAK.DAT`, `HOP.DAT`, `BAD.DAT`, `HOPS.SUB`, `HOPJOB.SUB`, `U0MARK.TXT` |
| 4 | `HELLO.CMD` `MENU.CMD` `MCLEAN.CMD`, `HOP4.DAT`, `U4MARK.TXT` |

A program can only be run from the user area it is in (or from user 0 if it is
a SYS file), so the programs the hop tests use are copied to user 4.

All tests start at the `B>` prompt in user 0 with a clean slate:

```
A>B:
B>USER 0
B>ERA $$$.SUB
```

### Reading the screen

The CCP echoes every record it pops from `$$$.SUB` after the prompt, so a
launch shows the stack as it runs. Use that as the main evidence. Spacing and
the drive letter in the prompt may differ slightly.

### Leftover check (used by the hop tests)

```
B>USER 4
B>DIR $$$.SUB          -> NO FILE expected
B>USER 0
B>DIR $$$.SUB          -> NO FILE expected after leaving the menu with Q
```

## 2. Checks that run on emu2 (no CCP, no user areas)

Run from the project directory; for the `soak/` files run from `soak/` and use
`../mcheck.cmd`. emu2 needs `EMU2_CPM_VER=2.2` for `menu.cmd` itself, not for
`mcheck.cmd`.

| ID | Command | Expected | Result |
|---|---|---|---|
| M1 | `emu2 mcheck.cmd menu.dat` | `11 entries, 0 warnings, 0 errors` | |
| M2 | `emu2 mcheck.cmd menu1.dat`, `menu2.dat` | 6 entries each, 0 errors | |
| M3 | `emu2 ../mcheck.cmd hop.dat` (in `soak/`) | `12 entries, 0 warnings, 0 errors` | |
| M4 | `emu2 ../mcheck.cmd bad.dat` (in `soak/`) | 3 entries (`E@4 good`, `E label@4`, `E!@15 good`) and 11 `ERR:` lines: `@16`, bare `@`, `@x`, `@4A`, `@123`, `C@4`, `CP@4`, `C!@4`, `M@4`, `EP@`, `@4` with no type | |
| M5 | `emu2 mcheck.cmd nofile.dat` | `ERR: cannot open the menu file`, exit code 1 | |
| M6 | `emu2 ../mcheck.cmd limits.dat` (in `soak/`): a 71-character label, a 126-character command, and 15 more valid entries | `WRN` label cut (line 2), `ERR` command longer than 125 (line 3), `ERR` too many entries (line 18, `entry 15`), `15 entries, 1 warnings, 2 errors` | |
| S1 | `EMU2_CPM_VER=2.2 emu2 ../msub.cmd /N soak` (in `soak/`) | lists lines 2-13, then `WARNING: CCP not recognised` (normal on emu2) and `OK: 12 commands = 12 of 128 records. Nothing written.` | pass |

Run M1 to M5 on pce too (`MCHECK BAD.DAT` at `B>`) if you want the same checks on the real system.

## 3. Regression: entries without a hop

Run `MENU` (menu.dat), then each entry. These cover the behaviour that
existed before the user hop and must not change.

| ID | Steps | Expected | Result |
|---|---|---|---|
| R1 | `E` Program with return menu | `HELLO` runs, key, menu comes back on the same entry | |
| R2 | `EP` | as R1, plus "Press any key to return to the menu" | |
| R3 | `E!` | `HELLO` runs, back at `B>`, no menu | |
| R4 | `S` Batch with menu (`BATCH PARAM1`) | `HELLO PARAM1`, `DIR`, then the menu | |
| R5 | `SP` / `S!` | as R4 with pause / without the menu | |
| R6 | `C` / `CP` / `C!` | `HELLO` runs via chain; menu back for `C`, `CP` (after a key), not for `C!` | |
| R7 | `M` Sub-menu 1, `B`, `M` Sub-menu 2, `Q` | sub-menus load in-process; `B` goes back; `Q!` in menu1 means no quit key there | |
| R8 | `Q` at the main menu | back at `B>`; `DIR $$$.SUB` says NO FILE | |
| R9 | `MENU` started from a SUBMIT job: `B>SUBMIT NEST` (`MENU B:MENU.DAT`, `DIR B:*.SUB`) | pick `E`, run it, `Q`: the job carries on with `DIR B:*.SUB` | |
| R10 | `E!`/`S!` inside the job of R9 | entry runs, then the rest of the job, no menu | |
| R11 | `MSUB SOAK` | 10 nested rounds finish, `DIR B:SOAK99.*` runs last | |
| R12 | `MENU SOAK.DAT`, then each entry | `DIR`s, nested `S`/`MSUB`, `A:STAT B:` chains and `E!` behave as the entry labels say | |
| R13 | `MENU` with a missing `.sub` in an `S` entry | error on the footer, menu stays | |

## 4. User hop

Start every test with `B>MENU HOP.DAT` in user 0 and select the entry by its
number (1 to 12 in the label). `U4MARK.TXT` exists only in user 4 and
`U0MARK.TXT` only in user 0, so each `DIR` shows where it ran.

The stacks `MENU` writes for entry *k* are, from the first record popped:

| Stack | Records, in the order the CCP pops them |
|---|---|
| user 0 (home) | `USER 4`, `MCLEAN 4`, `MENU B:HOP.DAT /k` (not for `E!` and `S!`) |
| user 4 | the command (or the lines of the `S` file), `USER 0` |

| ID | Entry | Expected screen sequence and result | Result |
|---|---|---|---|
| H1 | 1 `E@4` `DIR U4MARK.*` | `USER 4`, `DIR U4MARK.*` lists `U4MARK TXT`, `USER 0`, `MCLEAN 4`, `MENU B:HOP.DAT /1`; menu is back on entry 1 | |
| H2 | 2 `EP@4` | as H1, the menu waits for a key before it draws (`/P`) | |
| H3 | 3 `E!@4` | `USER 4`, `DIR U4MARK.*`, `USER 0`, `MCLEAN 4`, then `B>` prompt, no menu | |
| H4 | 4 `S@4` `HOPS` | `USER 4`, `DIR U4MARK.*` lists the file, `DIR U0MARK.*` says NO FILE, `USER 0`, `MCLEAN 4`, menu back | |
| H5 | 5 `SP@4` | as H4, menu waits for a key | |
| H6 | 6 `S!@4` | as H4, no menu at the end | |
| H7 | 7 `E@4` `HELLO` | `HELLO` runs from user 4 (it is not found if it only exists in user 0) | |
| H8 | 9 `E@0` | no hop: only `DIR U0MARK.*` and the menu; no `USER` and no `MCLEAN` lines | |
| H9 | 10 `E@15` `DIR` | `USER 15`, `DIR` (NO FILE), `USER 0`, `MCLEAN 15`, menu back | |
| H10 | 12 plain `E` | no hop lines at all | |

After H1 to H9, with the menu left by `Q`:

| ID | Check | Expected | Result |
|---|---|---|---|
| H11 | `USER 4`, `DIR $$$.SUB` | NO FILE: `MCLEAN` removed the empty file | |
| H12 | `USER 15`, `DIR $$$.SUB` after H9 | NO FILE | |
| H13 | `USER 0`, `DIR $$$.SUB` after `Q` | NO FILE | |
| H14 | the entry selected when the menu comes back | the same entry `/k` | |

### Nested hop (menu running in user 4)

| ID | Steps | Expected | Result |
|---|---|---|---|
| N1 | entry 8 `E@4` `B:MENU B:HOP4.DAT` | menu "Menu running in user 4" appears | |
| N2 | in it, entry 1 (`E`, local) | `DIR U4MARK.*` runs in user 4, the user-4 menu returns | |
| N3 | in it, entry 2 `E@4` | no hop (home is 4): no `USER`/`MCLEAN` lines | |
| N4 | in it, entry 3 `E@0` | `USER 0`, `DIR U0MARK.*` lists the file, `USER 4`, `MCLEAN 0`, `MENU B:HOP4.DAT /3`. `MCLEAN 0` must leave user 0's `$$$.SUB` alone because it still holds the outer records | |
| N5 | `Q` in the user-4 menu | `USER 0`, `MCLEAN 4`, `MENU B:HOP.DAT /8`: back in the main hop menu | |
| N6 | `Q` in the main hop menu, then `USER 4`, `DIR $$$.SUB`, `USER 0`, `DIR $$$.SUB` | NO FILE in both | |

### Inside a SUBMIT job

`HOPJOB.SUB` is `MENU B:HOP.DAT`, then `DIR U0MARK.*`.

| ID | Steps | Expected | Result |
|---|---|---|---|
| J1 | `B>SUBMIT HOPJOB` | menu opens | |
| J2 | entry 1, then `Q` | hop sequence as H1, menu back, `Q`, then `DIR U0MARK.*` of the job runs | |
| J3 | entry 3 `E!@4` | hop, `MCLEAN 4`, then `DIR U0MARK.*` of the job, then `B>` | |

## 5. Failure cases

| ID | Steps | Expected | Result |
|---|---|---|---|
| F1 | entry 11 `E@4` `NOSUCH` | `NOSUCH?` printed; the stack goes on: `USER 0`, `MCLEAN 4`, menu back. **If the chain stops here, record exactly where.** | |
| F2 | rename `MCLEAN.CMD` in user 0 and run H1 | `MCLEAN?` printed, the stack goes on to `MENU`; the empty `$$$.SUB` stays in user 4 (known limit); rename back | |
| F3 | fill the stack: a job with 126 or more records, `MENU` as its last line, then a hop entry | footer error `Too many commands in $$$.SUB`, no hop, menu stays, the job is intact | |
| F4 | fill user 4's `$$$.SUB` the same way (submit from user 4 so the file is there), then a hop from user 0 | footer error `... (user area)`, no write in either user | |
| F5 | fill the disk, then H1 | footer error `Cannot write $$$.SUB`; `USER 4`, `DIR $$$.SUB` says NO FILE | |
| F6 | `^C` in `HELLO` during H7 | observe: what remains in user 4 and user 0 (`DIR $$$.SUB` in both), and whether the menu comes back | |
| F7 | a `.sub` with a `USER 3` line used as `S@4` | the chain ends at that line (known limit); record where you land | |
| F8 | start `MENU HOP.DAT` while in user 4 (`USER 4`, `B:MENU B:HOP.DAT`) | entry 1 `E@4` runs without a hop; entry 9 `E@0` hops to user 0 | |

## 6. What to report

For a failing test: the test ID, the screen sequence you got (the echoed
lines), and the output of `DIR $$$.SUB` in user 0 and in the target user.
