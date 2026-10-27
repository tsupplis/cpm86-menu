#!/usr/bin/env python3
"""Smoke-test built CMDs with emu2 in a disposable directory.

This verifies version guards and CP/M parsing, not concurrent
process scheduling or the CCP warm-boot loop.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="menu-runtime-") as folder:
    work = Path(folder)
    for name in ("menu.cmd", "msub.cmd", "mclean.cmd", "mcheck.cmd",
                 "menu.dat", "menu1.dat", "menu2.dat", "batch.sub"):
        shutil.copy2(root / name, work / name)

    def run(program, *args, version="2.2"):
        command = ["emu2"]
        result = subprocess.run(command + [program + ".cmd", *args], cwd=work,
                                env={**os.environ, "EMU2_CPM_VER": version},
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=10)
        return result.stdout.decode("latin1").replace("\r", "")

    stack = work / "$$$.sub"
    for version in ("2.1", "3.0", "3.1", "4.1"):
        for program, args in (("msub", ("batch",)), ("mclean", ("0",))):
            # An empty stack is exactly what MCLEAN would otherwise delete.
            stack.write_bytes(b"")
            output = run(program, *args, version=version)
            assert "CP/M-86 1.1 required" in output, (program, version, output)
            assert stack.exists() and stack.read_bytes() == b"", (program, version)
    print("PASS: built MSUB/MCLEAN reject BDOS != 2.2 without deleting an empty stack")

    stack.write_bytes(b"")
    run("mclean", "0")
    assert not stack.exists(), "MCLEAN on 2.2 still deletes empty stack"
    print("PASS: built MCLEAN on BDOS 2.2 removes empty stacks")

    for dat in ("menu.dat", "menu1.dat", "menu2.dat"):
        output = run("mcheck", dat)
        assert "0 warnings, 0 errors" in output, output
    output = run("msub", "[N]", "batch")
    assert "Nothing written" in output, output
    assert not stack.exists()
    output = run("msub", "[n", "batch")
    assert "Nothing written" in output, output
    assert not stack.exists()
    print("PASS: built menu parsers and MSUB dry run ([N] and [n)")
