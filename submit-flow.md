# How `menu.cmd` hands work to the CCP through `$$$.SUB`

CP/M-86 1.1 (BDOS 2.2) runs one program at a time and has no way for a program
to say "run this, then come back to me". `menu.cmd` gets that behaviour by
reusing the mechanism `SUBMIT` uses: it leaves a list of commands in a file
for the CCP and makes the CCP read it.

This document describes the CP/M-86 1.1 CCP/BDOS in `../cpm86-kernel`
(`ccp.a86`, `bdos.a86`, `pcbios.a86`).

---

## 1. The three ingredients

| Ingredient | What it is | Who sets it | Who reads it |
|---|---|---|---|
| `$$$.SUB` | A file of 128-byte records, one command per record. **The last record is the top of a stack**: it runs first. | `menu.cmd` (`sub.c`) | CCP (`RDCOMN`) |
| `MDSUBE` | One byte in the CCP's data area. Non-zero = "read the next command from `$$$.SUB`". | `menu.cmd` (`setsub` in `os.asm`) | CCP (`RDCOMN`, `DELSUB`) |
| Warm boot | BDOS fn 0 (or fn 47, chain). Ends the program and restarts the CCP. | `menu.cmd` (`sub_exit` / `p_chain`) | BDOS → BIOS → CCP |

All three are needed:
- **Without `MDSUBE`**, the CCP does not look at the file. A warm boot enters
  the CCP at its hot-start entry (`CCPHOT`), which never re-checks the disk for
  `$$$.SUB`. Only a cold start (`CCPCLD`) does that.
- **Without the file**, the CCP finds nothing to read, clears `MDSUBE` and
  goes back to the keyboard.

Each record is `[length] [command text] 00h '$' 00h…`, upper case, at most 125
characters. This is the same layout DR `SUBMIT` writes.

---

## 2. Where things live in memory

CCP, BDOS and BIOS are loaded as **one** segment (`0051h` on this system, the
A-base of `CPM.SYS`). Offsets below are inside that segment.

```mermaid
flowchart LR
    subgraph SEG["CP/M segment (0051h)"]
        direction TB
        A["0000h  CCP code<br/>0000h jmp CCPCLD (cold)<br/>0006h jmp CCPHOT (warm)"]
        B["0009h  CMBUFF<br/>CCP command line buffer"]
        C["0800h  CCP data<br/>0805h MDSUBE (submit flag)<br/>0806h SUBFCB ($$$.SUB FCB)<br/>08CEh USRCOD, 0939h CURDRV"]
        D["0B00h  BDOS"]
        E["2500h  BIOS<br/>WBOOT jumps to CCP+6"]
        A --- B --- C --- D --- E
    end
    M["menu.cmd<br/>(its own segments)"] -- "BDOS fn 49 returns ES = 0051h" --> SEG
```

`menu.cmd` cannot hard-code `0051h` because it changes with memory size and
build. It calls BDOS fn 49 ("get system data address"), which returns with
`ES` = this segment, then writes `ES:[0805h]`.

> The original bug wrote to `0000:0805h`, which is physical address `805h`.
> That is `0051:02F5h`, a `ret` inside the CCP's command-name lookup.
> The flag was never set and the CCP code was damaged.

---

## 3. The full loop for an `E` entry (run a command, come back)

The menu item is `E WordStar | WS`, and the menu was started as `B:MENU`.

```mermaid
sequenceDiagram
    autonumber
    participant U as User
    participant M as menu.cmd
    participant F as $$$.SUB (on disk)
    participant B as BDOS / BIOS
    participant C as CCP

    U->>M: Enter on "WordStar"
    M->>F: create (delete + make), or push on top inside a SUBMIT job (section 7)
    M->>F: write record 0: "B:MENU A:MENU.DAT"  (bottom)
    M->>F: write record 1: "WS"  (top)
    M->>F: close (directory now says 2 records)
    M->>B: fn 49, then set MDSUBE = FFh at ES:0805h
    M->>B: fn 0 (warm boot)
    B->>C: BIOS WBOOT jumps to CCP+6 (CCPHOT)
    C->>C: restore its own user and drive
    C->>C: RDCOMN sees MDSUBE set
    C->>F: open, read last record (1) = "WS"
    C->>F: record count 2 to 1, close
    C->>U: echo "A>WS"
    C->>B: load and run WS.CMD
    Note over B,C: WS runs and exits with fn 0, another warm boot
    B->>C: CCPHOT again, MDSUBE still FFh
    C->>F: open, read last record (0) = "B:MENU A:MENU.DAT"
    C->>F: record count 1 to 0, close
    C->>B: load and run MENU.CMD from B:
    Note over M: menu.cmd is back, showing A:MENU.DAT
```

