# key-submit-issue.md — $$$.sub trigger investigation

## What we are trying to do

After writing `$$$.sub` and calling BDOS fn 0 (warm boot), the CCP should
pick up `$$$.sub` and execute its commands. This is how `E`/`E!`/`S`/`S!`
entry types work in `menu.cmd`.

---

## What we know

### 1. The test environment

- Tests run under **pce-ibmpc** (full PC hardware emulator), not emu2.
- The disk image contains a real CP/M-86 1.1 CCP (`ccp.a86`).
- BDOS fn 0 triggers a real warm boot; the real CCP restarts.

### 2. How the real SUBMIT command does it (`scd.a86`)

```asm
mov  es, cs:word ptr savess    ; savess = SS at program entry = CCP data segment
mov  es:byte ptr 0805h, 0FFh   ; set MDSUBE at absolute 0x0805
mov  dl, 0
mov  cl, 0
int  224                        ; BDOS fn 0 (warm boot)
```

- `savess` is the SS value saved at program entry — which is the CCP data segment.
- `0x0805` is an **absolute** address (not a segment:offset; `mdsube equ 0805h`).

### 3. How the CCP uses MDSUBE (`ccp.a86`)

- `MDSUBE` is a local variable in the CCP's own segment at a fixed offset.
- On cold start (`CCPCLD`), the CCP calls `INBDOS` (fn 13, Reset Disk) and
  stores the return value in `MDSUBE`.
- On each command loop iteration, `RDCOMN` checks `MDSUBE`:
  - If non-zero: calls `PATCHSUBMITSELDSK`, opens `SUBFCB` (`$$$     .SUB`),
    reads last sector, decrements RC, closes — and executes that command line.
  - If zero (or open fails): falls through to keyboard input and calls
    `DELSUB` which clears `MDSUBE` and deletes `$$$.sub`.

### 4. Our `sub_exit` implementation

```asm
xor  ax, ax
mov  es, ax              ; ES = segment 0
mov  byte ptr es:[0805h], 0FFh   ; write to absolute 0x0805
xor  cx, cx              ; BDOS fn 0
xor  dx, dx
int  0E0h
```

This matches `scd.a86` in intent (write `0xFF` to `0x0805`, call BDOS 0).

### 5. The $$$.sub file format

The CCP reads sectors from the **bottom** (last record = highest CR).
Stack order: last `sub_append` call = top of stack = runs first.
Our write order for `E`:
1. `sub_append(menucmd)`  → sector 0 = `MENU menu.dat` (runs last)
2. `sub_append(items[sel].cmd)` → sector 1 = command (runs first)

This is correct — the CCP pops sector 1 first.

### 6. The drive D corruption symptom

- After `menu.cmd` exits (without `$$$.sub` being processed), a subsequent
  call to `SUBMIT` goes to drive D, which is invalid.
- Drive D = drive number 3 (0-based). This is suspicious — `CURDRV` in the
  CCP is getting set to 3 somehow.
- In `CCPCLD`, `CURDRV` is set from the low nibble of CL passed to BDOS fn 0.
  Our `sub_exit` passes `CL=0` (drive A). So either:
  - Something else sets `CURDRV` to 3 before `RDCOMN` runs, or
  - The CCP warm-boot entry point uses a different CL source than we think.

---

## What we don't know

### A. Whether MDSUBE at 0x0805 is the right address at runtime

- `scd.a86` uses `savess` (the SS at entry) as the segment for the write,
  with `mdsube equ 0805h` as an absolute address.
- Our `sub_exit` uses `ES=0` with offset `[0805h]` — this is the same
  physical byte **only if** the CCP data segment is loaded at paragraph 0
  (i.e. physical address 0x0000), which is unlikely.
- **Key question**: is `0x0805` truly an absolute physical address, or is it
  `CS/DS:0805h` within the CCP's own segment?
