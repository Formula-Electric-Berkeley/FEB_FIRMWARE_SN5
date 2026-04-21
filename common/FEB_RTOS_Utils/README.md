# FEB RTOS Utils

Header-only helper library for FreeRTOS-based FEB boards. Exposes a single fail-fast macro that catches RTOS handle allocation failures at boot.

## CMake Target

Header-only INTERFACE library. Add to your board's `CMakeLists.txt`:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_rtos_utils)
```

## Public API

| Macro | Description |
|---|---|
| `REQUIRE_RTOS_HANDLE(handle)` | If `handle == NULL`, calls `Error_Handler()` to halt. Otherwise no-op. |

See [`Inc/feb_rtos_utils.h`](Inc/feb_rtos_utils.h) for the full declaration.

## Dependencies

- FreeRTOS (caller uses `osMutexNew`, `osSemaphoreNew`, `osMessageQueueNew`, `osThreadNew`, etc.)
- An `Error_Handler(void)` symbol provided by the board — CubeMX generates one in `main.c` by default.

No other `feb_*` libraries are required.

## Configuration

None.

## Usage

Call the macro immediately after creating each RTOS object. If the kernel failed to allocate the object (heap exhaustion, misconfigured `configTOTAL_HEAP_SIZE`, etc.), the system halts at boot instead of crashing later with a NULL dereference.

```c
#include "feb_rtos_utils.h"
#include "cmsis_os2.h"

osMutexId_t canTxMutexHandle;
osMessageQueueId_t canRxQueueHandle;
osThreadId_t uartRxTaskHandle;

void Init_RTOS_Objects(void)
{
    canTxMutexHandle = osMutexNew(&canTxMutex_attributes);
    REQUIRE_RTOS_HANDLE(canTxMutexHandle);

    canRxQueueHandle = osMessageQueueNew(32, sizeof(FEB_CAN_Message_t), NULL);
    REQUIRE_RTOS_HANDLE(canRxQueueHandle);

    uartRxTaskHandle = osThreadNew(StartUartRxTask, NULL, &uartRxTask_attributes);
    REQUIRE_RTOS_HANDLE(uartRxTaskHandle);
}
```

## Boards Using This Library

- [BMS](../../BMS/README.md)
- [DASH](../../DASH/README.md)
- [PCU](../../PCU/README.md) *(via feb_io transitively; linked explicitly where FreeRTOS is in use)*
- [UART](../../UART/README.md)
- [UART_TEST](../../UART_TEST/README.md)

## See Also

- [`common/README.md`](../README.md) — library index
- [`FEB_Serial_Library/`](../FEB_Serial_Library/README.md) — UART / Log / Console that boards wire up alongside their RTOS objects