When the user presses `Q` and `MENU` was started from the keyboard (no outer
job, see section 7):

```mermaid
sequenceDiagram
    autonumber
    participant M as menu.cmd
    participant F as $$$.SUB
    participant C as CCP
    M->>F: delete
    M->>C: exit (warm boot)
    C->>C: RDCOMN: MDSUBE still FFh
    C->>F: open fails (file gone)
    C->>C: DELSUB: MDSUBE = 0
    C->>C: prompt "A>" and keyboard input
```

The loop above started with `MENU` popped from the bottom of the file: `MDSUBE`
is still `FFh` and the file is empty (record count 0). Pressing `Q` then
deletes nothing that matters, and the CCP ends submit mode.

---

## 4. The stack, step by step

The file is a stack. The CCP always takes the **last** record and then lowers
the record count, so the file shrinks from the end.

```mermaid
flowchart LR
    S0["menu writes<br/>rec1: WS  (top)<br/>rec0: B:MENU A:MENU.DAT"]
    S1["CCP pops rec1, runs WS<br/>left:<br/>rec0: B:MENU A:MENU.DAT"]
    S2["CCP pops rec0, runs MENU<br/>left: (empty, count 0)"]
    S3["menu pushes the next choice<br/>on top of what is left"]
    S0 --> S1 --> S2 --> S3 --> S0
```

For an `S` entry, every expanded line of the `.sub` file is pushed in reverse
order on top of the `MENU` line, so they run top-down and `MENU` runs last:

```text
top    rec3  DIR P1          runs 1st
       rec2  TYPE X.TXT      runs 2nd
       rec1  COPY A B        runs 3rd
bottom rec0  B:MENU A:X.DAT  runs last -> back in the menu
```

`E!`, `S!` and `C!`, and every entry in an exit-only sub-menu, leave out the
bottom `MENU` record, so nothing brings the menu back.

---

## 5. How the CCP picks its next command (`RDCOMN`)

This runs every time the CCP is about to show a prompt.

```mermaid
flowchart TD
    P["CCP about to read a command"] --> Q{"MDSUBE set?"}
    Q -- no --> K["DELSUB (nothing to do)<br/>read keyboard"]
    Q -- yes --> O{"open $$$.SUB<br/>on current drive and user"}
    O -- not found --> K2["DELSUB: MDSUBE = 0,<br/>delete $$$.SUB<br/>read keyboard"]
    O -- found --> R{"read record<br/>count - 1"}
    R -- fails, count was 0 --> K2
    R -- ok --> X["count = count - 1, close<br/>echo the line<br/>run it like a typed command"]
```

Two consequences:
- **The file is looked up on the CCP's current drive and user.** A program
  that changes drive or user itself (BDOS 14/32) is harmless, because the CCP
  restores its own drive and user at every warm boot. A **command line** `B:`
  or `USER 3` changes the CCP's own drive or user. The next lookup then misses
  the file and the rest of the stack, including `MENU`, is lost.
- **The CCP stops reading the file only when it is empty or gone.** That is
  why `Q` deletes it and why `menu.cmd` recreates it on each launch.

---

## 6. The `C` path (chain): same stack, different start

`C` entries use BDOS fn 47 (chain) instead of fn 0. Chain puts the command
straight into the CCP's command buffer, so the program starts **without** a
`$$$.SUB` read. The file is used only for the way back.

```mermaid
sequenceDiagram
    autonumber
    participant M as menu.cmd
    participant F as $$$.SUB
    participant B as BDOS
    participant C as CCP
    M->>F: write rec0 "B:MENU A:MENU.DAT", close
    M->>M: upcase "WS" into chainbuf, set DMA to it
    M->>B: set MDSUBE, then fn 47 (chain)
    B->>C: copy "WS" into CMBUFF, warm boot to CCPHOT
    C->>B: CMBUFF not empty, run WS directly
    Note over B,C: WS exits, warm boot
    C->>F: RDCOMN pops rec0 "B:MENU A:MENU.DAT"
    Note over M: menu.cmd is back
```

`C!` chains without a `MENU` record. Started from the keyboard, it deletes any stale `$$$.SUB` and does not set `MDSUBE`. Inside a SUBMIT job it keeps the file and sets `MDSUBE`, so the rest of the job runs after the program.

