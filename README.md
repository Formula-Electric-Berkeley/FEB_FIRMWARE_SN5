# FEB Firmware SN5

Firmware for the FEB SN5 Formula E vehicle. Each subdirectory corresponds to a board on the car, built around STM32F4 microcontrollers.

## Project Structure

| Directory | Board | MCU | Notes | Docs |
|-----------|-------|-----|-------|------|
| `BMS/` | Battery Management System | STM32F446RE | FreeRTOS, cell-monitor front-end | [BMS/README.md](BMS/README.md) |
| `DASH/` | Driver Dashboard | STM32F469NI | FreeRTOS, LVGL, SDRAM, FATFS | [DASH/README.md](DASH/README.md) |
| `DCU/` | Data Control Unit | STM32F446RE | Placeholder (empty user code) | [DCU/README.md](DCU/README.md) |
| `LVPDB/` | Low Voltage Power Distribution | STM32F446RE | Dual CAN, 7× TPS | [LVPDB/README.md](LVPDB/README.md) |
| `PCU/` | Powertrain Control Unit | STM32F446RE | Dual CAN, triple ADC, RMS gateway | [PCU/README.md](PCU/README.md) |
| `Sensor_Nodes/` | Sensor Aggregator | STM32F446RE | IMU + GPS + WSS; third-party lwgps | [Sensor_Nodes/README.md](Sensor_Nodes/README.md) |

All boards are fully buildable with CMake.

## Common Libraries

Shared across boards, under `common/`. See [`common/README.md`](common/README.md) for the library index.

| Target | Role | Docs |
|---|---|---|
| `feb_io` | Umbrella: UART + Log + Console + Commands + Version + Time + String_Utils | [FEB_Serial_Library](common/FEB_Serial_Library/README.md) |
| `feb_can` | FreeRTOS-safe CAN runtime (dispatcher) | [FEB_CAN_Library](common/FEB_CAN_Library/README.md) |
| *(compiled into each board)* | CAN message definitions + code generator | [FEB_CAN_Definitions](common/FEB_CAN_Definitions/README.md) |
| `feb_tps` | TPS2482 power-monitor driver | [FEB_TPS_Library](common/FEB_TPS_Library/README.md) |
| `feb_time` | 64-bit microsecond monotonic clock | [FEB_Time_Library](common/FEB_Time_Library/README.md) |
| `feb_rtos_utils` | `REQUIRE_RTOS_HANDLE` fail-fast macro | [FEB_RTOS_Utils](common/FEB_RTOS_Utils/README.md) |

## Repository Layout

```
cmake/                           # Shared toolchain files
  gcc-arm-none-eabi.cmake        # ARM GCC cross-compiler config
  FEB_Version.cmake              # Per-board build provenance generator
common/                          # Shared libraries (see common/README.md)
  FEB_Serial_Library/            # feb_io umbrella + sub-libraries
  FEB_CAN_Library/               # feb_can runtime dispatcher
  FEB_CAN_Definitions/           # CAN message defs + code generator
  FEB_TPS_Library/               # feb_tps
  FEB_Time_Library/              # feb_time
  FEB_RTOS_Utils/                # feb_rtos_utils
scripts/                         # Developer scripts (see scripts/README.md)
  setup.sh                       # First-time dev environment setup
  build.sh                       # Build firmware (interactive/batch/release)
  flash.sh                       # Flash firmware to boards
  format.sh                      # Code formatting (--check for CI)
  cubemx.sh                      # Generate HAL code from .ioc files
  setup-hooks.sh                 # Install pre-commit hooks
third-party/                     # Vendored external code (lwgps, etc.)
.github/workflows/               # CI/CD pipelines
```

## Cloning the Repository

```bash
git clone https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5.git
```

## Getting Started

For first-time setup, run the setup script:

```bash
./scripts/setup.sh        # Full setup with initial build
./scripts/setup.sh --quick  # Skip initial build
```

This script:
- Installs the toolchain versions pinned in `.settings/bundles.store.json` (via `cube`)
- Installs pre-commit hooks
- Configures CMake
- Runs an initial build to verify everything works

## Prerequisites

### Required

