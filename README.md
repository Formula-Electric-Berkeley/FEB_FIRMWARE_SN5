# FEB Firmware SN5

Firmware for the FEB SN5 Formula E vehicle. Each subdirectory corresponds to a board on the car, built around STM32F4 microcontrollers.

## Project Structure

| Directory | Board | MCU | Notes |
|-----------|-------|-----|-------|
| `LVPDB/` | Low Voltage Power Distribution Board | STM32F446RE | |
| `BMS/` | Battery Management System | STM32F446RE | |
| `DASH/` | Dashboard | STM32F469RE | FreeRTOS, FatFS |
| `DART/` | DART | STM32F446RE | |
| `DCU/` | DCU | STM32F446RE | |
| `PCU/` | PCU | STM32F446RE | |
| `Sensor_Nodes/` | Sensor Nodes | STM32F446RE | |

All boards are fully buildable with CMake.

```
cmake/                           # Shared toolchain files
  gcc-arm-none-eabi.cmake        # ARM GCC cross-compiler config
common/
  FEB_CAN_Library_SN4/           # CAN library (git submodule)
    gen/                         # Generated C pack/unpack code
    *_messages.py                # Python message definitions
    generate_can.sh              # Generation script
scripts/
  flash.sh                       # Board flashing script
  version.sh                     # Version tagging script
.github/workflows/               # CI/CD pipelines
```

## Cloning the Repository

```bash
# Clone with submodules (required for CAN library)
git clone --recursive https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5.git

# If already cloned without --recursive:
git submodule update --init --recursive
```

## Prerequisites

### Required

