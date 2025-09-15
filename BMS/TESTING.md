# BMS Project Testing Guide

This document explains how to set up and run tests for the BMS (Battery Management System) firmware project.

## Overview

The BMS project includes comprehensive unit testing using Google Test framework with **53 tests** covering:
- Battery monitoring and safety systems (27 tests)
- Printf redirection and UART functionality (26 tests)
- CAN library integration testing (generated C/H files from Python definitions)

## Quick Start

### Prerequisites

**All Platforms:**
- CMake 3.14 or higher
- C++ compiler supporting C++14
- Git (for fetching dependencies and submodules)
- Python 3.8+ (for CAN library generation, optional)

**CAN Library Generation (Optional):**
- Python `cantools` package: `pip install cantools`
- **Recommended**: Conda with dedicated environment:
  ```bash
  conda create -n feb_can python=3.8
  conda activate feb_can
  pip install cantools
  ```

**Platform-specific Installation:**

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt install cmake build-essential git
```

**macOS:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake (via Homebrew)
brew install cmake
```

**Windows:**
- Install Visual Studio 2019+ with C++ tools, OR
- Install MinGW-w64 or MSYS2
- Install CMake from https://cmake.org/download/

### Running Tests

#### Modern Script System (Recommended)

**Individual Test Script:**
```bash
# Clean, build, and run all tests
./scripts/test.sh                # test.bat on Windows

# Run tests with specific gtest options
./scripts/test.sh --gtest_filter="*Printf*"    # test.bat --gtest_filter="*Printf*" on Windows
./scripts/test.sh --gtest_repeat=3             # test.bat --gtest_repeat=3 on Windows
```

**Development Workflow Integration:**
```bash
# Build and test (validation workflow)
./scripts/dev.sh -bt             # dev.bat -bt on Windows

# Complete development cycle with testing (includes submodule update)
./scripts/dev.sh --all           # dev.bat --all on Windows

# Development mode (no hardware flashing)
./scripts/dev.sh --dev           # dev.bat --dev on Windows

# Clean build with testing only
./scripts/dev.sh -ct             # dev.bat -ct on Windows
```

#### Legacy Scripts (Still Available)
```bash
# Original combined scripts
./scripts/run_tests.sh              # run_tests.bat on Windows (Full test suite)
./run_vscode_tests.sh               # VSCode optimized (Unix/macOS)  
run_vscode_tests.bat                # VSCode optimized (Windows)
```

#### VSCode Integration
1. **Run Tests**: `Cmd+Shift+P` → "Tasks: Run Task" → "Run Unit Tests"
2. **Debug Tests**: Press `F5` → Select debug configuration for your platform
3. **Individual Tasks**: Available tasks: Clean, Build, Flash, Test, Development Workflow
4. **Legacy Support**: "Build and Run Tests" task still available

## Project Structure

```
BMS/
├── Core/
│   ├── Inc/                    # STM32 headers
│   ├── Src/                    # STM32 source files
│   └── User/                   # Custom firmware modules
│       ├── Inc/
│       │   ├── battery_monitor.h
│       │   └── printf_redirect.h
│       └── Src/
│           ├── battery_monitor.c
│           └── printf_redirect.c
├── scripts/                    # Development automation scripts
│   ├── *.sh                    # Unix shell scripts
│   ├── *.bat                   # Windows batch scripts
│   ├── dev.sh/.bat             # Unified development workflow
│   ├── test.sh/.bat            # Individual test script
│   ├── submodule.sh/.bat       # Git submodule management
│   └── generate_can.sh/.bat    # CAN library generation
├── FEB_CAN_Library_SN4/        # Git submodule for CAN messages
│   ├── *_messages.py           # Python CAN message definitions (editable)
│   ├── generate.py             # CAN library generation script
│   └── gen/                    # Generated C/H files (auto-generated)
│       ├── feb_can.c           # Generated CAN functions
│       ├── feb_can.h           # Generated CAN headers
│       └── FEB_CAN.dbc         # Generated DBC database
├── test/
│   ├── unit/                   # Unit test files
│   │   ├── test_battery_monitor.cpp
│   │   └── test_printf_redirect.cpp
│   ├── mocks/                  # STM32 HAL mocks
│   │   ├── stm32f4xx_hal.h
│   │   └── stm32f4xx_hal.c
│   ├── build/                  # Test build directory
│   ├── CMakeLists.txt          # Test build configuration
│   └── README.md               # Detailed test documentation
├── .vscode/                    # VSCode configuration
│   ├── c_cpp_properties.json  # IntelliSense settings
│   ├── tasks.json              # Build tasks
│   ├── launch.json             # Debug configurations
│   └── settings.json           # Editor settings
├── run_tests.sh                # Legacy combined test runner
├── run_vscode_tests.sh         # VSCode optimized test runner (Unix/macOS)
├── run_vscode_tests.bat        # VSCode optimized test runner (Windows)
├── BMS.code-workspace          # VSCode workspace
└── TESTING.md                  # This file
```

