# Implementation overview

How `menu.cmd` is built and how its parts fit. For the CCP hand-over, see
[submit-flow.md](submit-flow.md).

## 1. Modules

```mermaid
flowchart LR
    subgraph SRC["sources (in cpm86-menu)"]
        MC["menu.c<br/>UI, launch"]
        MD["menudat.c<br/>.dat parser"]
        MS["msub.c<br/>SUBMIT replacement"]
        MK["mcheck.c<br/>.dat checker"]
        ML["mclean.a86<br/>empty $$$.SUB remover"]
    end
    subgraph ULS["util.lib sources (in cpm86-hacking)"]
        SC["sub.c<br/>$$$.SUB stack"]
        SF["subfile.c<br/>.sub reader"]
        CO["conio.c<br/>VT52 console"]
        OS["os.asm<br/>BDOS / CCP / hardware"]
        UT["util.c / dpb.c / dirent.c<br/>epoch.c / gfx.asm / debug.c"]
    end
    SC --> UL["util.lib"]
    SF --> UL
    CO --> UL
    OS --> UL
    UT --> UL
    MC --> CMD["menu.cmd"]
    MD --> CMD
    UL --> CMD
    LC["Aztec libc<br/>(-lc86)"] --> CMD
    MS --> MSC["msub.cmd"]
    UL --> MSC
    LC --> MSC
    MK --> MKC["mcheck.cmd"]
    MD -- "-DMCHECK" --> MKC
    UL --> MKC
    LC --> MKC
    ML -- "ASM86 + GENCMD 8080" --> MLC["mclean.cmd"]
```

`util.lib` is built in `../cpm86-hacking` and copied into this workspace. The headers
(`sub.h`, `conio.h`) are copied alongside it; `menudat.h` is local.
`mcheck.cmd` compiles `menudat.c` a second time with `-DMCHECK`, which adds a
`menu_diag` hook that reports skipped and altered lines. In the normal build
the hook is compiled away. `mclean.cmd` is assembler in the 8080 model (one
segment, a 32-byte stack inside the image).

| File | Key functions used by menu / msub |
|---|---|
| `menu.c` | `main`, `draw_screen`, `draw_entry`, `build_menucmd`, `numrec` (`USER n` / `MCLEAN n` records), `menu_error` |
| `msub.c` | `main`: banner, usage, `/N` check, `sub_load` → push → `sub_exit` |
| `menudat.c` | `load_menu` → `items[]`, `menu_title`, `menu_back`, `menu_quit_disabled`, `menu_has_snr`; uses `sub_readln`; parses the `@n` suffix |
| `mcheck.c` | `main`: `load_menu` with the `menu_diag` hook, prints `INF:` / `WRN:` / `ERR:` |
| `mclean.a86` | `MCLEAN n`: user n, open `$$$.SUB`, delete if 0 records, restore the user |
| `sub.c` | `sub_open` (CREATE / APPEND), `sub_append`, `sub_close`, `sub_records`, `sub_abort`, `sub_delete` |
| `subfile.c` | `sub_load`, `sub_line`, `sub_lineno`, `sub_pushall`, `sub_name`, `sub_errmsg`, `sub_errline`, `sub_readln` |
| `os.asm` | CCP gate: `ccpseg`, `setsub`, `sub_exit`, `p_chain`, `sub_active`, `sub_cmddrv`<br/>OS / hardware: `getcurdrv`, `setcurdrv`, `getccpdrv`, `bdosx`, `delay`, `bioskey`, `pckey`, `xstatline` |
| `conio.c` | `cputs` (BDOS fn 2), `cputc`, `kbhit`, `getch`, `gotoxy`, `clrscr`, `clreos`, `clreol`, `cursor`, `wrapline`, `statline`, `setstatus`, `textcolor`, `scrmode`, `crtreset` |
| `util.c` | `getenv`, `printenv`, `istrcmp` (not used by menu / msub directly) |
| `dirent.c` | `dirent_load`, `dirent_sort`, `dirent_free`, `dirent_is_fat` (not used by menu / msub directly) |

## 2. Startup

```mermaid
flowchart TD
    A["BDOS 12 = 22h ?"] -- no --> X["'CP/M-86 1.1 required'<br/>exit"]
    A -- yes --> B["parse args<br/>file.dat  /n  /P"]
    B --> C["cmddrv = sub_cmddrv()<br/>subact = sub_active()"]
    C --> D{"/P ?"}
    D -- yes --> E["'Press any key…'<br/>getch"]
    D -- no --> F
    E --> F["load_menu(datfile)"]
    F -- "0 entries" --> Y["usage<br/>exit"]
    F --> G["sel = /n - 1<br/>draw_screen"]
    G --> L["key loop"]
```

## 3. Key loop

