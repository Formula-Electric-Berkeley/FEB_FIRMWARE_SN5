# BMS Firmware Project

A Battery Management System (BMS) firmware project for STM32F4xx microcontrollers, featuring comprehensive unit testing and AI agent development support.

## Project Overview

This project provides a robust BMS firmware implementation with focus on:
- **Safety-critical operation** with proper error handling
- **FreeRTOS Real-Time Operating System** with thread-safe printf redirection
- **printf redirection** to UART with thread safety and ISR support
- **Comprehensive unit testing** with Google Test framework and FreeRTOS mocking
- **Cross-platform development** support (Windows, macOS, Linux)
- **AI agent compatibility** with detailed documentation for automated development

## Features

### Core Firmware
- **STM32F4xx HAL Integration**: Standard peripheral library support
- **FreeRTOS Integration**: Real-time multitasking with thread-safe operations
- **UART Communication**: Thread-safe printf redirection for debugging and data output
- **ISR-Safe Logging**: Queue-based printf for interrupt service routines
- **Modular Architecture**: Clean separation between user code and auto-generated code
- **Error Handling**: Robust error detection and reporting with timeout protection

### Testing Framework
- **45+ Unit Tests**: Comprehensive test coverage including FreeRTOS scenarios
- **Mock STM32 HAL**: Complete hardware abstraction layer mocking
- **Mock FreeRTOS**: Thread safety, mutex, and queue testing simulation
- **Cross-platform Testing**: Works on Windows, macOS, and Linux
- **VSCode Integration**: Built-in test runner and debugging support

### AI Agent Support
- **Specialized Documentation**: Tailored guidelines for Claude, Gemini, and other AI agents
- **Code Generation Rules**: Strict policies for auto-generated vs user code
- **STM32CubeMX Integration**: Proper .ioc file handling procedures

## Quick Start

### Prerequisites

**Required for All Platforms:**
- CMake 3.14+
- C/C++ compiler (GCC, Clang, or MSVC)
- Git with submodule support
- Python 3.8+ (for CAN library generation)

**CAN Library Generation (Optional):**
- Python `cantools` package: `pip install cantools`
- **Recommended**: Conda with dedicated environment:
  ```bash
  conda create -n feb_can python=3.8
  conda activate feb_can
  pip install cantools
  ```

**Platform-specific Installation:**
```bash
# Linux (Ubuntu/Debian)
sudo apt install cmake build-essential git python3 python3-pip

# macOS  
brew install cmake python3
xcode-select --install

# Windows
# Install Visual Studio 2019+ or MinGW-w64
# Install CMake from https://cmake.org
# Install Python 3.8+ from https://python.org
```

**Hardware Tools (for flashing):**
- STM32CubeProgrammer with CLI tools in PATH

### Building Firmware

```bash
# Configure and build STM32 firmware
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake
make
```

### Running Tests

```bash
# Quick test run
./run_tests.sh

# VSCode optimized runner
./run_vscode_tests.sh          # Unix/macOS
run_vscode_tests.bat           # Windows
```

### VSCode Development

1. Open project in VSCode
2. Install recommended extensions (C/C++, CMake Tools)
3. Select appropriate configuration:
   - **STM32**: For firmware development
   - **Tests**: For unit test development
4. Use built-in tasks: `Cmd+Shift+P` → "Tasks: Run Task"

## Project Structure

