#!/usr/bin/env python3
"""Patch PiStorm3D's built-in vertex shader to use depth as reciprocal W.

Carmageddon's Glide path submits pre-projected XYZ and texture S/Q, T/Q, Q.
PiStorm3D's immediate-mode path converts the texture coordinates back to S,T,
but drops Q before GPU submission.  With the orthographic projection used by
the Glide shim, the shader's final depth is proportional to Q.  Feeding that
existing value to V3D's reciprocal-W output restores perspective interpolation
without changing position or clipping.
"""

from pathlib import Path
import sys


OLD = b"stvpmv 3, r4                   ; nop"
NEW = b"stvpmv 3, rf13                 ; nop"


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} INPUT OUTPUT", file=sys.stderr)
        return 2

    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    data = source.read_bytes()
    count = data.count(OLD)
    if count != 1:
        print(f"refusing to patch: expected one shader signature, found {count}", file=sys.stderr)
        return 1

    patched = data.replace(OLD, NEW, 1)
    target.write_bytes(patched)
    target.chmod(source.stat().st_mode)
    print(f"patched {source} -> {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