---

## 7. Nesting: `MENU` inside a SUBMIT job

`menu.cmd` never assumes it owns `$$$.SUB`. At start it reads `MDSUBE`
(`sub_active`):

| `MDSUBE` at start | Meaning | Launch | `Q` |
|---|---|---|---|
| `0` | Started from the keyboard. The CCP has already run `DELSUB`, so any `$$$.SUB` left is stale. | `SUB_CREATE`: delete, then write a fresh stack | delete `$$$.SUB`, exit |
| `FFh` | Started from a `$$$.SUB` line. The records still in the file are the rest of an outer job. | `SUB_APPEND`: push on top of them | leave the file, exit: the CCP continues the outer job |
| unknown | CCP not recognised (emu2, other build) | as `0` | as `0` |

Example: the user runs `SUBMIT JOB` with `JOB.SUB` = `MENU`, `DIR`.

```mermaid
flowchart TB
    A["SUBMIT writes<br/>rec1: MENU (top)<br/>rec0: DIR"]
    B["CCP pops MENU<br/>left: rec0 DIR"]
    C["user picks E WS<br/>menu pushes on top<br/>rec2: WS (top)<br/>rec1: B:MENU A:MENU.DAT<br/>rec0: DIR"]
    D["CCP pops WS, then MENU<br/>left: rec0 DIR"]
    E["user presses Q<br/>menu exits, file kept"]
    F["CCP pops DIR<br/>file empty, submit mode ends"]
    A --> B --> C --> D --> E --> F
    D -. "user picks another entry" .-> C
```

`MSUB` (`msub.cmd`) makes the same choice, so `MSUB` jobs nest the same way.

To push, `sub_open(SUB_APPEND)` opens the file and sets the current record to
its record count (BDOS open returns the record count of extent 0 in FCB byte
15). Sequential writes then go on top. If a write fails, `sub_abort` puts the
old record count back and closes, the same operation the CCP uses to pop. The
outer job is left as it was. The CCP only reads extent 0, so the stack is
limited to 128 records. `sub_append` refuses to write past that.

---

## 8. Where `menu.cmd` gets the drive letters

The `MENU` line has to work from wherever the CCP is when it pops it.

| Part | Source |
|---|---|
| `B:` before `MENU` | The drive `menu.cmd` was started from. Read from the CCP's command buffer (`CMBUFF+2`, offset `000Bh`), which still holds `B:MENU …` while menu runs (`sub_cmddrv` in `os.asm`). |
| `A:` before the `.dat` | The current drive (BDOS fn 25) when the `.dat` name has none. |
| `/n` and `/P` after it | `/n`: the entry that was launched, so the menu comes back on it. `/P` (only for `EP`/`SP`/`CP`): menu waits for a key before drawing, so the entry's output stays visible. |

---

## 9. Safety guards

- `menu.cmd` refuses to start unless BDOS fn 12 returns `22h` (CP/M-86 1.1),
  because the offsets above belong to that CCP.
- Before writing `MDSUBE` or reading `CMBUFF`, `ccpseg` (`os.asm`) makes three
  checks: fn 12 = `22h`, fn 49 does not return `0FFh` (not implemented, for
  example under emu2), and the CCP **signature** matches: `'CMD'` at `0800h`
  and `'$$$'` at `0807h` (bit 7 masked). These are the labels `MENUSIG` and
  `SUBNAM` in `ccp.a86` and `ccpexp.a86`; `ccporg.a86` has no such labels but
  the same layout. On any other CCP build the signature does not match. `menu.cmd`
  then writes nothing into the CCP: the commands are still in `$$$.SUB`, but
  nothing triggers the CCP to run them.
- An `S` file is fully read and checked **before** `$$$.SUB` is created. Any
  error leaves the menu running and no partial `$$$.SUB` behind.

---

## 10. Running an entry in another user area (`@n`)

The CCP reads `$$$.SUB` from **its own current drive and user** (section 5), so
an entry cannot simply start with `USER 4`: the `MENU` record and the rest of the
stack stay in the first user and are never read. The `@n` suffix gets around
this without changing the CCP, by using what the CCP already does for a
`USER n` command line.

### What the CCP does on `USER n`

