# Developer Scripts

Everyday tooling for the FEB_FIRMWARE_SN5 repo. All scripts are POSIX `bash` and tested on macOS (zsh default + bash) and Windows Git Bash. Run them from the repo root unless noted.

## Quick Reference

| Script | One-liner | Common usage |
|---|---|---|
| [`setup.sh`](setup.sh) | First-time dev environment setup | `./scripts/setup.sh` |
| [`build.sh`](build.sh) | Build firmware (interactive / batch / release) | `./scripts/build.sh -b BMS` |
| [`flash.sh`](flash.sh) | Flash an ELF/bin to a board via the pinned STM32CubeProgrammer bundle | `./scripts/flash.sh -b BMS` |
| [`serial.sh`](serial.sh) | Open the board's USB-VCP serial console at 115200 (tio wrapper; one-shot `--exec`/`--read` modes for verification) | `./scripts/serial.sh -b BMS` |
| [`format.sh`](format.sh) | Run `clang-format` across `Core/User/` | `./scripts/format.sh --check` |
| [`setup-hooks.sh`](setup-hooks.sh) | Install / remove pre-commit hooks | `./scripts/setup-hooks.sh` |
| [`cubemx.sh`](cubemx.sh) | Drive STM32CubeMX headlessly from a `.ioc` | `./scripts/cubemx.sh -g -b BMS` |
| [`flash-patcher.py`](flash-patcher.py) | Stamp flash-time provenance into a `.feb_flash_info` ELF section | Invoked automatically by `flash.sh` |

## `setup.sh` — First-Time Dev Environment

```bash
./scripts/setup.sh           # Full setup + initial build
./scripts/setup.sh --quick   # Skip initial build
./scripts/setup.sh -h
```

Steps: install pinned bundles via `cube` → pre-commit install → CMake configure → initial build.

