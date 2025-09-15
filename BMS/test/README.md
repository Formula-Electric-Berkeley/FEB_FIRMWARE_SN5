# BMS Unit Tests

This directory contains comprehensive unit tests for the BMS (Battery Management System) firmware using Google Test framework.

## Structure

```
test/
├── CMakeLists.txt       # CMake configuration for building tests
├── mocks/               # Mock implementations of STM32 HAL functions
│   ├── stm32f4xx_hal.h  # Mock HAL header
│   └── stm32f4xx_hal.c  # Mock HAL implementation
└── unit/                # Unit test files
    └── test_battery_monitor.cpp  # Example tests for battery monitoring functions
```

## Running Tests

### Quick Start
From the BMS directory, run:
```bash
./run_tests.sh
```

### Manual Build
```bash
cd test
mkdir build
cd build
cmake ..
make
./bms_tests
```

## Writing New Tests

### 1. Create Your Module
Place your production code in `Core/User/`:
- Headers in `Core/User/Inc/`
- Source files in `Core/User/Src/`

### 2. Handle STM32 Dependencies
Use the `UNIT_TEST` macro to conditionally compile STM32-specific code:

```c
#ifndef UNIT_TEST
#include "stm32f4xx_hal.h"
extern ADC_HandleTypeDef hadc1;
#endif

uint32_t ReadSensor(void) {
#ifdef UNIT_TEST
    return mock_sensor_value;
#else
    return HAL_ADC_GetValue(&hadc1);
#endif
}
```

### 3. Create Mock Functions
Add any new HAL functions you need to `test/mocks/stm32f4xx_hal.h` and `.c`:

```c
// In stm32f4xx_hal.h
extern uint32_t mock_gpio_state;
void Mock_SetGPIOState(uint32_t state);

// In stm32f4xx_hal.c
uint32_t mock_gpio_state = 0;
void Mock_SetGPIOState(uint32_t state) {
    mock_gpio_state = state;
}
```

### 4. Write Your Test
Create a new test file in `test/unit/`:

```cpp
#include <gtest/gtest.h>

extern "C" {
    #define UNIT_TEST
    #include "your_module.h"
    #include "stm32f4xx_hal.h"
}

TEST(YourModuleTest, TestName) {
    // Arrange
    Mock_SetSomeValue(expected_value);
    
    // Act
    result = YourFunction();
    
    // Assert
    EXPECT_EQ(result, expected_result);
}
```

### 5. Add to CMakeLists.txt
Update `test/CMakeLists.txt` to include your new files:

```cmake
set(TEST_SOURCES
    unit/test_battery_monitor.cpp
    unit/test_your_module.cpp        # Add your test file
    ../Core/User/Src/battery_monitor.c
    ../Core/User/Src/your_module.c   # Add your source file
    mocks/stm32f4xx_hal.c
)
```

## Test Examples

The `test_battery_monitor.cpp` file demonstrates:

1. **Basic unit tests** - Testing individual functions with various inputs
2. **Edge case handling** - NULL pointers, boundary values
3. **Mock usage** - Using mock ADC values for hardware-dependent code
4. **Integration tests** - Testing multiple functions working together
5. **Parameterized tests** - Testing with different test fixtures

## Tips for Testable Code

1. **Separate logic from hardware** - Keep business logic in separate functions from HAL calls
2. **Use dependency injection** - Pass HAL handles as parameters instead of using globals
3. **Return status codes** - Make functions return testable status/error codes
4. **Avoid static functions** - Make functions accessible for testing (or test through public interfaces)
5. **Use interfaces** - Define clear interfaces between modules

## Coverage Report (Optional)

To generate a coverage report:

```bash
cd test/build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage"
make
./bms_tests
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
open coverage_report/index.html  # On macOS
```

## Troubleshooting

- **Linking errors**: Make sure all source files are added to CMakeLists.txt
- **Multiple definition errors**: Check that mock functions aren't conflicting with real implementations
- **Test discovery issues**: Ensure test names follow Google Test conventions (TEST or TEST_F macros)
- **Build errors on macOS**: You may need to install Xcode Command Line Tools: `xcode-select --install`