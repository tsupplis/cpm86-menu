#!/usr/bin/env python3
"""Host fault injection and terminal-stream regression, in a temporary folder."""
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="menu-check-") as folder:
    binary = Path(folder) / "menutest"
    subprocess.run([os.environ.get("CC_HOST", "cc"), "-std=gnu89", "-I" + str(root),
                    str(root / "tests/menutest.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], cwd=folder, check=True)
    result = subprocess.run([str(binary), "screen.bin"], cwd=folder,
                            text=True, capture_output=True, check=True)
    print(result.stdout, end="")
    capture = (Path(folder) / "screen.bin").read_bytes()
    expected = (root / "tests/screen.sha").read_text().strip()
    assert hashlib.sha256(capture).hexdigest() == expected, "terminal output changed"
    metrics = dict(field.split("=") for field in result.stdout.split())
    assert int(metrics["console_calls"]) < 500, "console batching regressed"
    print("PASS: terminal bytes match the previous renderer; console calls below 500")
