# AI Agent Guidelines for BMS Firmware Project

This document provides comprehensive guidelines for AI agents working on the BMS (Battery Management System) firmware project. These rules ensure project integrity, maintain development standards, and prevent conflicts with auto-generated code.

## Critical Rules - READ FIRST

### 🚨 NEVER EDIT THESE LOCATIONS
- **`Drivers/` directory** - STM32 HAL drivers (auto-generated)
- **Generated code outside `/* USER CODE */` sections**
- **Startup files** (startup_stm32f4xx.s, system_stm32f4xx.c)
- **Auto-generated CMake files** (except user sections)
- **Linker scripts** (.ld files)
- **Generated CAN library files** in FEB_CAN_Library_SN4/gen/

### ✅ SAFE TO EDIT
- **`Core/User/Inc/`** - User header files
- **`Core/User/Src/`** - User source files
- **`/* USER CODE BEGIN */` to `/* USER CODE END */` sections**
- **`test/` directory** - All test-related files
- **`scripts/` directory** - Development automation scripts
- **`.vscode/` directory** - IDE configuration
- **Documentation files** (*.md)
- **`BMS.ioc`** - STM32CubeMX configuration (with regeneration)
- **Python CAN definitions** - *_messages.py files in FEB_CAN_Library_SN4/

## Project Architecture

### Directory Structure
```
BMS/
├── Core/
│   ├── Inc/                    # 🟡 Generated headers (user sections only)
│   ├── Src/                    # 🟡 Generated source (user sections only)
│   └── User/                   # 🟢 User code (full edit access)
│       ├── Inc/                # 🟢 User headers
│       └── Src/                # 🟢 User implementations
├── Drivers/                    # 🔴 Never edit - STM32 HAL
├── scripts/                    # 🟢 Development automation scripts
│   ├── *.sh                    # 🟢 Unix shell scripts
│   ├── *.bat                   # 🟢 Windows batch scripts
│   ├── dev.sh/.bat             # 🟢 Unified development workflow
│   ├── submodule.sh/.bat       # 🟢 Git submodule management
│   └── generate_can.sh/.bat    # 🟢 CAN library generation
├── FEB_CAN_Library_SN4/        # 🟡 Git submodule (see CAN section)
│   ├── *_messages.py           # 🟢 Python CAN message definitions
│   ├── generate.py             # 🟡 CAN generation script
│   └── gen/                    # 🔴 Auto-generated C/H files
├── test/                       # 🟢 Full testing framework
│   ├── unit/                   # 🟢 Unit test files
│   ├── mocks/                  # 🟢 Hardware mocks
│   └── build/                  # 🟡 Build artifacts
├── .vscode/                    # 🟢 IDE configuration
├── build/                      # 🟡 Firmware build output
├── cmake/                      # 🟡 Build system files
├── BMS.ioc                     # 🟢 Hardware configuration
└── *.md                        # 🟢 Documentation
```

Legend:
- 🟢 **Full Edit Access** - Modify freely
- 🟡 **Conditional Access** - Specific sections only
- 🔴 **No Access** - Never modify

### User Code Boundaries

#### Generated Files with User Sections
Look for these patterns:
```c
/* USER CODE BEGIN section_name */
// 🟢 YOUR CODE GOES HERE
/* USER CODE END section_name */
```

Common user sections:
- `Includes` - Add your #include statements
- `PV` - Private variables
- `0`, `1`, `2`, etc. - General user code areas
- `WHILE(1)` - Main loop additions
- `4` - Interrupt handlers

#### Example User Code Placement
```c
/* USER CODE BEGIN Includes */
#include "printf_redirect.h"
#include "custom_sensors.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static uint32_t sensor_data[16];
static volatile bool data_ready = false;
/* USER CODE END PV */

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    
    /* USER CODE BEGIN 2 */
    printf("System starting...\n");
    Sensors_Initialize();
    /* USER CODE END 2 */
    
    while (1) {
        /* USER CODE BEGIN 3 */
        if (data_ready) {
            Sensors_ProcessData();
            data_ready = false;
        }
        /* USER CODE END 3 */
    }
}
```

## FreeRTOS Development Rules

### ⚠️ CRITICAL: Task Configuration via .ioc File

**ALWAYS define FreeRTOS tasks through BMS.ioc - NEVER manually create tasks in code**