```
BMS/
├── Core/                      # STM32 generated and system code
│   ├── Inc/                   # System headers
│   ├── Src/                   # System source files
│   └── User/                  # User-modifiable code
│       ├── Inc/
│       │   └── printf_redirect.h
│       └── Src/
│           └── printf_redirect.c
├── Drivers/                   # STM32 HAL drivers
├── scripts/                   # Cross-platform development automation
│   ├── *.sh                   # Unix/Linux/macOS shell scripts
│   ├── *.bat                  # Windows batch scripts
│   ├── dev.sh/.bat            # Unified development workflow
│   ├── submodule.sh/.bat      # Git submodule management
│   └── generate_can.sh/.bat   # CAN library generation
├── FEB_CAN_Library_SN4/       # Git submodule for CAN message definitions
│   ├── *_messages.py          # Python CAN message definitions (editable)
│   ├── generate.py            # CAN library generation script
│   └── gen/                   # Generated C/H files (auto-generated)
│       ├── feb_can.c          # Generated CAN functions
│       ├── feb_can.h          # Generated CAN headers
│       └── FEB_CAN.dbc        # Generated DBC database
├── cmake/                     # CMake toolchain files
├── test/                      # Unit testing framework
│   ├── unit/                  # Unit test files
│   ├── mocks/                 # STM32 HAL mocks
│   └── build/                 # Test build directory
├── .vscode/                   # VSCode configuration
├── build/                     # Firmware build directory
├── BMS.ioc                    # STM32CubeMX configuration
├── CMakeLists.txt             # Main CMake configuration
├── README.md                  # This file
├── TESTING.md                 # Detailed testing guide
├── CLAUDE.md                  # Claude AI agent guidelines
├── AGENTS.md                  # General AI agent guidelines
└── GEMINI.md                  # Gemini AI agent guidelines
```

## Testing

The project includes **45+ comprehensive unit tests** covering:

### Printf Redirection Tests (26 tests)
- Character and string transmission
- Formatted output (printf, snprintf)
- UART HAL integration
- Error handling and edge cases
- Cross-platform compatibility

### FreeRTOS-Specific Tests (19 tests)
- Thread-safe printf with mutex protection
- ISR-safe printf using FreeRTOS queues
- Initialization and deinitialization
- Mutex timeout handling
- Concurrent access scenarios
- Error recovery with RTOS protection
- High-frequency stress testing

### Integration Tests (3 tests)
- Complete message flow
- Mixed output methods
- Error recovery scenarios

## Development Scripts

The project provides a comprehensive **cross-platform script system** with identical functionality on Unix/Linux/macOS (`.sh`) and Windows (`.bat`).

### Core Development Scripts

Execute specific development tasks independently:

```bash
# Clean all build directories  
./scripts/clean.sh              # clean.bat on Windows

# Build STM32 firmware
./scripts/build.sh              # build.bat on Windows  

# Flash firmware to connected device
./scripts/flash.sh              # flash.bat on Windows

# Run unit test suite
./scripts/test.sh               # test.bat on Windows
```

### CAN Library Management

Manage the FEB CAN Library submodule and automatic C/H file generation:

```bash
# Initialize submodule (after first clone)
./scripts/submodule.sh --init           # submodule.bat --init on Windows

# Update submodule and regenerate CAN library files
./scripts/submodule.sh --update         # submodule.bat --update on Windows

# Check submodule status
./scripts/submodule.sh --status         # submodule.bat --status on Windows

# Reset submodules to committed versions
./scripts/submodule.sh --reset          # submodule.bat --reset on Windows

# Manually regenerate CAN library files
./scripts/generate_can.sh               # generate_can.bat on Windows

# Show all submodule management options
./scripts/submodule.sh --help           # submodule.bat --help on Windows
```

#### CAN Library Integration Workflow

The project uses the FEB_CAN_Library_SN4 submodule to automatically generate C/H files from Python CAN message definitions:

```
FEB_CAN_Library_SN4/                    # Git submodule
├── bms_messages.py                     # BMS CAN message definitions (editable)
├── pcu_messages.py                     # PCU CAN message definitions (editable)
├── dash_messages.py                    # Dashboard message definitions (editable)
├── generate.py                         # Main generation script
└── gen/                                # Auto-generated files (DO NOT EDIT)
    ├── FEB_CAN.dbc                     # Generated DBC database
    ├── feb_can.c                       # Generated C source
    └── feb_can.h                       # Generated C headers
```

**Automatic Generation Process**:
1. Python message definitions define CAN message structure
2. `generate.py` creates DBC file from Python definitions
3. `cantools` converts DBC to C source and header files
4. Generated files are automatically included in CMake build system

**Environment Requirements**:
- **Conda (Recommended)**: Create `feb_can` environment with `cantools`
- **System Python**: Fallback if Conda unavailable, requires `pip install cantools`
- Scripts automatically detect and use appropriate Python environment

### Unified Development Workflow

Automate complete development workflows with automatic submodule updates:

