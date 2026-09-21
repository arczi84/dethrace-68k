#!/usr/bin/env python3
"""Enable MiniGL v12's existing XYZW position path for immediate vertices."""

from hashlib import sha256
from pathlib import Path
import sys


EXPECTED_SHA256 = "78c2fd0c93760676d781a75c1efb4e29dcd109875f829a96839ffa99aaf61a8d"
TEXT_FILE_OFFSET = 0x28


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


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} INPUT PACKER.BIN OUTPUT", file=sys.stderr)
        return 2

    source = Path(sys.argv[1])
    packer_path = Path(sys.argv[2])
    target = Path(sys.argv[3])
    original = source.read_bytes()
    digest = sha256(original).hexdigest()
    if digest != EXPECTED_SHA256:
        print(f"refusing unknown MiniGL binary: SHA-256 {digest}", file=sys.stderr)
        return 1

    packer = packer_path.read_bytes()
    data = bytearray(original)

    # Known unused diagnostic routine; large enough for the replacement loop.
    cave = TEXT_FILE_OFFSET + 0x8E3A
    expected_cave = bytes.fromhex("4e55ffc048e7203043ed000826592059")
    if bytes(data[cave:cave + len(expected_cave)]) != expected_cave:
        raise RuntimeError("unexpected diagnostic code-cave contents")
    data[cave:cave + len(packer)] = packer

    # Position allocation: 3 floats -> 4 floats per submitted vertex.
    at_vma(data, 0x1292A, bytes.fromhex("d086"), bytes.fromhex("d084"))
    # Replace the XYZ pack loop with the XYZW packer.
    at_vma(
        data, 0x12966,
        bytes.fromhex("4a866f00010a"),
        bytes.fromhex("4ef900008e3a"),
    )
    # Position attribute: stride 12/size 3 -> stride 16/size 4.  Leave all
    # shader strings untouched so the library selects its built-in vec4 path.
    at_vma(data, 0x13178, bytes.fromhex("4878000c"), bytes.fromhex("48780010"))
    at_vma(data, 0x1317E, bytes.fromhex("48780003"), bytes.fromhex("48780004"))
    at_vma(data, 0x13182, bytes.fromhex("48780003"), bytes.fromhex("48780004"))
    at_vma(data, 0x1318A, bytes.fromhex("48780003"), bytes.fromhex("42a74e71"))

    target.write_bytes(data)
    target.chmod(source.stat().st_mode)
    print(f"patched {source} -> {target}")
    print(f"SHA-256 {sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