```
🚨 REQUIRED WORKFLOW:
1. Open BMS.ioc in STM32CubeMX
2. Navigate to FreeRTOS → Queues tab
3. For ISR printf: Add "PrintfISRQueue" (10 items, 68 bytes each)
4. Navigate to FreeRTOS → Tasks tab
5. Add/modify tasks with proper parameters:
   - Task Name
   - Priority (0-56, higher number = higher priority)
   - Stack Size (in words, not bytes)
   - Entry Function name
6. For ISR printf: Add "PrintfISRTask" (Entry: "StartPrintfISRTask")
7. Generate code to update main.c task creation
8. Use printf_redirect_init() in main.c USER CODE section
```

### Thread-Safe Development Patterns

**✅ CORRECT Printf Usage:**
```c
// In FreeRTOS tasks - use thread-safe functions
void BMS_MonitorTask(void *pvParameters) {
    for (;;) {
        debug_printf_safe("Battery voltage: %.2fV\n", battery_voltage);
        uart_printf_safe(&huart2, "Current: %.1fA\n", current);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// In interrupt handlers - use ISR-safe functions
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    uart_printf_isr("ADC conversion complete: %d\n", adc_value);
}
```

**❌ INCORRECT Printf Usage:**
```c
// DON'T use regular printf in FreeRTOS tasks - causes resource conflicts
void BadTask(void *pvParameters) {
    printf("This will corrupt output!\n");  // NOT thread-safe
}

// DON'T use thread-safe functions in ISRs - will block
void BadISR(void) {
    debug_printf_safe("This will hang!\n");  // Mutex in ISR = deadlock
}
```

### FreeRTOS Initialization Pattern

**Required main.c setup:**
```c
/* USER CODE BEGIN 2 */
// Initialize printf system BEFORE starting scheduler  
printf_redirect_init();  

// Verify initialization
debug_printf_safe("FreeRTOS printf system initialized\n");
/* USER CODE END 2 */

/* Start scheduler */
osKernelStart(); // or vTaskStartScheduler()
```

### Resource Management Rules

1. **Mutex Usage**: All `*_safe()` functions use internal mutex with 100ms timeout
2. **Queue Usage**: `uart_printf_isr()` uses .ioc-defined PrintfISRQueue (10 messages, 68 bytes each)  
3. **Task Usage**: Printf ISR processing handled by .ioc-defined PrintfISRTask with StartPrintfISRTask entry
4. **Stack Considerations**: Thread-safe functions use static buffers to minimize stack usage
5. **Error Handling**: Always check return values from `*_safe()` functions for timeout/error detection

## STM32CubeMX Integration

### Hardware Configuration Process

1. **Modify .ioc File**: Edit `BMS.ioc` for hardware changes (including FreeRTOS tasks)
2. **Request Regeneration**: Tell user to regenerate in STM32CubeMX
3. **Never Bypass**: Don't manually edit generated peripheral code or task creation

### Example Configuration Change
```
I need to add SPI1 peripheral for sensor communication. 
I've updated the BMS.ioc configuration file.

Please regenerate the code:
1. Open BMS.ioc in STM32CubeMX
2. Click "GENERATE CODE"
3. Choose "Yes" when prompted about overwriting
4. All USER CODE sections will be preserved

After regeneration, SPI1 will be properly initialized.
```

### Peripheral Usage Pattern
```c
/* USER CODE BEGIN 2 */
// Initialize your application after STM32CubeMX init
HAL_SPI_Init(&hspi1);  // hspi1 created by STM32CubeMX
Custom_SPI_Setup();    // Your configuration
/* USER CODE END 2 */

/* USER CODE BEGIN 0 */
// Your custom SPI functions
void Custom_SPI_Setup(void) {
    // Configure SPI for your specific use case
}

HAL_StatusTypeDef Custom_SPI_Transmit(uint8_t* data, uint16_t size) {
    return HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
}
/* USER CODE END 0 */
```

## CAN Library Management

The project uses the FEB_CAN_Library_SN4 submodule for automatic C/H file generation from Python CAN message definitions.

### CAN Library Workflow

1. **Edit Python Definitions**: Modify Python message files (e.g., bms_messages.py)
2. **Update Submodule**: Run `./scripts/submodule.sh --update` 
3. **Automatic Generation**: C/H files generated in `gen/` directory
4. **Build Integration**: Generated files automatically included in build