## VSCode Setup

### C++ Configurations

The project includes two IntelliSense configurations:

1. **STM32**: For main firmware development
   - Uses STM32 HAL includes
   - Configured for ARM development

2. **Tests**: For unit test development  
   - Includes GoogleTest headers
   - Mock STM32 HAL support
   - Defines `UNIT_TEST=1`

**Switch configs**: `Cmd+Shift+P` → "C/C++: Select a Configuration"

### Available Tasks

| Task | Description |
|------|-------------|
| Build and Run Tests | Complete test build and execution |
| Run Tests Only | Execute pre-built tests |
| Build Tests Only | Build test executable without running |
| Configure Tests | Reconfigure CMake build system |
| Clean Test Build | Clean build and start fresh |

**Access**: `Cmd+Shift+P` → "Tasks: Run Task"

### Debug Configurations

| Configuration | Platform | Debugger |
|---------------|----------|----------|
| Debug Tests (Linux/Mac) | Linux/Unix | GDB |
| Debug Tests (macOS) | macOS | LLDB |
| Debug Tests (Windows) | Windows | GDB |
| Debug Specific Test | All | Filter-based debugging |

**Usage**: Press `F5` and select appropriate configuration

## Test Coverage

### Battery Monitor Tests (27 tests)
- **Voltage validation**: Over/under voltage detection
- **Temperature monitoring**: Thermal protection limits
- **Current measurement**: Overcurrent protection
- **State of Charge**: SOC calculation accuracy
- **Data validation**: Error handling and edge cases
- **Integration**: Full ADC-to-real-world scenarios

### Printf Redirect Tests (26 tests)
- **Character transmission**: Single/multiple character output
- **String functions**: puts() and printf() formatting
- **UART integration**: HAL function mocking and verification
- **Error handling**: Transmission failure scenarios
- **Format support**: Various printf format specifiers
- **Integration**: Complete message flow testing

## Cross-Platform Support

### Tested Platforms
- ✅ **Linux**: Ubuntu 20.04+, other distributions
- ✅ **macOS**: 10.15+ (Intel & Apple Silicon)
- ✅ **Windows**: Windows 10+, MSYS2, MinGW, VS2019+

### Build System
- **Auto-detection**: Automatically selects best generator (Ninja → Make → VS)
- **Cross-platform scripts**: All `.sh`/`.bat` pairs provide identical functionality
- **No hardcoded paths**: Uses relative paths and CMake variables

### Script Features
**Full Cross-Platform Equivalency**:
- ✅ Same command-line arguments (-c, --clean, -b, --build, etc.)
- ✅ Identical error handling and exit codes
- ✅ Consistent help documentation and output formatting
- ✅ File size reporting in build and flash operations
- ✅ Enhanced error messages with troubleshooting advice

**CAN Library Integration Testing**:
- 🔄 Automatic C/H file generation from Python CAN message definitions
- 🐍 Conda environment detection with system Python fallback
- 📦 Dependency checking for `cantools` package with installation guidance
- 🔧 Generated files automatically included in test builds for integration validation

## Troubleshooting

### Common Issues

**CMake/Compiler Issues:**
```bash
# Check installations
cmake --version
gcc --version  # or clang --version

# Clean rebuild
cd test && rm -rf build && mkdir build
cd build && cmake .. && make
```

**VSCode IntelliSense Issues:**
1. Make sure you're using the correct C++ configuration ("STM32" vs "Tests")
2. Reload VSCode window: `Cmd+Shift+P` → "Developer: Reload Window"
3. Check that `compile_commands.json` exists in build directories

**Permission Issues (Unix/macOS):**
```bash
chmod +x *.sh  # Make scripts executable
```

**Windows Path Issues:**
- Use the `.bat` script instead of `.sh`
- Ensure CMake and compiler are in PATH

### Build Directory Problems

If you encounter build issues:
1. Delete `test/build` directory
2. Run the test script again (it will recreate everything)
3. Or manually: `cd test/build && cmake .. && cmake --build .`

### CAN Library Issues

**Submodule Problems:**
```bash
# Check submodule status
./scripts/submodule.sh --status     # submodule.bat --status on Windows

# Initialize submodules (after first clone)
./scripts/submodule.sh --init       # submodule.bat --init on Windows

# Update submodule and regenerate CAN files
./scripts/submodule.sh --update     # submodule.bat --update on Windows
```

**Python Environment Issues:**
```bash
# Check if cantools is available
python3 -c "import cantools; print('CAN tools ready')"

# Install in current environment
pip install cantools

# Or create dedicated Conda environment
conda create -n feb_can python=3.8
conda activate feb_can
pip install cantools
```

