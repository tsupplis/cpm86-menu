# Implementation overview

How `menu.cmd` is built and how its parts fit. For the CCP hand-over, see
[submit-flow.md](submit-flow.md).

## 1. Modules

```mermaid
flowchart LR
    subgraph SRC["sources"]
        MC["menu.c<br/>UI, launch"]
        MD["menudat.c<br/>.dat parser"]
        SC["sub.c<br/>$$$.SUB stack"]
        SF["subfile.c<br/>.sub reader"]
        MS["msub.c<br/>SUBMIT replacement"]
        CO["conio.c<br/>VT52 console"]
        OS["os.asm<br/>BDOS / CCP access"]
    end
    subgraph LIB["libraries"]
        SL["sub.lib"]
        UL["util.lib"]
    end
    SC --> SL
    SF --> SL
    CO --> UL
    OS --> UL
    MC --> CMD["menu.cmd"]
    MD --> CMD
    SL --> CMD
    UL --> CMD
    LC["Aztec libc<br/>(-lc86)"] --> CMD
    MS --> MSC["msub.cmd"]
    SL --> MSC
    UL --> MSC
```

| File | Key functions |
|---|---|
| `menu.c` | `main`, `draw_screen`, `draw_entry`, `build_menucmd`, `menu_error` |
| `msub.c` | `main`: banner, usage, `/N` check, `sub_load` → push → `sub_exit` |
| `subfile.c` | `sub_load`, `sub_line`, `sub_pushall`, `sub_name`, `sub_errmsg`, `sub_errline` |
| `menudat.c` | `load_menu` → `items[]`, `menu_title`, `menu_back`, `menu_quit_disabled`, `menu_has_snr` |
| `sub.c` | `sub_open` (CREATE / APPEND), `sub_append`, `sub_close`, `sub_abort`, `sub_delete` |
| `os.asm` | `ccpseg` (gate), `setsub`, `sub_exit`, `p_chain`, `sub_active`, `sub_cmddrv` |
| `conio.c` | `cputs` (BDOS 9 runs), `cputc`, `getch`, `gotoxy`, `clrscr`, `cursor` |

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
    S0 -- no --> M1["remenu = E S C, not exit-only<br/>menucmd = [D:]MENU D:dat /n [/P]"]
    M1 --> C0{"C / C! and not remenu ?"}
    C0 -- yes --> CH["p_chain(cmd)<br/>no MENU record"]
    C0 -- no --> OP["sub_open<br/>APPEND if subact = 1, else CREATE"]
    OP --> PU["push: menucmd (if remenu)<br/>then sub_pushall / cmd"]
    PU -- "write fails" --> AB["sub_abort<br/>menu_error"]
    PU --> CL["sub_close"]
    CL --> W{"C / CP ?"}
    W -- yes --> PC["p_chain(cmd, 1)"]
    W -- no --> SE["sub_exit<br/>setsub + BDOS 0"]
```

Exit-only rule: a `.dat` with `M!` and no `S!` entry → `remenu = 0`.

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

## 7. Limits

| Item | Limit | On overflow |
|---|---|---|
| entries per `.dat` | 15 | ignored |
| label / title | 70 | cut |
| command | 125 | entry skipped |
| `S` file | 64 lines × 125 chars | error, stay in menu |
| `$$$.SUB` | 128 records | write fails → `sub_abort` |