### CAN Library Structure
```
FEB_CAN_Library_SN4/                    # Git submodule
├── bms_messages.py                     # ✅ BMS CAN message definitions
├── pcu_messages.py                     # ✅ PCU CAN message definitions  
├── dash_messages.py                    # ✅ Dashboard message definitions
├── generate.py                         # 🟡 Main generation script
└── gen/                                # ❌ Auto-generated (DO NOT EDIT)
    ├── FEB_CAN.dbc                     # Generated DBC database
    ├── feb_can.c                       # Generated C source
    └── feb_can.h                       # Generated C headers
```

### Using Generated CAN Library

```c
/* USER CODE BEGIN Includes */
#include "feb_can.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
void App_SendBMSStatus(void) {
    // Create BMS status message
    bms_state_t bms_status = {
        .voltage = 400,      // 400V
        .current = 25,       // 25A
        .temperature = 45    // 45°C
    };
    
    // Pack message into CAN frame
    uint8_t can_data[8];
    int result = bms_state_pack(can_data, &bms_status, sizeof(can_data));
    
    if (result >= 0) {
        // Send CAN message using HAL
        CAN_TxHeaderTypeDef tx_header = {
            .StdId = BMS_STATE_FRAME_ID,
            .RTR = CAN_RTR_DATA,
            .IDE = CAN_ID_STD,
            .DLC = 8,
            .TransmitGlobalTime = DISABLE
        };
        
        uint32_t tx_mailbox;
        HAL_CAN_AddTxMessage(&hcan1, &tx_header, can_data, &tx_mailbox);
    }
}
/* USER CODE END 0 */
```

### Python Environment Requirements

**Conda Environment (Recommended)**:
```bash
# Create dedicated environment
conda create -n feb_can python=3.8
conda activate feb_can
pip install cantools

# Scripts automatically detect and use feb_can environment
```

**System Python (Alternative)**:
```bash
# Install cantools globally
pip install cantools

# Scripts automatically fall back to system Python
```

### CAN Library Management Rules for AI Agents

**✅ ALLOWED**:
- Edit Python message definition files (*_messages.py)
- Run generation scripts (generate_can.sh/bat)
- Update submodules (submodule.sh/bat)
- Include generated headers in user code

**❌ FORBIDDEN**:
- Edit generated C/H files in gen/ directory
- Modify generate.py script without understanding
- Manually create DBC files
- Bypass the Python→C generation workflow

## Development Tools

### Automated Scripts

The project provides comprehensive automation scripts for common development tasks:

#### Individual Task Scripts
```bash
# Clean all build directories
./scripts/clean.sh               # clean.bat on Windows

# Build STM32 firmware only
./scripts/build.sh               # build.bat on Windows

# Flash pre-built firmware to device
./scripts/flash.sh               # flash.bat on Windows

# Build and run unit tests
./scripts/test.sh                # test.bat on Windows
```

#### Submodule and CAN Library Management
```bash
# Initialize submodules (after first clone)
./scripts/submodule.sh --init    # submodule.bat --init on Windows

# Update submodules and regenerate CAN library
./scripts/submodule.sh --update  # submodule.bat --update on Windows

# Check submodule status
./scripts/submodule.sh --status  # submodule.bat --status on Windows

# Generate CAN library C/H files from Python definitions
./scripts/generate_can.sh        # generate_can.bat on Windows
```

#### Development Workflow Automation
```bash
# Complete development cycle (includes submodule update)
./scripts/dev.sh --all           # dev.bat --all on Windows

# Development mode (no hardware flashing)
./scripts/dev.sh --dev           # dev.bat --dev on Windows

# Build and flash (common hardware workflow)
./scripts/dev.sh -bf             # dev.bat -bf on Windows

# Build and test (code validation workflow)
./scripts/dev.sh -bt             # dev.bat -bt on Windows

# Clean, build, then test
./scripts/dev.sh -cbt            # dev.bat -cbt on Windows

# Show all available options
./scripts/dev.sh --help          # dev.bat --help on Windows

# Preview commands without execution
./scripts/dev.sh --dry-run --all # dev.bat --dry-run --all on Windows
```

#### Cross-Platform Script Features
**Full Equivalency**: All `.sh`/`.bat` script pairs provide identical functionality:
- ✅ Same command-line arguments (-c, --clean, -b, --build, etc.)
- ✅ Identical error handling and exit codes
- ✅ Consistent help documentation and output formatting
- ✅ File size reporting in build and flash operations
- ✅ Enhanced error messages with troubleshooting advice

