#!/usr/bin/env python3
"""
flash-patcher.py - Stamp flash-time provenance into an ELF before programming.

Given a linked ELF that contains a .feb_flash_info section, this tool:
  1. Resolves the section's load address via `arm-none-eabi-readelf -S`.
  2. Verifies the existing placeholder magic (FEB_FLASH_INFO_MAGIC = 0xFEB1A5F1)
     so we fail loud on stale / wrong-target binaries instead of corrupting
     someone else's firmware.
  3. Builds a 128-byte replacement blob carrying:
       - magic  (uint32_t)
       - schema (uint32_t)
       - flash_utc (32 bytes, ISO-8601 UTC, null-padded)
       - flasher_user (24 bytes)
       - flasher_host (32 bytes)
       - reserved (32 bytes, zero)
  4. Invokes `arm-none-eabi-objcopy --update-section .feb_flash_info=<blob>`
     to rewrite the region in-place.

The output is a patched ELF (default: alongside input as <name>.patched.elf)
that flash.sh hands to STM32_Programmer_CLI.

Exit codes:
   0 - success
   1 - CLI / argument error
   2 - toolchain missing (nm/readelf/objcopy not on PATH)
   3 - ELF missing .feb_flash_info section or symbol
   4 - placeholder magic mismatch (likely wrong-target ELF)

Keep this file in sync with common/FEB_Serial_Library/FEB_Version/Inc/feb_version.h
- struct layout is duplicated here because we can't #include C from Python.
Any change to FEB_Flash_Info_t MUST bump FEB_FLASH_INFO_SCHEMA and this
file's SCHEMA constant.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# Must match FEB_FLASH_INFO_MAGIC / FEB_FLASH_INFO_SCHEMA in feb_version.h.
MAGIC = 0xFEB1A5F1
SCHEMA = 1

# Struct layout - must match FEB_Flash_Info_t in feb_version.h exactly.
# Total 128 bytes. Native little-endian (STM32 is LE).
STRUCT_FMT = "<II32s24s32s32s"
STRUCT_SIZE = struct.calcsize(STRUCT_FMT)
assert STRUCT_SIZE == 128, f"struct size drifted: {STRUCT_SIZE}"

SECTION_NAME = ".feb_flash_info"

# Toolchain prefix - STM32CubeCLT / GCC ARM Embedded both ship these.
TOOL_PREFIX = os.environ.get("FEB_TOOLCHAIN_PREFIX", "arm-none-eabi-")


def _log(msg: str) -> None:
    print(f"[flash-patcher] {msg}", file=sys.stderr)


def _die(code: int, msg: str) -> None:
    print(f"[flash-patcher] ERROR: {msg}", file=sys.stderr)
    sys.exit(code)


def _find_tool(name: str) -> str:
    exe = shutil.which(TOOL_PREFIX + name)
    if not exe:
        _die(2, f"'{TOOL_PREFIX}{name}' not on PATH - install STM32CubeCLT or GCC ARM Embedded")
    return exe


def _read_section_bytes(elf: Path, section: str) -> bytes:
    """Extract the raw bytes of <section> from <elf> via objcopy."""
    objcopy = _find_tool("objcopy")
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    try:
        # objcopy --dump-section writes the section contents to a file.
        subprocess.run(
            [objcopy, "-O", "binary", "--only-section", section, str(elf), str(tmp_path)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        return tmp_path.read_bytes()
    except subprocess.CalledProcessError as e:
        _die(3, f"failed to extract section '{section}' from {elf}: {e.stderr.decode(errors='replace')}")
        return b""  # unreachable
    finally:
        tmp_path.unlink(missing_ok=True)


def _encode_fixed(value: str, width: int, field_name: str) -> bytes:
    """Encode <value> as UTF-8, null-pad to <width>, truncate if too long."""
    data = value.encode("utf-8", errors="replace")
    if len(data) >= width:
        _log(f"truncating {field_name}: '{value}' > {width - 1} bytes")
        data = data[: width - 1]
    return data + b"\x00" * (width - len(data))


def build_blob(flash_utc: str, flasher_user: str, flasher_host: str) -> bytes:
    """Pack a 128-byte blob matching FEB_Flash_Info_t."""
    return struct.pack(
        STRUCT_FMT,
        MAGIC,
        SCHEMA,
        _encode_fixed(flash_utc, 32, "flash_utc"),
        _encode_fixed(flasher_user, 24, "flasher_user"),
        _encode_fixed(flasher_host, 32, "flasher_host"),
        b"\x00" * 32,
    )


def _utc_now() -> str:
    # Seconds precision is enough; adds no semantic value to go finer.
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _hostname_short() -> str:
    import socket
    return socket.gethostname().split(".", 1)[0]


def _user() -> str:
    return os.environ.get("USER") or os.environ.get("USERNAME") or "unknown"


def _locate_struct_in_section(section_bytes: bytes) -> int:
    """Return the offset at which the FEB_Flash_Info_t struct begins.

    The linker may insert leading alignment padding before the KEEP()
    data (ALIGN(16) inside the section body pads to a 16-byte boundary
    relative to the section's own start address). We tolerate any
    leading padding by searching for the magic word.
    """
    magic_bytes = struct.pack("<I", MAGIC)
    # Limit search to the first 64 bytes - if the magic isn't near the
    # start, something is badly wrong with the binary.
    idx = section_bytes.find(magic_bytes, 0, 64)
    return idx


def patch_elf(src_elf: Path, dst_elf: Path,
              flash_utc: str, flasher_user: str, flasher_host: str,
              require_placeholder: bool) -> dict:
    """Patch src_elf's .feb_flash_info section and write to dst_elf."""
    # 1. Sanity-check: section exists.
    existing = _read_section_bytes(src_elf, SECTION_NAME)
    if len(existing) < STRUCT_SIZE:
        _die(3, f"section {SECTION_NAME} is {len(existing)} bytes, expected >= {STRUCT_SIZE}")

    # 2. Locate the struct (tolerant of leading linker alignment padding).
    struct_offset = _locate_struct_in_section(existing)
    if struct_offset < 0:
        _die(4, f"magic 0x{MAGIC:08X} not found in section {SECTION_NAME} "
                f"(wrong target ELF? stale build? linker section missing?)")
    if len(existing) < struct_offset + STRUCT_SIZE:
        _die(3, f"section too short after padding: offset={struct_offset}, "
                f"section_size={len(existing)}, struct_size={STRUCT_SIZE}")

    # 3. Verify schema. Under require_placeholder we demand an exact match
    #    so we never silently re-stamp an image from a future firmware
    #    with a different layout.
    (schema,) = struct.unpack("<I", existing[struct_offset + 4:struct_offset + 8])
    if schema != SCHEMA:
        if require_placeholder:
            _die(4, f"schema mismatch: got {schema}, expected {SCHEMA} "
                    f"(feb_version.h bumped but flash-patcher.py was not)")
        else:
            _log(f"warning: schema {schema} != {SCHEMA}, patching anyway")

    # 4. Build the new section payload: preserve leading padding + any
    #    trailing reserved bytes, only overwrite the 128-byte struct.
    new_blob = build_blob(flash_utc, flasher_user, flasher_host)
    new_section = (
        existing[:struct_offset]
        + new_blob
        + existing[struct_offset + STRUCT_SIZE:]
    )

    # 5. Copy src -> dst and run objcopy --update-section on the copy.
    dst_elf.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src_elf, dst_elf)

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        tmp_path = Path(tmp.name)
        tmp_path.write_bytes(new_section)
    try:
        objcopy = _find_tool("objcopy")
        subprocess.run(
            [objcopy, f"--update-section", f"{SECTION_NAME}={tmp_path}", str(dst_elf)],
            check=True,
        )
    finally:
        tmp_path.unlink(missing_ok=True)

    return {
        "flash_utc": flash_utc,
        "flasher_user": flasher_user,
        "flasher_host": flasher_host,
        "elf": str(dst_elf),
    }


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Stamp flash-time metadata into an ELF.")
    ap.add_argument("--elf", required=True, type=Path, help="input ELF (linked firmware)")
    ap.add_argument("--out", type=Path, default=None, help="output ELF path (default: <elf>.patched.elf)")
    ap.add_argument("--utc", default=None, help="flash UTC override (default: now)")
    ap.add_argument("--user", default=None, help="flasher user override (default: $USER)")
    ap.add_argument("--host", default=None, help="flasher host override (default: hostname -s)")
    ap.add_argument("--allow-schema-mismatch", action="store_true",
                    help="warn instead of dying on schema mismatch")
    ap.add_argument("--print", action="store_true",
                    help="print the resulting metadata as key=value lines")
    args = ap.parse_args(argv)

    if not args.elf.exists():
        _die(1, f"input ELF not found: {args.elf}")

    out = args.out or args.elf.with_suffix(args.elf.suffix + ".patched.elf")
    flash_utc = args.utc or _utc_now()
    flasher_user = args.user or _user()
    flasher_host = args.host or _hostname_short()

    result = patch_elf(
        args.elf, out,
        flash_utc=flash_utc,
        flasher_user=flasher_user,
        flasher_host=flasher_host,
        require_placeholder=not args.allow_schema_mismatch,
    )

    if args.print:
        for k, v in result.items():
            print(f"{k}={v}")
    else:
        _log(f"patched {args.elf.name} -> {out}")
        _log(f"  flash_utc    = {flash_utc}")
        _log(f"  flasher_user = {flasher_user}")
        _log(f"  flasher_host = {flasher_host}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