```mermaid
flowchart TD
    K["getch"] --> J{"key"}
    J -- "K / J / arrows" --> R["redraw 2 rows"] --> K
    J -- "B (M! set)" --> BK["datfile = menu_back<br/>reload"] --> K
    J -- "Q (no Q!)" --> Q{"subact = 1 ?"}
    Q -- no --> QD["sub_delete<br/>exit"]
    Q -- yes --> QK["keep file, exit<br/>outer job continues"]
    J -- Enter --> T{"type"}
    T -- M --> MM["load_menu(sub-menu)<br/>in process"] --> K
    T -- "E S C (+P, !)" --> LA["launch (section 4)"]
```

## 4. Launch

```mermaid
flowchart TD
    S0{"S / SP / S! ?"} -- yes --> RS["sub_load<br/>read + expand"]
    RS -- error --> ER["menu_error<br/>stay in menu"]
    RS --> M1
    S0 -- no --> M1["remenu = E S C, not exit-only<br/>menucmd = [D:]MENU D:dat /n [/P]<br/>home = BDOS 32 get<br/>hop = @n set and n ≠ home"]
    M1 --> C0{"C / C! and not remenu ?"}
    C0 -- yes --> CH["p_chain(cmd)<br/>no MENU record"]
    C0 -- no --> H0{"hop ?"}
    H0 -- yes --> CK["capacity check<br/>sub_records() in both users"]
    CK -- "over 128" --> ER2["menu_error<br/>nothing written"]
    CK --> AL["BDOS 32 = n<br/>sub_open, push USER home,<br/>then cmd / sub_pushall, sub_close<br/>BDOS 32 = home"]
    AL -- "write fails" --> AB2["sub_abort<br/>menu_error"]
    AL --> OP
    H0 -- no --> OP["sub_open<br/>APPEND if subact = 1, else CREATE"]
    OP --> PU["push: menucmd (if remenu)<br/>then sub_pushall / cmd<br/>(hop: MCLEAN n, USER n instead)"]
    PU -- "write fails" --> AB["sub_abort<br/>if hop: delete the user n file<br/>menu_error"]
    PU --> CL["sub_close"]
    CL --> W{"C / CP ?"}
    W -- yes --> PC["p_chain(cmd, 1)"]
    W -- no --> SE["sub_exit<br/>setsub + BDOS 0"]
```

Exit-only rule: a `.dat` with `M!` and no `S!` entry → `remenu = 0`.

The hop branch (`@n`, E and S types only) writes the user-n stack first so a
failure there leaves nothing in the home user. The records and the CCP side of
it are in [submit-flow.md](submit-flow.md), section 10.

## 5. CCP access (os.asm)

```mermaid
flowchart LR
    G["ccpseg"] --> V{"BDOS 12 = 22h"}
    V -- yes --> S{"BDOS 49 ≠ FFh<br/>ES = CCP seg"}
    S -- yes --> SIG{"'CMD' @0800h<br/>'$$$' @0807h"}
    SIG -- yes --> OK["CF = 0"]
    V -- no --> NO["CF = 1<br/>touch nothing"]
    S -- no --> NO
    SIG -- no --> NO
    OK --> W1["setsub: write MDSUBE @0805h"]
    OK --> R1["sub_active: read MDSUBE"]
    OK --> R2["sub_cmddrv: read CMBUFF @000Bh"]
```

## 6. Data formats

**`$$$.SUB` record** (128 bytes, one per command, last record = top of stack)

| byte 0 | 1 … n | n+1 | n+2 | rest |
|---|---|---|---|---|
| length n (≤ 125) | command, upper case | `00h` | `'$'` | `00h` |

**Re-launch record**

```text
[D:]MENU  D:file.dat  /n  [/P]
 │         │           │    └ EP/SP/CP only: wait for a key first
 │         │           └ entry to select on return
 │         └ current drive added when missing (BDOS 25)
 └ drive MENU was started from (CCP CMBUFF)
```

**`MenuItem`** (`menudat.h`)

| field | size | meaning |
|---|---|---|
| `label` | 71 | shown text |
| `cmd` | 126 | command, `.sub` + params, or `.dat` |
| `type` | int | `MTYPE_E` `ENR` `S` `SNR` `M` `C` `CNR` |
| `pause` | int | 1 for `EP` / `SP` / `CP` |
| `user` | int | `@n` suffix, 0 to 15; `-1` = none |

**Hop records** (`numrec` in `menu.c`)

```text
user n stack (bottom to top):   USER <home>   <cmd | S lines>
home stack (bottom to top):     [outer job]  [MENU ...]  [<drv>:]MCLEAN n  USER n
```

## 7. Limits

| Item | Limit | On overflow |
|---|---|---|
| entries per `.dat` | 15 | ignored |
| label / title | 70 | cut |
| command | 125 | entry skipped |
| `S` file | 64 lines × 125 chars | error, stay in menu |
| `$$$.SUB` | 128 records | write fails → `sub_abort` |
| hop: both stacks | 128 records each | checked before writing, `menu_error` |
| `@n` | 0 to 15 | line skipped (`ERR:` in MCHECK) |