- `scd.a86` uses `savess` (the actual segment address of the CCP data group)
  to do the write — not a hardcoded segment of 0. Our code uses `ES=0` which
  is segment 0, giving physical address `0*16 + 0x0805 = 0x0805`. If the CCP
  data segment is at paragraph e.g. `0x0080` then the physical address of
  `MDSUBE` is `0x0080*16 + 5 = 0x0805`. These are the same only if
  `0x0080*16 = 0x0800` — i.e. the CCP data segment paragraph is `0x0080`.
  **This needs to be confirmed** against the actual memory map.

### B. Why drive D appears

- The source of `CURDRV = 3` is unknown.
- Possibilities: CL is not 0 when BDOS 0 is called; something in the CCP
  reinitialisation uses the previous `CURDRV`; or the pce-ibmpc disk image
  has a different CCP entry convention than `ccp.a86` suggests.

### C. Whether $$$.sub is actually being written correctly

- We have seen the `xxd` dump of `$$$.sub` and it looks correct.
- But we haven't confirmed that BDOS 16 (Close) sets `RC` (FCB byte 15)
  correctly so the CCP can find the last record.

### D. Whether the CCP warm-boot entry re-initialises MDSUBE

- `CCPCLD` reads MDSUBE from BDOS fn 13 on cold start.
- It is not confirmed whether the same path is taken on warm boot (BDOS fn 0),
  or whether MDSUBE is preserved from before the warm boot.
- If the warm boot path does NOT call `INBDOS`, then MDSUBE must be set by
  us before BDOS 0 — which is what we try to do.

---

## Next steps to try

1. **Confirm the CCP load address** — find where the CCP data segment is
   actually mapped in memory. Check the `cpmbase.img` or pce config for the
   CCP load paragraph. If it's `0x0080`, then `ES=0` + `[0805h]` is correct.
   If it's different, we need to use the real segment.

2. **Check the warm-boot entry point** — find what CL value pce-ibmpc passes
   to the CCP on BDOS fn 0, and whether MDSUBE is re-read from BDOS fn 13.

3. **Try writing MDSUBE the same way `scd.a86` does** — save SS at program
   entry into a static variable (the CCP's SS = its data segment), then use
   that saved segment value to write MDSUBE, rather than hardcoding `ES=0`.

---

## Resolution

- `0805h` is an offset in the **CCP segment**, not an absolute address. On
  this CP/M-86 1.1 (`ccp.a86`/`bdos.a86`, BDOS 22h), CCP, BDOS and BIOS share
  one load segment, `0051h` (`cpm.sys` A-base). `scd.a86` writes
  `savess:0805h` with `savess` = CCP segment.
- The old `ES=0` write hit physical `0x0805` = `0051:02F5h`, which is the
  `ret` of `CKRSCM` (built-in command lookup) in the CCP. Patching it to
  `FFh` made every transient command run `dec word [bp+si+3C04h]` /
  `and [di-9],dh` (random CCP data/FCB corruption) and then jump through
  `IXRSRT[-1]` (wild jump: the "drive D" symptom). Built-ins were unaffected.
- WBOOT jumps to `CCP+6` (`CCPHOT`), which never re-runs BDOS fn 13, so
  `MDSUBE` has to be set before BDOS 0. Our write never reached it, so
  `$$$.SUB` was ignored.
- Fix (`os.asm` `setsub`): if fn 12 returns 22h, call BDOS fn 49, which
  returns with `ES` = BDOS/CCP segment, and set `es:[0805h]` to `0FFh`. Skip
  if fn 49 returns `0FFh` (not implemented, e.g. emu2).
- `p_chain` was also wrong: it wrote to `0000:0080h` (interrupt vectors
  20h-3Fh). BDOS fn 47 (`MCHAIN`) copies a NUL-terminated string from the
  current DMA address, and the hot-start path does not upcase it. It now
  upcases into its own buffer, sets the DMA segment and offset to that buffer,
  and for `C` entries calls `setsub` so `$$$.SUB` (`MENU ...`) runs after the
  chained program exits.
