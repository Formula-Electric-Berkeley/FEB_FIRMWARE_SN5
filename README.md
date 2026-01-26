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
cmake/                     # Shared toolchain files
  gcc-arm-none-eabi.cmake  # ARM GCC cross-compiler config
  starm-clang.cmake        # Alternative STARM Clang config
.github/workflows/         # CI/CD pipelines
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
- **Python 3** + `cantools` -- only needed to regenerate CAN message definitions (see [CAN Library](#can-library))
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

Auto-generated CAN message packing/unpacking code lives in `LVPDB/Core/User/Inc/FEB_CAN_Library_SN4/gen/`. The generated files are committed to the repo, so you do not need to regenerate them to build.

To regenerate after changing message definitions:

```bash
cd LVPDB/Core/User/Inc/FEB_CAN_Library_SN4

# Install cantools (once)
pip install cantools

# Generate DBC file from Python message definitions
python3 generate.py

# Generate C source from DBC
sudo python3 -m cantools generate_c_source -o gen gen/FEB_CAN.dbc
```

See `LVPDB/Core/User/Inc/FEB_CAN_Library_SN4/README.md` for more details.

## CI/CD

GitHub Actions runs on pushes to `main`, `sa/*`, and `dev/*` branches, and on pull requests to `main`:

- **Build** (`build.yml`): Builds all boards that have a `CMakeLists.txt` + `Core/` directory. Boards without these are skipped with a warning.
- **Code Quality** (`quality.yml`):
  - `clang-format` check on user code (`Core/User/` only, not CubeMX-generated code)
  - `cppcheck` static analysis on `Core/` (excludes `Drivers/` and `Middlewares/`)

## Code Organization

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
