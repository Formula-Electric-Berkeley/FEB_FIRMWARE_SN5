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

- **ARM GCC Toolchain** (`arm-none-eabi-gcc`) -- must be on your `PATH`
  - Install via [ARM Developer](https://developer.arm.com/downloads/-/gnu-rm) or your package manager
  - Or install [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) which bundles it
- **CMake** >= 3.22
- **Ninja** build system

### Optional

- **STM32CubeCLT / STM32CubeIDE** -- for IDE integration, flashing, and debugging
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

### Using STM32_Programmer_CLI

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
| **Release** (`release.yml`) | Tag `v*` | Builds Release binaries, creates GitHub Release with `.elf`, `.bin`, `.hex` artifacts. |

### Flash Size Limits

| Board | Flash Limit | MCU |
|-------|-------------|-----|
| BMS, PCU, LVPDB, DART, DCU, Sensor_Nodes | 512 KB | STM32F446RE |
| DASH | 2 MB | STM32F469RE |

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
