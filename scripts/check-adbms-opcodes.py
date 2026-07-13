#!/usr/bin/env python3
"""Fail if two ADBMS6830B command opcodes share a value.

Parses plain `#define NAME 0xNNNN` lines in the library command header and
reports duplicate opcode values. Function-like macros (ADCV(...), RDSTATC(err))
and bit-flag defines (ADCV_RD, ADAX_PUP, *_BASE aliases) are skipped — only
whole-command opcodes are compared. The pre-rewrite header shipped
CMHB 0x0011 colliding with RDCSALL; this guard catches that class of error.
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "common/FEB_ADBMS_Library/Inc/ADBMS6830B_Commands.h"

# Plain object-like defines with a bare hex value (no parameters, no expression)
DEFINE_RE = re.compile(r"^#define\s+([A-Z][A-Z0-9_]*)\s+(0x[0-9A-Fa-f]+)\s*(?://.*|/\*.*)?$")

# Not whole-command opcodes: option bits and explicit base aliases
SKIP_PREFIXES = ("ADCV_", "ADSV_", "ADAX_", "ADAX2_")
SKIP_SUFFIXES = ("_BASE",)


def main() -> int:
    if not HEADER.is_file():
        print(f"check-adbms-opcodes: header not found: {HEADER}", file=sys.stderr)
        return 1

    values = defaultdict(list)
    for line in HEADER.read_text().splitlines():
        m = DEFINE_RE.match(line.strip())
        if not m:
            continue
        name, value = m.group(1), int(m.group(2), 16)
        if name.startswith(SKIP_PREFIXES) or name.endswith(SKIP_SUFFIXES):
            continue
        values[value].append(name)

    duplicates = {v: names for v, names in values.items() if len(names) > 1}
    if duplicates:
        print(f"check-adbms-opcodes: duplicate opcode values in {HEADER.name}:", file=sys.stderr)
        for value, names in sorted(duplicates.items()):
            print(f"  0x{value:04X}: {', '.join(names)}", file=sys.stderr)
        return 1

    print(f"check-adbms-opcodes: OK ({sum(len(n) for n in values.values())} opcodes, no duplicates)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