Requires the `cube` CLI from the STM32Cube for VS Code extension (see the top-level [README.md](../README.md#environment-setup)). The script runs `cube bundle install --project` at the repo root and does **not** edit any shell profile. CI / non-TTY runs fall back to safe defaults for every prompt.

## Board list (`lib/boards.sh`)

`build.sh`, `flash.sh`, `serial.sh`, `format.sh` and `cubemx.sh` read the board list from [`lib/boards.sh`](lib/boards.sh). Add a new board there: `FEB_BOARD_DIRS` lists source directories, `FEB_BOARD_TARGETS` lists the buildable/flashable CMake targets (Sensor_Nodes has FRONT and REAR variants).

## Toolchain resolution (`lib/cube-tools.sh`)

`build.sh`, `flash.sh` and `setup.sh` source [`lib/cube-tools.sh`](lib/cube-tools.sh). It finds `cube` (on PATH, in `~/.local/stm32cube/bin`, or inside the VS Code extension), asks `cube --resolve` for the PATH of the bundles pinned in `.settings/bundles.store.json`, and puts them first on PATH. The scripts then call `cmake`, `ninja`, `arm-none-eabi-*` and `STM32_Programmer_CLI` by name, so they use exactly the versions VS Code uses. Without `cube` (e.g. CI) they fall back to whatever is on PATH; `build.sh` prints which source it used. It also warns if `CUBE_BUNDLE_PATH` still points at an STM32CubeCLT folder.

## `build.sh` — Build Firmware

```bash
./scripts/build.sh                  # Interactive menu
./scripts/build.sh -a               # Build all boards (Debug)
./scripts/build.sh -b LVPDB         # Build one board
./scripts/build.sh -b LVPDB -b PCU  # Build several
./scripts/build.sh -r               # Release mode
./scripts/build.sh -c               # Clean + rebuild
./scripts/build.sh -l               # Loop mode
```

Validates toolchain, re-runs `cmake --preset Debug` if needed, and prints per-target Flash/RAM usage after the build. See the top-level [README.md](../README.md) for the CMake-direct equivalents.


## `flash.sh` — Flash a Board

```bash
./scripts/flash.sh                    # Interactive menu (with build timestamps)
./scripts/flash.sh -b LVPDB           # Flash a specific board
./scripts/flash.sh -f firmware.elf    # Flash a specific file
./scripts/flash.sh -l                 # Loop mode
./scripts/flash.sh --list-probes      # List connected programmers
./scripts/flash.sh -h
```

Uses the pinned STM32CubeProgrammer bundle via `lib/cube-tools.sh`, falling back to `STM32_Programmer_CLI` on PATH (release zips ship `lib/cube-tools.sh` alongside `flash.sh`).

Internally calls [`flash-patcher.py`](flash-patcher.py) to stamp the ELF's `.feb_flash_info` section with flash timestamp and flasher identity before programming.

## `serial.sh` — Serial Console Helper

```bash
./scripts/serial.sh                                  # Board menu → port menu → tio
./scripts/serial.sh -b BMS                           # Print BMS expectations, then port menu
./scripts/serial.sh -p /dev/cu.usbmodem1234          # Open a specific port directly
./scripts/serial.sh -b PCU --read 3                  # Capture 3s of output, print, exit
./scripts/serial.sh -b BMS --exec "version,uptime"   # Round-trip the console, exit
./scripts/serial.sh --baud 9600 -b PCU               # Override baud (default 115200)
./scripts/serial.sh --list                           # List candidate ports and exit
./scripts/serial.sh -h
```

A thin wrapper around [`tio`](https://github.com/tio/tio) — install with `brew install tio` (macOS) or `apt install tio` (Debian/Ubuntu). The script hard-fails with install instructions if `tio` is not on PATH.

The companion to [`flash.sh`](flash.sh): after programming a board, run this to actually verify the firmware is alive. `version` / `uptime` / `help` over the feb_io console is what proves a successful flash, not the programmer's exit code.

Three modes:

- **Interactive** (default) — board menu, then port menu (auto-picked when only one is connected), then `tio`. Prints per-board banner expectations and the `Ctrl-T q` detach hint before opening the line.
- **`--read <SECONDS>`** — captures everything that comes off the port for N seconds, prints the transcript to stdout, exits. Use this from a script or from Claude to verify a boot banner.
- **`--exec "<cmd1,cmd2,...>"`** — sends each comma-separated command followed by CR/LF, waits `--settle` seconds (default 2), prints the captured transcript to stdout, exits. Diagnostics go to stderr so `transcript=$(./scripts/serial.sh -p ... --exec "version")` captures only the device's reply. Exit `0` on bytes received, `1` on silence — branch on it to detect a dead board.

Auto-detects ports on macOS (`/dev/cu.usbmodem*`, `/dev/cu.usbserial*`, `/dev/cu.SLAB_USBtoUART*`) and Linux (`/dev/ttyACM*`, `/dev/ttyUSB*`). When invoked with `-b <BOARD>`, prints what to expect on serial for that board (banner timing, smoke-test commands, when to look at CAN instead of UART).

## `format.sh` — clang-format

```bash
./scripts/format.sh           # Format all Core/User/*.c and *.h in place
./scripts/format.sh --check   # CI mode; exits 1 if any file would change
```

Uses the repo's top-level `.clang-format` (LLVM base, 2-space indent, 120 columns, Allman braces) with clang-format **19.1.2**, the same version as the `st-arm-clangd` bundle VS Code formats with. Uses `clang-format` from PATH only if it is exactly that version; otherwise installs `clang-format==19.1.2` from PyPI into `build/.tools/` (requires `python3`). CI relies on the same bootstrap.

## `setup-hooks.sh` — Pre-commit Hooks

```bash
./scripts/setup-hooks.sh          # Install hooks
./scripts/setup-hooks.sh --remove # Uninstall
./scripts/setup-hooks.sh -h
```

Installs `pre-commit` via (in order) Homebrew → `pipx` → `pip --user`. Then installs the hooks from `.pre-commit-config.yaml`. Hooks: trailing-whitespace, end-of-file-fixer, check-added-large-files, check-merge-conflict, mixed-line-ending, `clang-format`, `cppcheck`, CAN validation.

## `cubemx.sh` — Headless STM32CubeMX

```bash
./scripts/cubemx.sh                     # Interactive menu
./scripts/cubemx.sh -g -b BMS           # Generate code for BMS
./scripts/cubemx.sh -i -b LVPDB         # Inspect LVPDB .ioc
./scripts/cubemx.sh -a -g               # Generate for all boards
./scripts/cubemx.sh --list-boards       # List boards and .ioc status
./scripts/cubemx.sh --show-pins -b BMS  # GPIO pin assignments
./scripts/cubemx.sh --show-peripherals -b BMS
./scripts/cubemx.sh -m -b BMS           # Migrate .ioc to current CubeMX version
./scripts/cubemx.sh --update-packs      # Refresh STM32 firmware packs
```

Inspection subcommands (`-i`, `--show-pins`, `--show-peripherals`, `--list-boards`) parse the `.ioc` directly and work without STM32CubeMX installed. Code generation and migration require STM32CubeMX.

On Windows Git Bash, POSIX paths (`/c/...`) are converted to native (`C:\...`) via `cygpath -w` (with a `sed` fallback) before being passed into CubeMX scripts, since STM32CubeMX.exe is a native Windows binary.

## `flash-patcher.py` — Flash-Time Provenance

Not normally invoked directly — `flash.sh` calls it before programming. Given a linked ELF, it stamps the `.feb_flash_info` section with a 128-byte blob carrying magic / schema / UTC timestamp / flasher user / flasher host, then runs `arm-none-eabi-objcopy --update-section` to rewrite the region. Verifies `FEB_FLASH_INFO_MAGIC = 0xFEB1A5F1` first so a stale or wrong-target binary fails loudly instead of silently corrupting.

Requires `arm-none-eabi-readelf` and `arm-none-eabi-objcopy` on PATH (already true after `setup.sh`).

## Cross-Platform Notes

- All scripts are `bash`, not `sh`. On Windows, run them from **Git Bash** (bundled with [Git for Windows](https://git-scm.com/download/win)) or WSL.
- Line endings are LF. Git is configured via `.gitattributes` (if present) to preserve this.
- ANSI color output is used by default. Redirecting stdout to a file still includes escape codes — pipe through `sed 's/\x1b\[[0-9;]*m//g'` if you need plain text.
- Scripts prefer `command -v` for tool lookup and fall back to globbing well-known install paths. If you install a tool somewhere non-standard, set `PATH` yourself or open a PR to add your location to the glob list.