**CAN Library Integration**:
- 🔄 Automatic C/H file generation from Python CAN message definitions
- 🐍 Conda environment detection with system Python fallback
- 📦 Dependency checking for `cantools` package with installation guidance
- 🔧 Generated files automatically included in CMake build system

#### VSCode Integration
All scripts are integrated into VSCode tasks:
- `Ctrl+Shift+P` → "Tasks: Run Task"
- Select from: Clean, Build, Flash, Test, Development Workflow

## Testing Framework

### Test Architecture
The project includes **26 unit tests** covering:
- Printf redirection functionality (23 tests)
- Integration scenarios (3 tests)
- STM32 HAL mocking system

### Test Development Rules
1. **Create New Tests**: Add to `test/unit/test_module.cpp`
2. **Update Build**: Modify `test/CMakeLists.txt`
3. **Use Mocks**: Leverage existing STM32 HAL mocks
4. **Verify Coverage**: Run full test suite

### Test File Template
```cpp
#include <gtest/gtest.h>

extern "C" {
    #include "your_module.h"
    #include "stm32f4xx_hal.h"
}

class YourModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mock_ResetAll();
    }
    
    void TearDown() override {
        Mock_ResetAll();
    }
};

TEST_F(YourModuleTest, FunctionName_Scenario) {
    // Arrange
    Mock_SetHALStatus(HAL_OK);
    
    // Act
    int result = function_under_test();
    
    // Assert
    EXPECT_EQ(EXPECTED_VALUE, result);
}
```

### Mock System Usage
```cpp
// Configure mock return values
Mock_SetADCValue(2048);
Mock_SetHALStatus(HAL_ERROR);
Mock_UART_ClearBuffer();

// Execute code under test
your_function_that_uses_hal();

// Verify mock interactions
uint8_t* buffer = Mock_UART_GetBuffer();
uint32_t size = Mock_UART_GetBufferSize();
EXPECT_EQ(expected_size, size);
```

## Build System

### Test Commands
```bash
# Run all tests (26 tests)
./run_tests.sh

# VSCode-optimized runner
./run_vscode_tests.sh           # Unix/macOS
run_vscode_tests.bat            # Windows

# Run specific test
./run_vscode_tests.sh --gtest_filter="PrintfRedirectTest.*"
```

### Firmware Build (if toolchain available)
```bash
# Configure for STM32
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake

# Build
make

# Output: BMS.elf, BMS.hex, BMS.bin
```

### CMake Configuration Updates
When adding new user modules:

```cmake
# test/CMakeLists.txt
set(TEST_SOURCES
    unit/test_printf_redirect.cpp
    unit/test_new_module.cpp          # Add your test
    ../Core/User/Src/printf_redirect.c
    ../Core/User/Src/new_module.c     # Add your source
    mocks/stm32f4xx_hal.c
)
```

## Development Patterns

### Error Handling
```c
/* USER CODE BEGIN 0 */
typedef enum {
    APP_OK = 0,
    APP_ERROR_UART,
    APP_ERROR_SENSOR,
    APP_ERROR_INVALID_PARAM
} AppError_t;

AppError_t App_SendData(const char* data) {
    if (data == NULL) return APP_ERROR_INVALID_PARAM;
    
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2, 
        (uint8_t*)data, strlen(data), 1000);
    
    if (status != HAL_OK) {
        printf("UART Error: %d\n", status);
        return APP_ERROR_UART;
    }
    
    return APP_OK;
}
/* USER CODE END 0 */
```

### Logging and Debugging
```c
/* USER CODE BEGIN Includes */
#include "printf_redirect.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
#define LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

void App_LogSystemStatus(void) {
    LOG_INFO("System status: OK");
    LOG_DEBUG("Free heap: %d bytes", get_free_heap());
}
/* USER CODE END 0 */
```

### State Machine Pattern
```c
/* USER CODE BEGIN PV */
typedef enum {
    STATE_INIT,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_SHUTDOWN
} SystemState_t;

static SystemState_t current_state = STATE_INIT;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
void App_StateMachine(void) {
    switch (current_state) {
        case STATE_INIT:
            if (App_Initialize() == APP_OK) {
                current_state = STATE_RUNNING;
                LOG_INFO("System initialized");
            } else {
                current_state = STATE_ERROR;
            }
            break;
            
        case STATE_RUNNING:
            App_ProcessData();
            if (error_detected) {
                current_state = STATE_ERROR;
            }
            break;
            
        case STATE_ERROR:
            App_HandleError();
            break;
            
        case STATE_SHUTDOWN:
            App_Cleanup();
            break;
    }
}
/* USER CODE END 0 */
```