`USER n` is handled inside the CCP (`USER` / `USERSE` in `ccp.a86`). It stores
the new user code, calls `GETUSS` and ends in `ENDPRT`, which goes to `CCPLOP`.
There is **no warm boot**, so `MDSUBE` is still set, and the next `RDCOMN` opens
`$$$.SUB` again. The BDOS adds its own user code to the FCB (`USRCOD`, set by
BDOS fn 32), so the file it finds is the `$$$.SUB` **of the new user**. A user
number of 16 or more is refused before anything is changed.

The result is a relay: one stack hands over to another stack in the other user.

### The two stacks

For `E@4 WordStar | WS` started in user 0:

| `$$$.SUB` | Records, first popped on top |
|---|---|
| user 4 | `WS` (top), `USER 0` (bottom) |
| user 0 | `USER 4` (top), `MCLEAN 4`, `MENU B:MENU.DAT /1` (bottom, not for `E!`, `S!`) |

```mermaid
sequenceDiagram
    participant M as menu.cmd
    participant F4 as user 4 $$$.SUB
    participant F0 as user 0 $$$.SUB
    participant C as CCP
    participant X as mclean.cmd
    M->>F4: bdos 32 = 4: write USER 0, then WS, close
    M->>F0: bdos 32 = 0: write MENU, MCLEAN 4, USER 4, close
    M->>C: MDSUBE = FFh, BDOS 0
    C->>F0: pops USER 4, no warm boot
    C->>F4: RDCOMN in user 4 pops WS, runs it
    C->>F4: after the warm boot, pops USER 0
    C->>F0: RDCOMN in user 0 pops MCLEAN 4
    X->>F4: user 4: file has 0 records, delete it; back to user 0
    C->>F0: pops MENU
```

- The user-4 stack is written first, so a failure there leaves nothing in user 0.
- The last record of the user-4 stack is `USER 0`, so the CCP is home again
  before `MCLEAN` and `MENU` are popped.
- The home user comes from BDOS fn 32 (get) at launch time, so the same menu
  works from any user. If `@n` equals the current user, nothing is added: the
  entry runs as a normal one.
- For an `S` entry the expanded lines of the `.sub` file are pushed in the user-4
  stack in place of `WS`. The file itself is read from the current user.
- Inside a SUBMIT job (`MDSUBE` set at start) each stack is appended to what is
  already in that user's file, as in section 7. The job in the home user is
  untouched below the new records.

### The leftover file

`RDCOMN` lowers the record count and closes the file **before** it runs the
command. `DELSUB` removes the file only when a later `RDCOMN` finds nothing
more to read, and only in the CCP's current user. When the last record of the
user-4 stack (`USER 0`) has been popped, the CCP is back in user 0 and never
looks in user 4 again: an empty (0-record) `$$$.SUB` stays there.

`MCLEAN n` is the record that removes it. It switches to user `n` (BDOS 32),
opens `$$$.SUB`, deletes it **only if the record count is 0**, restores the
user and exits. The count check matters when a menu is started inside user 4 by
the hop and hops again: the user-4 file then still holds the outer records and
must not be deleted.

`MCLEAN` runs from the home user, so it does not need to be in user `n`.

### Which files are found where

BDOS opens a file in the current user. Only when the current user is not 0 does
it fall back to user 0, and only for files with the SYS attribute. Therefore:

| What | Where it must be |
|---|---|
| `MENU.CMD`, `MCLEAN.CMD`, the `.dat` file, the `.sub` of an `S` entry | the home user (they run or are read there) |
| the program of the entry (`WS`) | user `n`, or user 0 with the SYS attribute |
| `$$$.SUB` of the home user | not visible from user `n` (not SYS) |

### Failures

- Both stacks have to fit in 128 records. This is checked with `sub_records()`
  (it writes nothing) before the first write, and reported on the footer line.
- If writing the user-`n` stack works and the home stack fails, the user-`n`
  file is deleted when `menu` created it. When it was appended to a job that was
  already running there, its records stay; this case cannot be undone with the
  current `sub_*` functions.
- A `USER` or bare `X:` line inside an `S` file still ends the chain at that
  line. The relay does not change that.
- If `MCLEAN` does not run (for example the chain ended earlier), the empty file
  stays in user `n` until the next hop there creates a new one.
- A command that is not found (`OPENW` fails in `TRNCMK`) makes the CCP print
  `cmd?` through `COMERR` and go back to `CCPLOP`, which reads `$$$.SUB` again;
  the stack is not lost. A program that exists only in user 0, without the SYS
  attribute, is "not found" from user `n` in the same way.
- There is no drive version of this. A drive change in a chain is not supported.