```bash
# Show all available options
./scripts/dev.sh --help                 # dev.bat --help on Windows

# Complete development cycle (submodule → clean → build → test → flash)
./scripts/dev.sh --all                  # dev.bat --all on Windows

# Development cycle without hardware (submodule → clean → build → test)
./scripts/dev.sh --dev                  # dev.bat --dev on Windows

# Individual workflow combinations
./scripts/dev.sh -s                     # Update submodules only
./scripts/dev.sh -bf                    # Build then flash
./scripts/dev.sh -bt                    # Build then test  
./scripts/dev.sh -cbt                   # Clean, build, then test

# Preview commands without execution
./scripts/dev.sh --dry-run --all        # dev.bat --dry-run --all on Windows
```

### Cross-Platform Features

**Script Equivalency**: All `.sh`/`.bat` pairs provide identical functionality:
- ✅ Same command-line arguments and options (-c, --clean, -b, --build, etc.)
- ✅ Identical error handling and exit codes (proper error propagation)
- ✅ Consistent help documentation and output format (matching --help text)
- ✅ File size reporting in build and flash operations
- ✅ Automatic requirement detection (compilers, tools, Python/Conda)
- ✅ Enhanced error messages with actionable troubleshooting advice

**CAN Library Integration**:
- 🔄 **Automatic Generation**: Submodule updates trigger C/H file generation
- 🐍 **Python Environment**: Auto-detects Conda `feb_can` environment or system Python
- 📦 **Dependency Management**: Checks for `cantools` availability with helpful error messages
- 🔧 **Build Integration**: Generated files automatically included in CMake build

### Legacy Scripts

For backward compatibility:

```bash
./scripts/run_tests.sh         # run_tests.bat - Complete test suite
./run_vscode_tests.sh          # VSCode optimized (Unix only)
```

**Debug tests:**
- VSCode: Press `F5` → Select debug configuration
- Command line: `gdb ./test/build/bms_tests`

## FreeRTOS Integration

This project uses **FreeRTOS** for real-time multitasking with thread-safe printf redirection and task management.

### FreeRTOS Configuration

**Task Definition via STM32CubeMX (.ioc file):**
- FreeRTOS is **enabled in BMS.ioc** with heap management (USE_FreeRTOS_HEAP_4)
- All FreeRTOS tasks must be **defined in BMS.ioc** through STM32CubeMX
- Task parameters (stack size, priority, entry function) are configured in the .ioc file
- **ISR printf support**: "printfISRTask" and "printfISRQueue" are now defined in BMS.ioc
- After modifying tasks in .ioc, **regenerate code** using STM32CubeMX to update task creation

**printf Redirection Features:**
- **Thread-Safe Functions**: `uart_printf_safe()`, `debug_printf_safe()` with mutex protection
- **ISR-Safe Functions**: `uart_printf_isr()` uses .ioc-defined FreeRTOS queue for interrupt-safe logging
- **Initialization**: Call `printf_redirect_init()` to setup mutexes (queue/task defined in .ioc)
- **Task Management**: Printf ISR task defined in .ioc configuration, not created programmatically

### FreeRTOS Development Workflow

1. **Configure Tasks and Queues in STM32CubeMX**:
   ```
   Open BMS.ioc → FreeRTOS tab → Tasks tab → Add/modify tasks
   Set task parameters: Name, Priority, Stack Size, Entry Function
   
   For ISR printf support:
   - Queues tab → Add "PrintfISRQueue" (10 items, 68 bytes each)
   - Tasks tab → Add "PrintfISRTask" (Entry: "StartPrintfISRTask")
   ```

2. **Regenerate Code**:
   ```
   STM32CubeMX → Project → Generate Code
   This updates task creation functions in main.c
   ```

3. **Initialize printf System**:
   ```c
   /* USER CODE BEGIN 2 */
   printf_redirect_init();  // Setup thread-safe printf
   /* USER CODE END 2 */
   ```

4. **Use Thread-Safe printf**:
   ```c
   // In FreeRTOS tasks
   debug_printf_safe("Task running: %s\\n", pcTaskGetName(NULL));
   uart_printf_safe(&huart2, "Sensor: %d\\n", sensor_value);
   
   // In interrupt service routines
   uart_printf_isr("ISR triggered: %d\\n", interrupt_count);
   ```

### FreeRTOS Configuration Details

