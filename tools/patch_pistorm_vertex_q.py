#!/usr/bin/env python3
"""Add Carmageddon's texture Q to PiStorm3D's immediate vertex stream.

This patch is deliberately tied to the known PiStorm3D MiniGL v12 binary.  It
expands the packed position attribute from XYZ to XYZQ, keeps Q out of the
orthographic position calculation, and sends Q as reciprocal W for perspective
varying interpolation.
"""

from hashlib import sha256
from pathlib import Path
import sys


EXPECTED_SHA256 = "78c2fd0c93760676d781a75c1efb4e29dcd109875f829a96839ffa99aaf61a8d"
TEXT_FILE_OFFSET = 0x28

PACK_XYZQ = bytes.fromhex(
    "206d000c226dffc8220620184c3c00000000005cd0ab0f7c28402014"
    "e1584840e15822c0202c0004e1584840e15822c0202c0008e1584840"
    "e15822c04aab0fbc6706202c00546006203c3f800000e1584840e158"
    "22c0538166b04ef900012a74"
)


def at_vma(data: bytearray, vma: int, old: bytes, new: bytes) -> None:
    offset = TEXT_FILE_OFFSET + vma
    actual = bytes(data[offset:offset + len(old)])
    if actual != old:
        raise RuntimeError(
            f"unexpected bytes at VMA 0x{vma:x}: {actual.hex()} != {old.hex()}"
        )
    if len(old) != len(new):
        raise RuntimeError("fixed-address replacement changed size")
    data[offset:offset + len(new)] = new


def shader_line(data: bytearray, old: bytes, new: bytes) -> None:
    if len(new) > len(old):
        raise RuntimeError("replacement shader line is too long")
    new = new + b" " * (len(old) - len(new))
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"expected one shader line {old!r}, found {count}")
    pos = data.index(old)
    data[pos:pos + len(old)] = new


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} INPUT OUTPUT", file=sys.stderr)
        return 2

    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    original = source.read_bytes()
    digest = sha256(original).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"refusing unknown MiniGL binary: SHA-256 {digest}", file=sys.stderr)
        return 1

    data = bytearray(original)

    # Replace an unused built-in diagnostic routine with the XYZQ packer.
    cave = TEXT_FILE_OFFSET + 0x8E3A
    expected_cave = bytes.fromhex("4e55ffc048e7203043ed000826592059")
    if bytes(data[cave:cave + len(expected_cave)]) != expected_cave:
        raise RuntimeError("unexpected diagnostic code-cave contents")
    data[cave:cave + len(PACK_XYZQ)] = PACK_XYZQ

    # Allocate 16 bytes per vertex and jump over the original XYZ-only packer.
    at_vma(data, 0x1292A, bytes.fromhex("d086"), bytes.fromhex("d084"))
    at_vma(
        data, 0x12966,
        bytes.fromhex("4a866f00010a"),
        bytes.fromhex("4ef900008e3a"),
    )

    # Describe the first V3D attribute as a four-float position stream.
    at_vma(data, 0x13178, bytes.fromhex("4878000c"), bytes.fromhex("48780010"))
    at_vma(data, 0x1317E, bytes.fromhex("48780003"), bytes.fromhex("48780004"))
    at_vma(data, 0x13182, bytes.fromhex("48780003"), bytes.fromhex("48780004"))
    at_vma(data, 0x1318A, bytes.fromhex("48780003"), bytes.fromhex("42a74e71"))

    # Vertex shader: input 3 is Q; texture S/T consequently move to 4/5.
    # Matrix translation still uses an explicit 1, so Q cannot move geometry.
    shader_line(
        data,
        b"or rf3, 0x3f800000, 0x3f800000 ; nop",
        b"ldvpmv_in rf3,  3 ; nop",
    )
    shader_line(data, b"ldvpmv_in rf11,  3 ; nop", b"ldvpmv_in rf11,  4 ; nop")
    shader_line(data, b"ldvpmv_in rf12,  4 ; nop", b"ldvpmv_in rf12,  5 ; nop")
    shader_line(data, b"nop ; fmul r0, rf3, r5", b"or r0, r5, r5 ; nop")
    shader_line(
        data,
        b"stvpmv 3, r4                   ; nop",
        b"stvpmv 3, rf3                  ; nop",
    )

    target.write_bytes(data)
    target.chmod(source.stat().st_mode)
    print(f"patched {source} -> {target}")
    print(f"SHA-256 {sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