## AI Agent Responsibilities

### Code Quality Assurance
1. **Always Test**: Write unit tests for new functionality
2. **Check Returns**: Verify HAL function return values
3. **Handle Errors**: Implement proper error handling
4. **Document Code**: Add comments for complex logic
5. **Follow Patterns**: Use established coding conventions

### Project Integrity
1. **Respect Boundaries**: Stay within user code areas
2. **Use Tools Properly**: STM32CubeMX for hardware config
3. **Preserve Tests**: Maintain existing test coverage
4. **Clean Builds**: Verify builds work after changes
5. **Update Docs**: Keep documentation current

### Communication with Users
1. **Explain Changes**: Describe what you're modifying
2. **Request Regeneration**: When .ioc files are modified
3. **Report Issues**: Alert to potential conflicts
4. **Seek Clarification**: Ask when requirements are unclear
5. **Provide Context**: Explain why changes are needed

## Common Mistakes to Avoid

### ❌ Don't Do This
```c
// DON'T: Edit generated initialization code
void MX_USART2_UART_Init(void) {
    // This function is auto-generated - don't modify!
    // Put your code in USER CODE sections instead
}

// DON'T: Manually configure peripherals
RCC->APB1ENR |= RCC_APB1ENR_USART2EN;  // Use STM32CubeMX instead

// DON'T: Ignore return values
HAL_UART_Transmit(&huart2, data, len, 1000);  // Unchecked!
```

### ✅ Do This Instead
```c
/* USER CODE BEGIN 2 */
// DO: Use STM32CubeMX generated initialization
// Then add your application code
App_Initialize();
/* USER CODE END 2 */

/* USER CODE BEGIN 0 */
// DO: Check return values
AppError_t App_SendMessage(const char* msg) {
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2, 
        (uint8_t*)msg, strlen(msg), 1000);
    
    return (status == HAL_OK) ? APP_OK : APP_ERROR_UART;
}
/* USER CODE END 0 */
```

## Debugging and Troubleshooting

### When Things Go Wrong

**Build Errors:**
1. Check user code placement
2. Verify header includes
3. Clean and rebuild: `rm -rf build test/build`

**Test Failures:**
1. Run specific test: `--gtest_filter="TestName.*"`
2. Debug with breakpoints
3. Check mock configuration

**Generated Code Issues:**
1. Verify .ioc file is correct
2. Regenerate with STM32CubeMX
3. Check user code preservation

### Debug Tools Available
- **VSCode Debugging**: Full breakpoint support
- **Printf Debugging**: Output goes to UART
- **Unit Test Framework**: Isolated component testing
- **Mock System**: Controllable hardware simulation

## Platform Support

### Tested Environments
- ✅ **Linux**: Ubuntu 20.04+, other distributions
- ✅ **macOS**: 10.15+ (Intel & Apple Silicon)
- ✅ **Windows**: Windows 10+, MSYS2, MinGW, Visual Studio

### Cross-Platform Testing
- All test scripts work on supported platforms
- VSCode configuration is platform-independent
- CMake handles toolchain differences

## Documentation Standards

### Code Comments
```c
/**
 * @brief Initialize sensor subsystem
 * @param config Sensor configuration structure
 * @return APP_OK on success, error code otherwise
 */
AppError_t Sensors_Initialize(const SensorConfig_t* config);
```

### File Headers
```c
/**
 * @file    custom_module.c
 * @brief   Custom module implementation
 * @author  Your Name
 * @date    YYYY-MM-DD
 */
```

## Summary Checklist for AI Agents

Before making any changes:
- [ ] Identify if changes affect user code areas
- [ ] Plan test coverage for new functionality
- [ ] Check if hardware configuration changes are needed
- [ ] Verify existing tests will still pass
- [ ] Prepare clear explanation for user

After making changes:
- [ ] Run full test suite (`./run_tests.sh`)
- [ ] Verify build works (`cmake --build build`)
- [ ] Update relevant documentation
- [ ] Test on target platform if possible
- [ ] Commit changes with clear messages

---

**Remember**: When in doubt, ask the user for guidance. It's better to clarify requirements than to make incorrect assumptions about auto-generated code or hardware configuration.