- **STM32CubeCLT** -- bundles ARM GCC, CMake, Ninja, and STM32_Programmer_CLI
  - Download from [ST website](https://www.st.com/en/development-tools/stm32cubeclt.html)

### Optional

- **STM32CubeMX** -- for modifying `.ioc` peripheral configurations
- **Python 3** -- only needed to regenerate CAN message definitions; dependencies are managed automatically (see [CAN Library](#can-library))
- **clang-format** -- for code formatting (enforced in CI)

### Environment Setup

Set `CUBE_BUNDLE_PATH` to your STM32CubeCLT install directory. This is required for VSCode IntelliSense to find the cross-compiler's system headers (`stdio.h`, etc.).

Add to your shell profile (`~/.zshrc` or `~/.bashrc`):

```bash
export CUBE_BUNDLE_PATH=/opt/ST/STM32CubeCLT_1.19.0
```

Then restart your terminal (and VSCode, if open).

### Verify Your Setup

```bash
arm-none-eabi-gcc --version
cmake --version    # 3.22+
ninja --version
echo $CUBE_BUNDLE_PATH   # should print your CubeCLT path
```

## Code Formatting

The project uses `clang-format` with an LLVM-based style (2-space indent, 120-char lines, Allman braces). The `.clang-format` file in the repo root is automatically detected by editors and tools.

### Format on Save (VSCode)

1. Install the **C/C++** extension (Microsoft) or **clang-format** extension
2. Add to your `.vscode/settings.json` (workspace or user):
   ```json
   {
     "editor.formatOnSave": true,
     "[c]": {
       "editor.defaultFormatter": "ms-vscode.cpptools"
     },
     "[cpp]": {
       "editor.defaultFormatter": "ms-vscode.cpptools"
     }
   }
   ```

### Format Script

Format all user code across all boards:

```bash
./scripts/format.sh           # Format all Core/User/ files
./scripts/format.sh --check   # Check only (CI mode, exits 1 if changes needed)
```

## Build

All builds are driven from the project root.

### Using CMake Presets (recommended)

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

### Using the Flash Script (Recommended)

The `scripts/flash.sh` script provides an easy interface for flashing boards with prerequisite checking and interactive selection:

```bash
./scripts/flash.sh                    # Interactive menu (shows build timestamps)
./scripts/flash.sh -b LVPDB           # Flash specific board
./scripts/flash.sh -f firmware.elf    # Flash specific file
./scripts/flash.sh -l                 # Loop mode (flash multiple boards)
./scripts/flash.sh --list-probes      # List connected programmers
./scripts/flash.sh -h                 # Show help
```

The script checks for STM32CubeCLT installation and provides platform-specific setup instructions if needed. If the firmware hasn't been built yet, it offers to build it for you.

### Using STM32_Programmer_CLI Directly

```bash
STM32_Programmer_CLI --connect port=swd --download build/Debug/LVPDB/LVPDB.elf -hardRst -rst --start
```

### Using VSCode

If you have the STM32 VSCode extension installed, use the **CubeProg: Flash project (SWD)** task (defined in `LVPDB/.vscode/tasks.json`).

## CAN Library

The CAN message library is a **git submodule** located at `common/FEB_CAN_Library_SN4/`. It provides auto-generated C pack/unpack functions from Python message definitions. The generated files are committed to the submodule, so you do not need to regenerate them to build.

### Regenerating CAN Code

After modifying Python message definitions in `common/FEB_CAN_Library_SN4/*_messages.py`:

```bash
cd common/FEB_CAN_Library_SN4
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

### Updating the Submodule

If upstream CAN library changes:

```bash
git submodule update --remote common/FEB_CAN_Library_SN4
```

See `common/FEB_CAN_Library_SN4/README.md` for detailed documentation on adding new CAN messages.

## CI/CD

GitHub Actions runs on pushes and pull requests to `main`:

| Workflow | Trigger | Description |
|----------|---------|-------------|
| **Build** (`build.yml`) | Push/PR to main | Matrix build of all 7 boards. Skips boards missing `CMakeLists.txt` or `Core/`. |
| **Code Quality** (`quality.yml`) | Push/PR to main | `clang-format` on `Core/User/` files, `cppcheck` static analysis. |
| **CAN Validation** (`can-validate.yml`) | Push/PR to main | Checks submodule is up-to-date with upstream, validates generated files match definitions. |
| **Firmware Size** (`size.yml`) | Push/PR to main | Tracks Flash/RAM usage per board. Warns at 90%, fails at 98%. |
| **Latest Release** (`latest-release.yml`) | Push to main | Auto-updates "latest" pre-release with current firmware binaries. |
| **Tagged Release** (`release.yml`) | Tag `v*` | Builds Release binaries, creates versioned GitHub Release. |

### Flash Size Limits

| Board | Flash Limit | MCU |
|-------|-------------|-----|
| BMS, PCU, LVPDB, DART, DCU, Sensor_Nodes | 512 KB | STM32F446RE |
| DASH | 2 MB | STM32F469RE |

## Releases

### Downloading Firmware

Pre-built firmware binaries are available from [GitHub Releases](https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5/releases):

- **Latest Build**: The `latest` pre-release is automatically updated on every push to `main`. Download from:
  ```
  https://github.com/Formula-Electric-Berkeley/FEB_FIRMWARE_SN5/releases/tag/latest
  ```

- **Versioned Releases**: Stable releases are tagged with version numbers (e.g., `v1.0.0`).

Each release includes `.elf`, `.bin`, and `.hex` files for all 7 boards.

### Creating a Versioned Release

Use the version script to create and push a new version tag:

```bash
./scripts/version.sh              # Auto-increment patch (v1.0.0 → v1.0.1)
./scripts/version.sh patch        # Same as above
./scripts/version.sh minor        # Bump minor (v1.0.1 → v1.1.0)
./scripts/version.sh major        # Bump major (v1.1.0 → v2.0.0)
./scripts/version.sh 2.5.0        # Set explicit version (→ v2.5.0)
```

This automatically:
1. Creates a git tag with the new version
2. Pushes the tag to GitHub
3. Triggers the Tagged Release workflow
4. Creates a GitHub Release with all firmware binaries

## Code Organization

### Shared Code

The `common/` directory contains shared code across all boards:

```
common/
  FEB_CAN_Library_SN4/      # Git submodule - CAN message definitions
    gen/                    # Generated C code (feb_can.c, feb_can.h)
    *_messages.py           # Python message definitions per board
    generate_can.sh         # Generation script
```

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
