#!/usr/bin/env python3
# Copyright (C) 2026 Honkord
"""Verify public headers under include/HonkordGL do not pull forbidden paths or vendor SDK angle includes."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INC = ROOT / "include" / "HonkordGL"

INCLUDE_LINE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)([>"])')

FORBIDDEN_SUBSTRING = re.compile(
    r"(internal/|\.\./|/src/|/internal/)",
    re.IGNORECASE,
)


def _angle_vendor(name: str) -> bool:
    n = name.strip().lower().replace("\\", "/")
    if n in ("windows.h",):
        return True
    if n.startswith(
        (
            "gl/",
            "gles",
            "egl/",
            "vulkan/",
            "metal/",
            "opencl/",
            "cuda/",
            "d3d",
            "dxgi",
            "dxgi.h",
        )
    ):
        return True
    if n.startswith("opengl/"):
        return True
    return False


def main() -> int:
    failed = False
    for path in sorted(INC.rglob("*.h")):
        if "third_party" in path.parts:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"check_public_headers: cannot read {path}: {exc}", file=sys.stderr)
            return 2
        for lineno, line in enumerate(text.splitlines(), 1):
            m = INCLUDE_LINE.match(line)
            if not m:
                continue
            inc_path = m.group(2)
            if FORBIDDEN_SUBSTRING.search(inc_path):
                print(
                    f"{path.relative_to(ROOT)}:{lineno}: forbidden include path: {line.strip()}",
                    file=sys.stderr,
                )
                failed = True
            if m.group(1) == "<" and _angle_vendor(inc_path):
                print(
                    f"{path.relative_to(ROOT)}:{lineno}: forbidden angle-bracket include: {line.strip()}",
                    file=sys.stderr,
                )
                failed = True
    if failed:
        print("check_public_headers: FAILED", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