- **VS Code + STM32Cube for Visual Studio Code extension** (`stmicroelectronics.stm32-vscode-extension`, recommended automatically when you open the repo). It provides the `cube` CLI, which downloads CMake, Ninja, the GNU tools for STM32 and STM32CubeProgrammer as versioned *bundles*.
- **Git** (on Windows, [Git for Windows](https://git-scm.com/download/win) for Git Bash)

STM32CubeCLT is **not** required. The toolchain versions are pinned once, at the repo root, in `.settings/bundles.store.json`; VS Code and the scripts in `scripts/` both use those pins.

### Optional

- **STM32CubeMX** -- for modifying `.ioc` peripheral configurations
- **Python 3** -- only needed to regenerate CAN message definitions; dependencies are managed automatically (see [CAN Definitions](#can-definitions))
- **clang-format** -- not installed by hand; `./scripts/format.sh` fetches the pinned 19.1.2 into `build/.tools/` (needs Python 3)

### Environment Setup

1. Install the extension and open the repo folder in VS Code once, so it installs the `cube` CLI.
2. Put `cube` on your terminal PATH (it lives in `~/.local/stm32cube/bin`):
   ```bash
   # zsh / bash (~/.zshrc, ~/.bashrc)
   export PATH="$HOME/.local/stm32cube/bin:$PATH"
   # fish
   fish_add_path ~/.local/stm32cube/bin
   ```
3. Download the pinned bundles from the repo root:
   ```bash
   cube bundle install --project
   ```

Do **not** set `CUBE_BUNDLE_PATH`. Older versions of this README and `setup.sh` pointed it at the STM32CubeCLT folder, which hides every bundle from `cube` (`No such command 'cmake'`) and makes VS Code's CMake configure fail. If your shell profile has an `export CUBE_BUNDLE_PATH=...` line (or a `# STM32CubeCLT tools (added by FEB setup script)` block), delete it and fully quit and reopen VS Code.

### Verify Your Setup

```bash
cube --resolve cmake        # pinned CMake, plus Ninja/GCC on its PATH
cube --resolve programmer   # pinned STM32CubeProgrammer
./scripts/build.sh --check  # should report "Toolchain source: STM32Cube bundles"
```

### Windows Development

The repository scripts are bash-based and work with Git Bash. The steps above apply unchanged; run `./scripts/setup.sh` from Git Bash. Alternatively, use [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install) with Ubuntu.

## STM32CubeMX Code Generation

Generate HAL code from `.ioc` files without opening the STM32CubeMX GUI:

```bash
./scripts/cubemx.sh                     # Interactive menu
./scripts/cubemx.sh -g -b BMS           # Generate code for BMS
./scripts/cubemx.sh -i -b LVPDB         # Inspect LVPDB configuration
./scripts/cubemx.sh -a -g               # Generate code for all boards
./scripts/cubemx.sh --list-boards       # List all boards with .ioc status
```

The script can:
- **Generate code** (`-g`): Run STM32CubeMX headlessly to regenerate HAL code
- **Inspect** (`-i`): View MCU, clock, and peripheral configuration
- **Show pins** (`--show-pins`): Display GPIO pin assignments
- **Show peripherals** (`--show-peripherals`): List enabled peripherals

> **Note:** Code generation requires STM32CubeMX. Inspection commands work without it by parsing `.ioc` files directly.

## Code Formatting

The project uses `clang-format` with an LLVM-based style (2-space indent, 120-char lines, Allman braces). The `.clang-format` file in the repo root is automatically detected by editors and tools.

Everything formats with **clang-format 19.1.2**:

- **VS Code** formats C and C++ on save with the STM32Cube clangd extension (`starm-clangd` 19.1.2, pinned in `.settings/bundles.store.json`). This is already configured in the workspace settings.
- **`./scripts/format.sh`, pre-commit and CI** use the matching `clang-format==19.1.2` from PyPI, installed automatically into `build/.tools/` (requires `python3`).

If you bump the `st-arm-clangd` pin, update `CLANG_FORMAT_VERSION` in `scripts/format.sh` to the same version.

### Format Script

Format all user code across all boards:

```bash
./scripts/format.sh           # Format all Core/User/ files
./scripts/format.sh --check   # Check only (CI mode, exits 1 if changes needed)
```

## Pre-commit Hooks

The repository includes pre-commit hooks that run automatically before each commit:

```bash
./scripts/setup-hooks.sh          # Install hooks
./scripts/setup-hooks.sh --remove # Remove hooks
```

Hooks include:
- **clang-format** - Formats C code in `Core/User/`
- **cppcheck** - Static analysis for warnings and portability issues
- **CAN validation** - Verifies generated CAN files match definitions
- **Trailing whitespace** and **end-of-file** fixers

To run hooks manually:

```bash
pre-commit run --all-files     # Run all hooks on all files
pre-commit run clang-format    # Run specific hook
git commit --no-verify         # Skip hooks (use sparingly)
```

## Build

All builds are driven from the project root.

### Using the Build Script (Recommended)

```bash
./scripts/build.sh                # Interactive menu
./scripts/build.sh -a             # Build all boards (Debug)
./scripts/build.sh -b LVPDB       # Build specific board
./scripts/build.sh -b LVPDB -b PCU  # Build multiple boards
./scripts/build.sh -r             # Build in Release mode
./scripts/build.sh -c             # Clean and rebuild
./scripts/build.sh -l             # Loop mode (build multiple boards interactively)
```

The script validates prerequisites, shows build progress, and displays Flash/RAM usage summary.

### Using CMake Presets

```bash
# Configure (Debug) -- configures all boards
cmake --preset Debug

# Build a specific board
cmake --build build/Debug --target LVPDB
```

### Manual CMake

```bash
cmake -S . -B build/Debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake

cmake --build build/Debug --target LVPDB
```

### Building a Specific Board

Use `--target` to build a single board:

```bash
cmake --build build/Debug --target BMS
```

Build all boards at once by omitting `--target`:

```bash
cmake --build build/Debug
```

In VSCode with the CMake Tools extension, select the target from the **Build Target** dropdown in the status bar.

### Build Outputs

After a successful build, outputs are in `build/Debug/<BOARD>/`:

- `<BOARD>.elf` -- ELF executable (for debugging)
- `<BOARD>.bin` -- Raw binary (for flashing)
- `<BOARD>.hex` -- Intel HEX (for flashing)
- `<BOARD>.map` -- Linker map file

## Flashing

### Using Pre-built Firmware (Easiest)

Download the latest firmware zip from [GitHub Releases](https://github.com/FormulaElectricBerkeley/FEB_FIRMWARE_SN5/releases):

1. Download `FEB_Firmware_latest.zip` (or a versioned release)
2. Extract the zip
3. Run:
   ```bash
   chmod +x flash.sh
   ./flash.sh
   ```
4. Select the board you want to flash from the interactive menu

The zip contains firmware for all boards organized in subdirectories, plus the flash script which auto-detects available firmware.

### Using the Flash Script (For Development)

The `scripts/flash.sh` script provides an easy interface for flashing boards with prerequisite checking and interactive selection:

```bash
./scripts/flash.sh                    # Interactive menu (shows build timestamps)
./scripts/flash.sh -b LVPDB           # Flash specific board
./scripts/flash.sh -f firmware.elf    # Flash specific file
./scripts/flash.sh -l                 # Loop mode (flash multiple boards)
./scripts/flash.sh --list-probes      # List connected programmers
./scripts/flash.sh -h                 # Show help
```

> **Note:** If you downloaded `flash.sh` from a GitHub release, make it executable first: `chmod +x flash.sh`

The script runs the pinned STM32CubeProgrammer bundle through `cube` (falling back to `STM32_Programmer_CLI` on PATH) and prints setup instructions if neither is available. If the firmware hasn't been built yet, it offers to build it for you.

### Using STM32CubeProgrammer Directly

```bash
cube programmer --connect port=swd --download build/Debug/LVPDB/LVPDB.elf -hardRst -rst --start
```

### Using VSCode

If you have the STM32 VSCode extension installed, use the **CubeProg: Flash project (SWD)** task (defined in `.vscode/tasks.json`).

## CAN Definitions

The CAN message definitions live in `common/FEB_CAN_Definitions/`. It provides auto-generated C pack/unpack functions from Python message definitions. The generated files in `gen/` are committed, so you do not need to regenerate them to build.

### Regenerating CAN Code

After modifying Python message definitions in `common/FEB_CAN_Definitions/msg_defs/*_messages.py`:

```bash
cd common/FEB_CAN_Definitions
./generate_can.sh
```

The script automatically manages a Python virtual environment and installs the correct version of cantools.

### Useful Commands

| Command | Description |
|---------|-------------|
| `./generate_can.sh` | Regenerate all CAN files |
| `./generate_can.sh --list` | List all messages with frame IDs |
| `./generate_can.sh --ids` | Show frame ID allocation map |
| `./generate_can.sh --check` | CI mode: verify files are up to date |

See `common/FEB_CAN_Definitions/README.md` for detailed documentation on adding new CAN messages.

## CI/CD

GitHub Actions runs on pushes and pull requests to `main`:

| Workflow | Trigger | Description |
|----------|---------|-------------|
| **Build** (`build.yml`) | Push/PR to main | Matrix build of all 9 boards. Skips boards missing `CMakeLists.txt` or `Core/`. |
| **Code Quality** (`quality.yml`) | Push/PR to main | `clang-format` on `Core/User/` files, `cppcheck` static analysis. |
| **CAN Validation** (`can-validate.yml`) | Push/PR to main | Validates that the committed generated CAN files match the definitions. |
| **Firmware Size** (`size.yml`) | Push/PR to main | Tracks Flash/RAM usage per board. Warns at 90%, fails at 98%. |
| **Security** (`security.yml`) | Push/PR to main, weekly | CodeQL security analysis + TruffleHog secret scanning. |
| **Latest Release** (`latest-release.yml`) | Push to main | Auto-updates "latest" pre-release with current firmware binaries. |
| **Tagged Release** (`release.yml`) | Tag `v*` | Builds Release binaries, creates versioned GitHub Release. |

### Flash Size Limits

| Board | Flash Limit | MCU |
|-------|-------------|-----|
| BMS, PCU, LVPDB, DCU, DCU_Receiver, Sensor_Nodes | 512 KB | STM32F446RE |
| DASH | 2 MB | STM32F469NI |

## Releases

### Downloading Firmware

Pre-built firmware binaries are available from [GitHub Releases](https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5/releases):

- **Latest Build**: The `latest` pre-release is automatically updated on every push to `main`. Download from:
  ```
  https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5/releases/tag/latest
  ```

- **Versioned Releases**: Stable releases are tagged with version numbers (e.g., `v1.0.0`).

Each release includes:
- `.elf`, `.bin`, and `.hex` files for all 9 boards
- `FEB_Firmware_{version}.zip` - Complete firmware package with bundled `flash.sh`
- `SHA256SUMS.txt` - Checksums for individual firmware files
- `.zip.sha256` - Checksum for the release archive

### Verifying Downloads

Verify integrity of downloaded files using the SHA256 checksums:

```bash
# Verify the zip archive
sha256sum -c FEB_Firmware_latest.zip.sha256

# Verify individual firmware files
sha256sum -c SHA256SUMS.txt
```

### Versioning

Firmware versions are **CalVer, derived from git at build time**. Nothing is bumped by hand and there are no `VERSION` files:

```
2026.11.08+4d7717a         # UTC date of the commit + short commit hash
2026.11.08+4d7717a-dirty   # built with uncommitted changes
```

The same commit always produces the same version, whoever builds it. A board's `version` console command prints it along with the branch, full SHA, build time and flash stamp; paste the hash into `git show` to see exactly what is running.

### Creating a Release

Tags are optional labels for builds that matter (a test day, a competition). Pushing a tag that starts with `v` runs the Tagged Release workflow, which builds Release binaries and publishes a GitHub Release:

```bash
git tag v2026.11.15            # CalVer date of the release; add .1, .2 for more on the same day
git push origin v2026.11.15
```

## Code Organization

### Shared Code

The `common/` directory contains shared code across all boards. Start at [`common/README.md`](common/README.md) for the full library index. Each library has its own README; see the **Common Libraries** table above for quick links.

### Board Directory Structure

Within each board directory:

```
Core/
  Inc/          # CubeMX-generated headers (do not edit manually)
  Src/          # CubeMX-generated source (do not edit manually)
  User/
    Inc/        # Your headers
    Src/        # Your source code
Drivers/
  STM32F4xx_HAL_Driver/   # HAL library (included in repo)
  CMSIS/                  # CMSIS library (included in repo)
```

All custom application code goes in `Core/User/`. CubeMX-generated files in `Core/Inc/` and `Core/Src/` should only be modified through STM32CubeMX to avoid losing changes on regeneration.