- **Scheduler**: Preemptive scheduling enabled
- **Heap**: USE_FreeRTOS_HEAP_4 with 15360 bytes total heap
- **Tick Rate**: 1000 Hz (1ms tick)
- **Max Priorities**: 56 priority levels
- **Stack Sizes**: Configurable per task in .ioc file
- **Middleware**: CMSIS-RTOS v2 wrapper enabled

## Development Guidelines

### Code Organization

**User Code Locations:**
- `Core/User/Inc/` - User header files
- `Core/User/Src/` - User source files
- `/* USER CODE BEGIN */` blocks in generated files

**Auto-generated (DO NOT EDIT):**
- Most of `Core/Inc/` and `Core/Src/`
- `Drivers/` directory
- Areas outside `/* USER CODE */` blocks

### STM32CubeMX Workflow

1. **Edit Configuration**: Modify `BMS.ioc` in STM32CubeMX
2. **Generate Code**: Use STM32CubeMX to regenerate project
3. **Preserve User Code**: Generator preserves `/* USER CODE */` sections
4. **Test Changes**: Run unit tests to verify functionality

### Adding New Features

1. **Create User Code**: Add to `Core/User/` directory
2. **Write Tests**: Add corresponding test files to `test/unit/`
3. **Update Build**: Modify `test/CMakeLists.txt` if needed
4. **Verify**: Run full test suite

## AI Agent Development

This project includes specialized documentation for AI agents:

- **[CLAUDE.md](CLAUDE.md)**: Guidelines for Claude AI agents
- **[AGENTS.md](AGENTS.md)**: General AI development guidelines  
- **[GEMINI.md](GEMINI.md)**: Guidelines for Gemini AI agents

Key principles for AI agents:
- ✅ Edit user code in `Core/User/` directories
- ✅ Modify `/* USER CODE */` sections
- ✅ Edit `.ioc` files and request regeneration
- ❌ Never modify auto-generated code outside user sections
- ❌ Don't edit STM32 HAL driver files
- ❌ Avoid changing generated CMake files

## Hardware Support

### Supported Microcontrollers
- **Primary**: STM32F446xx series
- **Compatible**: Other STM32F4xx series (with configuration changes)

### Peripheral Support
- **UART**: printf redirection and communication
- **GPIO**: Digital I/O control
- **ADC**: Analog signal reading (mockable for testing)
- **Timers**: PWM and timing functions

### Development Boards
- STM32F446RE Nucleo board
- Custom STM32F4xx designs
- Any STM32F4xx board with UART available

## Contributing

### Before Submitting Changes
1. **Run Tests**: Ensure all 26 tests pass
2. **Follow Guidelines**: Respect auto-generated vs user code boundaries
3. **Update Documentation**: Modify relevant .md files
4. **Test Cross-platform**: Verify on multiple operating systems

### Coding Standards
- **C Standard**: C11 for firmware code
- **C++ Standard**: C++14 for test code
- **Formatting**: Follow existing code style
- **Comments**: Document public APIs and complex logic
- **Error Handling**: Always check return values

### Pull Request Process
1. Fork the repository
2. Create feature branch: `git checkout -b feature/your-feature`
3. Commit changes with clear messages
4. Run full test suite: `./run_tests.sh`
5. Submit pull request with description

## Troubleshooting

### Build Issues
```bash
# Clean rebuild
rm -rf build test/build
mkdir build && cd build
cmake .. && make
```

### Test Issues
```bash
# Reset test environment
cd test
rm -rf build
./run_tests.sh
```

### VSCode Issues
- Switch C++ configuration: "STM32" for firmware, "Tests" for testing
- Reload window: `Cmd+Shift+P` → "Developer: Reload Window"
- Check extension recommendations

## License

This project is provided as-is for educational and development purposes.

## Support

- **Issues**: Report bugs and issues in the project repository
- **Documentation**: See `TESTING.md` for detailed testing information
- **AI Development**: See `CLAUDE.md`, `AGENTS.md`, or `GEMINI.md` for AI-specific guidelines

---

**Quick Commands:**
```bash
./run_tests.sh              # Run all tests
./run_vscode_tests.sh        # VSCode test runner
make -C build               # Build firmware
```

For detailed testing information, see [TESTING.md](TESTING.md)