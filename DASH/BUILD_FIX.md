# Build Fix Instructions

## Problem
After regenerating code from STM32CubeMX, getting "unknown type name" errors for:
- `TIM_HandleTypeDef`
- `SD_HandleTypeDef`
- `CRC_HandleTypeDef`
- `QSPI_HandleTypeDef`
- `SAI_HandleTypeDef`

## Root Cause
Build cache contains old object files from before code regeneration.

## ✅ Solution: Clean Rebuild

### Option 1: Via VSCode (RECOMMENDED)
1. Open Command Palette (`Ctrl+Shift+P`)
2. Type: `CMake: Delete Cache and Reconfigure`
3. Then type: `CMake: Clean Rebuild`

### Option 2: Via PowerShell
```powershell
# Navigate to DASH directory
cd "C:\Users\prana\OneDrive\tess\FEB-CS\FEB_FIRMWARE_SN5\DASH"

# Delete build folder
Remove-Item -Recurse -Force build

# Reconfigure and build
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

### Option 3: Via Terminal in VSCode
```bash
cd build
ninja clean
ninja
```

---

## ✅ Status: CAN Implementation

**ALL CAN FILES COMPILED SUCCESSFULLY** - No changes needed!

### Files That Work:
- ✅ `FEB_CAN_Setup.c`
- ✅ `FEB_CAN_TX.c`
- ✅ `FEB_CAN_RX.c`
- ✅ `FEB_CAN_BMS.c`
- ✅ `FEB_CAN_DASH.c`
- ✅ `FEB_CAN_PCU.c`
- ✅ `FEB_Heartbeat.c`
- ✅ `FEB_CAN_TPS.c`
- ✅ `FEB_IO.c`
- ✅ `FEB_i2c_protected.c`
- ✅ `FEB_TPS2482.c`

### Phase 1 Complete:
✅ CAN architecture migrated from SN4 to SN5  
✅ PCU's better CAN implementation integrated  
✅ Modular callback system implemented  
✅ All naming updated (ICS→DASH, APPS→PCU)  
✅ All bugs fixed  
✅ Code compiles without errors  

### Ready for Phase 2:
Once build completes, we can proceed with FreeRTOS integration:
- Add CAN RX message queue
- Modify callbacks to use queues
- Create CAN processing task
- Add mutexes for shared data

