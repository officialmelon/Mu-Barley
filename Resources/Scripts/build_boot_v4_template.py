#!/usr/bin/env python3
"""Replace the kernel in a header-v4 boot image before AVB is appended.

The output intentionally contains only the Android boot image through the
page-aligned kernel field.  Use avbtool add_hash_footer afterward to create
fresh metadata and expand it to the target partition size.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import struct
from pathlib import Path


ANDROID_MAGIC = b"ANDROID!"
HEADER_V4_SIZE = 1584
PAGE_SIZE = 4096


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--template", required=True, type=Path)
    parser.add_argument("--kernel-source-boot", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if args.output.exists():
        raise SystemExit(f"refusing to overwrite existing output: {args.output}")

    template = args.template.read_bytes()
    source = args.kernel_source_boot.read_bytes()

    if template[:8] != ANDROID_MAGIC or source[:8] != ANDROID_MAGIC:
        raise SystemExit("template and source must both have Android boot magic")

    template_header_size = struct.unpack_from("<I", template, 20)[0]
    template_header_version = struct.unpack_from("<I", template, 40)[0]
    template_ramdisk_size = struct.unpack_from("<I", template, 12)[0]
    template_signature_size = struct.unpack_from("<I", template, 1580)[0]
    if (
        template_header_size != HEADER_V4_SIZE
        or template_header_version != 4
        or template_ramdisk_size != 0
        or template_signature_size != 0
    ):
        raise SystemExit(
            "template must be header v4 with header_size=1584, "
            "ramdisk_size=0, and signature_size=0"
        )

    source_kernel_size = struct.unpack_from("<I", source, 8)[0]
    source_kernel = source[PAGE_SIZE : PAGE_SIZE + source_kernel_size]
    if len(source_kernel) != source_kernel_size or source_kernel[:3] != b"\x1f\x8b\x08":
        raise SystemExit("source kernel is truncated or is not gzip")

    unpacked = gzip.decompress(source_kernel)
    if unpacked[56:60] != b"ARM\x64":
        raise SystemExit("decompressed payload lacks ARM64 Image magic at offset 0x38")

    header_page = bytearray(template[:PAGE_SIZE])
    struct.pack_into("<I", header_page, 8, source_kernel_size)

    raw_size = PAGE_SIZE + align_up(source_kernel_size, PAGE_SIZE)
    output = header_page + source_kernel
    output += bytes(raw_size - len(output))
    args.output.write_bytes(output)

    print(f"template_sha256={sha256(template)}")
    print(f"kernel_size={source_kernel_size}")
    print(f"kernel_sha256={sha256(source_kernel)}")
    print(f"decompressed_size={len(unpacked)}")
    print(f"decompressed_sha256={sha256(unpacked)}")
    print(f"raw_boot_size={len(output)}")
    print(f"raw_boot_sha256={sha256(output)}")


if __name__ == "__main__":
    main()
