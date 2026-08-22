#!/usr/bin/env python3
"""Audit a Barley stage-trace BootShim/FD payload and Android boot v4 image."""

import argparse
import gzip
import hashlib
import struct
from pathlib import Path


PAGE_SIZE = 4096
FD_SIZE = 0x200000
PARTITION_SIZE = 0x2000000


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shim", type=Path, required=True)
    parser.add_argument("--fd", type=Path, required=True)
    parser.add_argument("--gzip", type=Path, required=True)
    parser.add_argument("--combined", type=Path, required=True)
    parser.add_argument("--boot", type=Path, required=True)
    args = parser.parse_args()

    shim = args.shim.read_bytes()
    fd = args.fd.read_bytes()
    packed = args.gzip.read_bytes()
    combined = args.combined.read_bytes()
    boot = args.boot.read_bytes()
    unpacked = gzip.decompress(packed)

    assert len(fd) == FD_SIZE
    assert combined == shim + fd
    assert unpacked == combined
    assert boot[:8] == b"ANDROID!"

    kernel_size, ramdisk_size, os_version, header_size = struct.unpack_from(
        "<4I", boot, 8
    )
    header_version = struct.unpack_from("<I", boot, 40)[0]
    cmdline = boot[44:1580].split(b"\0", 1)[0].decode("ascii", "replace")
    signature_size = struct.unpack_from("<I", boot, 1580)[0]
    kernel_offset = PAGE_SIZE
    kernel = boot[kernel_offset : kernel_offset + kernel_size]

    assert header_version == 4
    assert header_size == 1584
    assert ramdisk_size == 0
    assert signature_size == 0
    assert kernel == packed
    assert kernel[:3] == b"\x1f\x8b\x08"
    assert gzip.decompress(kernel) == combined
    assert len(boot) == PARTITION_SIZE

    code0, code1 = struct.unpack_from("<2I", shim, 0)
    text_offset, image_size, flags = struct.unpack_from("<3Q", shim, 8)
    arm64_magic = shim[0x38:0x3C]
    assert arm64_magic == b"ARMd"

    fd_word = struct.unpack_from("<I", fd, 0)[0]
    assert fd_word >> 26 == 0b000101
    branch_imm = fd_word & 0x03FFFFFF
    if branch_imm & 0x02000000:
        branch_imm -= 0x04000000
    fd_branch_target = branch_imm * 4

    footer = boot[-64:]
    magic, major, minor, original_size, vbmeta_offset, vbmeta_size, _ = (
        struct.unpack(">4sIIQQQ28s", footer)
    )
    assert magic == b"AVBf"
    assert boot[vbmeta_offset : vbmeta_offset + 4] == b"AVB0"

    print(f"BootShim size: {len(shim)}")
    print(f"BootShim SHA256: {sha256(shim)}")
    print(f"BootShim code0/code1: 0x{code0:08X} 0x{code1:08X}")
    print(f"ARM64 text_offset: 0x{text_offset:X}")
    print(f"ARM64 image_size: 0x{image_size:X}")
    print(f"ARM64 flags: 0x{flags:X}")
    print(f"ARM64 magic @0x38: {arm64_magic!r}")
    print(f"FD size: {len(fd)}")
    print(f"FD SHA256: {sha256(fd)}")
    print(f"FD first instruction: 0x{fd_word:08X}")
    print(f"FD reset branch target offset: 0x{fd_branch_target:X}")
    print(f"Combined size: {len(combined)}")
    print(f"Combined SHA256: {sha256(combined)}")
    print(f"Gzip size: {len(packed)}")
    print(f"Gzip SHA256: {sha256(packed)}")
    print(f"Gzip exact combined: {unpacked == combined}")
    print(f"FD exact at BootShim offset 0x{len(shim):X}: {combined[len(shim):] == fd}")
    print(f"First 64 decompressed bytes: {unpacked[:64].hex(' ').upper()}")
    print(f"Android magic: {boot[:8].decode('ascii')}")
    print(f"Header version: {header_version}")
    print(f"Header size: {header_size}")
    print(f"Kernel offset: 0x{kernel_offset:X}")
    print(f"Kernel size: {kernel_size}")
    print(f"Kernel SHA256: {sha256(kernel)}")
    print(f"Kernel gzip magic: {kernel[:3].hex(' ').upper()}")
    print(f"Ramdisk size: {ramdisk_size}")
    print(f"OS version field: 0x{os_version:08X}")
    print(f"Cmdline: {cmdline!r}")
    print(f"Signature size: {signature_size}")
    print(f"AVB footer version: {major}.{minor}")
    print(f"AVB original image size: {original_size}")
    print(f"AVB vbmeta offset: {vbmeta_offset}")
    print(f"AVB vbmeta size: {vbmeta_size}")
    print(f"Final image size: {len(boot)}")
    print(f"Final image SHA256: {sha256(boot)}")


if __name__ == "__main__":
    main()