**Generated File Problems:**
- Generated files missing: Run `./scripts/generate_can.sh` (or `.bat` on Windows)
- Build can't find CAN headers: Verify generated files exist in `FEB_CAN_Library_SN4/gen/`
- CAN functions undefined: Check that `feb_can.c` is included in CMakeLists.txt

## CAN Library Testing

### CAN Message Integration Tests

The project automatically includes generated CAN library files in the test build system. To test CAN functionality:

```cpp
// test/unit/test_can_integration.cpp
#include <gtest/gtest.h>

extern "C" {
    #include "feb_can.h"        // Generated CAN library
    #include "stm32f4xx_hal.h"  // HAL mocks
}

class CANIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mock_ResetAll();
    }
};

TEST_F(CANIntegrationTest, BMSStatus_PackUnpack_SuccessfulRoundTrip) {
    // Arrange: Create test message
    bms_status_t original_msg = {
        .voltage = 42.5,      // 425 after scaling by 0.1
        .current = 15.3,      // Scaled and offset
        .temperature = 25,    // 65 after offset of -40
        .fault_flags = 0x0F
    };
    
    uint8_t can_data[8];
    bms_status_t unpacked_msg;
    
    // Act: Pack and unpack message
    int pack_result = bms_status_pack(can_data, &original_msg, sizeof(can_data));
    int unpack_result = bms_status_unpack(&unpacked_msg, can_data, sizeof(can_data));
    
    // Assert: Verify round-trip integrity
    EXPECT_GE(pack_result, 0);
    EXPECT_GE(unpack_result, 0);
    EXPECT_FLOAT_EQ(original_msg.voltage, unpacked_msg.voltage);
    EXPECT_FLOAT_EQ(original_msg.current, unpacked_msg.current);
    EXPECT_EQ(original_msg.temperature, unpacked_msg.temperature);
    EXPECT_EQ(original_msg.fault_flags, unpacked_msg.fault_flags);
}

TEST_F(CANIntegrationTest, CANTransmission_WithMocks_VerifyHALIntegration) {
    // Test integration between generated CAN functions and HAL mocks
    Mock_SetHALStatus(HAL_OK);
    
    // Your CAN transmission test using generated functions
    // and HAL mocks would go here
}
```

### CAN Library Development Workflow

1. **Modify Python Definitions**: Edit files like `bms_messages.py`
2. **Regenerate CAN Files**: `./scripts/submodule.sh --update`
3. **Write Integration Tests**: Test pack/unpack functions
4. **Run Test Suite**: `./scripts/test.sh` includes CAN tests
5. **Validate with Hardware**: Use `./scripts/dev.sh --all` for complete workflow

## Adding New Tests

### 1. Create Test File
```cpp
// test/unit/test_new_module.cpp
#include <gtest/gtest.h>

extern "C" {
    #include "new_module.h"
    #include "stm32f4xx_hal.h"  // For mocks
}

class NewModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mock_ResetAll();  // Reset all mocks
    }
};

TEST_F(NewModuleTest, BasicFunctionality) {
    // Test implementation
    EXPECT_EQ(expected_value, actual_function_call());
}
```

### 2. Update Build System
Add your test file to `test/CMakeLists.txt`:
```cmake
set(TEST_SOURCES
    unit/test_battery_monitor.cpp
    unit/test_printf_redirect.cpp
    unit/test_new_module.cpp      # Add this line
    ../Core/User/Src/battery_monitor.c
    ../Core/User/Src/printf_redirect.c
    ../Core/User/Src/new_module.c # And this line
    mocks/stm32f4xx_hal.c
)
```

### 3. Run Tests
```bash
./run_vscode_tests.sh  # Your new tests will be included
```

## Best Practices

### Test Organization
- One test file per module
- Use descriptive test names: `ModuleTest.FeatureName_Scenario`
- Group related tests in test classes
- Include both positive and negative test cases

### Mock Usage
```cpp
// Set up mock return values
Mock_SetADCValue(2048);
Mock_SetHALStatus(HAL_ERROR);

// Verify mock interactions
EXPECT_EQ(Mock_UART_GetBufferSize(), expected_size);
```

### Debugging Tests
- Set breakpoints in test files and source code
- Use `Debug Specific Test` with filters like `"PrintfRedirectTest.*"`
- Check mock buffer contents: `Mock_UART_GetBuffer()`

## Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [CMake Documentation](https://cmake.org/documentation/)
- [VSCode C++ Extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)

## Contributing

1. Ensure all existing tests pass: `./run_tests.sh`
2. Add tests for new functionality
3. Follow existing naming conventions
4. Update documentation as needed
5. Test on your target platform before submitting

---

For detailed test implementation information, see [`test/README.md`](test/README.md).