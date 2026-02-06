# FEB CAN Library

Runtime CAN communication library with FreeRTOS-safe TX/RX queues, registration-based message handling, and automatic periodic transmission.

## Features

- **Multi-instance support** - CAN1 and CAN2 peripherals
- **TX registration** - Register messages with optional periodic auto-transmit
- **RX callbacks** - Register handlers for specific CAN IDs with flexible filtering
- **Filter types** - Exact match, mask-based, or wildcard (accept all)
- **FreeRTOS-safe** - Queue-based TX/RX for multi-task environments
- **Pack/unpack integration** - Works with generated `FEB_CAN_Library_SN4` message structs

## Integration

### 1. CMake

Add to your board's `CMakeLists.txt`:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_can)
```

### 2. CubeMX Configuration

Configure your CAN peripheral:

1. **CAN**: Enable CAN1 (and CAN2 if needed)
   - Prescaler, Time Quanta, etc. for your bus speed (typically 500kbps or 1Mbps)
2. **NVIC**: Enable interrupts:
   - `CAN1 RX0 interrupt`
   - `CAN1 RX1 interrupt`
   - `CAN1 TX interrupt` (optional, for TX complete tracking)
   - `CAN1 SCE interrupt` (optional, for error handling)

### 3. HAL Callback Wiring

In `stm32f4xx_it.c` or a callbacks file, route HAL callbacks to the library:

```c
#include "feb_can_lib.h"

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_RxFifo0Callback(hcan);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_RxFifo1Callback(hcan);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_TxMailbox0CompleteCallback(hcan);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_TxMailbox1CompleteCallback(hcan);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_TxMailbox2CompleteCallback(hcan);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    FEB_CAN_ErrorCallback(hcan);
}
```

### 4. Initialization

```c
#include "feb_can_lib.h"

void FEB_Main_Setup(void)
{
    FEB_CAN_Config_t cfg = {
        .hcan1 = &hcan1,
        .hcan2 = NULL,  // or &hcan2 if using CAN2
        .get_tick_ms = HAL_GetTick,
    };

    if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
    {
        // Handle init failure
    }
}
```

### 5. Main Loop Processing

```c
void FEB_Main_Loop(void)
{
    FEB_CAN_TX_ProcessPeriodic();  // Send due periodic messages
    FEB_CAN_RX_Process();          // Dispatch RX callbacks (FreeRTOS mode)
}
```

Or with FreeRTOS, use dedicated tasks:

```c
void StartCanTxTask(void *arg)
{
    for (;;)
    {
        FEB_CAN_TX_Process();          // Process TX queue
        FEB_CAN_TX_ProcessPeriodic();  // Send periodic messages
        osDelay(1);
    }
}

void StartCanRxTask(void *arg)
{
    for (;;)
    {
        FEB_CAN_RX_Process();  // Dispatch RX callbacks from queue
        osDelay(1);
    }
}
```

## TX API

### Simple Send

Send a message immediately:

```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, 0x100, FEB_CAN_ID_STD, data, sizeof(data));
```

### Registered TX Slots

Register a message for manual or periodic transmission:

```c
#include "FEB_CAN_Library_SN4/gen/feb_can.h"  // For message structs

// Data struct (must have static lifetime for periodic TX)
static struct feb_can_lvpdb_heartbeat_t heartbeat_data = {0};

// Register TX slot with periodic transmission
FEB_CAN_TX_Params_t tx_params = {
    .instance = FEB_CAN_INSTANCE_1,
    .can_id = FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID,
    .id_type = FEB_CAN_ID_STD,
    .data_ptr = &heartbeat_data,
    .data_size = sizeof(heartbeat_data),
    .period_ms = 100,  // Auto-transmit every 100ms (0 = manual only)
    .pack_func = (int (*)(uint8_t *, const void *, size_t))feb_can_lvpdb_heartbeat_pack,
};

int32_t hb_handle = FEB_CAN_TX_Register(&tx_params);
if (hb_handle < 0)
{
    // Handle registration failure
}

// Update data and it will be sent automatically every 100ms
heartbeat_data.error0 = some_error_condition;
heartbeat_data.error1 = another_error;

// Or trigger manual send
FEB_CAN_TX_SendSlot(hb_handle);

// Change period at runtime
FEB_CAN_TX_SetPeriod(hb_handle, 50);  // Now every 50ms
```

### ISR-Safe TX

For sending from interrupt context:

```c
FEB_CAN_TX_SendFromISR(FEB_CAN_INSTANCE_1, 0x100, FEB_CAN_ID_STD, data, len);
```

## RX API

### Register RX Callback

```c
#include "FEB_CAN_Library_SN4/gen/feb_can.h"

// Callback function
void on_bms_state(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                  const uint8_t *data, uint8_t length, void *user_data)
{
    struct feb_can_bms_state_t msg;
    feb_can_bms_state_unpack(&msg, data, length);

    // Use msg.bms_state, msg.relay_state, etc.
    LOG_D("CAN", "BMS state: %d, relay: %d", msg.bms_state, msg.relay_state);
}

// Register with exact ID match
FEB_CAN_RX_Params_t rx_params = {
    .instance = FEB_CAN_INSTANCE_1,
    .can_id = FEB_CAN_BMS_STATE_FRAME_ID,
    .id_type = FEB_CAN_ID_STD,
    .filter_type = FEB_CAN_FILTER_EXACT,
    .fifo = FEB_CAN_FIFO_0,
    .callback = on_bms_state,
    .user_data = NULL,  // Optional context passed to callback
};

int32_t rx_handle = FEB_CAN_RX_Register(&rx_params);
```

### Filter Types

| Type | Description | Example Use |
|------|-------------|-------------|
| `FEB_CAN_FILTER_EXACT` | Match exact CAN ID | Single message type |
| `FEB_CAN_FILTER_MASK` | Match ID range with mask | Group of related messages |
| `FEB_CAN_FILTER_WILDCARD` | Accept all messages | Debugging, logging |

#### Mask Filter Example

```c
// Match IDs 0x100-0x10F (mask ignores lower 4 bits)
FEB_CAN_RX_Params_t rx_params = {
    .instance = FEB_CAN_INSTANCE_1,
    .can_id = 0x100,
    .mask = 0x7F0,  // Match upper bits, ignore lower 4
    .id_type = FEB_CAN_ID_STD,
    .filter_type = FEB_CAN_FILTER_MASK,
    .callback = on_message_group,
};
FEB_CAN_RX_Register(&rx_params);
```

#### Wildcard Example

```c
// Receive all messages (for logging/debugging)
FEB_CAN_RX_Params_t rx_params = {
    .instance = FEB_CAN_INSTANCE_1,
    .filter_type = FEB_CAN_FILTER_WILDCARD,
    .fifo = FEB_CAN_FIFO_1,  // Use separate FIFO to avoid blocking priority messages
    .callback = on_any_message,
};
FEB_CAN_RX_Register(&rx_params);
```

### Extended Callback (with metadata)

For callbacks that need timestamp and error info:

```c
void on_message_extended(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                          const uint8_t *data, uint8_t length, uint32_t timestamp,
                          uint32_t error_flags, void *user_data)
{
    LOG_D("CAN", "ID 0x%03X at %lu ms", can_id, timestamp);
}

FEB_CAN_RX_RegisterExtended(&rx_params, on_message_extended);
```

## Filter Configuration

### Automatic Filter Update

After registering RX callbacks, update hardware filters:

```c
FEB_CAN_RX_Register(&rx1);
FEB_CAN_RX_Register(&rx2);
FEB_CAN_RX_Register(&rx3);

// Configure hardware filters based on registrations
FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_INSTANCE_1);
```

### Manual Filter Configuration

```c
// Configure filter bank 0 to accept ID 0x100 on FIFO0
FEB_CAN_Filter_Configure(FEB_CAN_INSTANCE_1, 0, 0x100, 0x7FF, FEB_CAN_ID_STD, FEB_CAN_FIFO_0);

// Accept all messages on filter bank 1
FEB_CAN_Filter_AcceptAll(FEB_CAN_INSTANCE_1, 1, FEB_CAN_FIFO_1);
```

## Diagnostics

```c
// Get registration counts
uint32_t tx_count = FEB_CAN_TX_GetRegisteredCount();
uint32_t rx_count = FEB_CAN_RX_GetRegisteredCount();

// Check queue status (FreeRTOS mode)
uint32_t tx_pending = FEB_CAN_TX_GetQueuePending();
uint32_t rx_pending = FEB_CAN_RX_GetQueuePending();

// Check TX readiness
if (FEB_CAN_TX_IsReady(FEB_CAN_INSTANCE_1))
{
    // At least one mailbox is free
}
uint32_t free_mailboxes = FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_INSTANCE_1);

// Error counters
uint32_t rx_overflow = FEB_CAN_GetRxQueueOverflowCount();
uint32_t tx_overflow = FEB_CAN_GetTxQueueOverflowCount();
uint32_t tx_timeouts = FEB_CAN_GetTxTimeoutCount();
uint32_t hal_errors = FEB_CAN_GetHalErrorCount();

// Reset error counters
FEB_CAN_ResetErrorCounters();
```

## Configuration Options

Override in your board's config before including headers:

| Define | Default | Description |
|--------|---------|-------------|
| `FEB_CAN_MAX_RX_HANDLES` | 32 | Maximum RX callback registrations |
| `FEB_CAN_MAX_TX_HANDLES` | 16 | Maximum TX slot registrations |
| `FEB_CAN_TX_QUEUE_SIZE` | 16 | TX queue depth (FreeRTOS) |
| `FEB_CAN_RX_QUEUE_SIZE` | 32 | RX queue depth (FreeRTOS) |
| `FEB_CAN_TX_TIMEOUT_MS` | 100 | TX mailbox timeout |
| `FEB_CAN_USE_FREERTOS` | auto | Force FreeRTOS mode on/off |
| `FEB_CAN_ENABLE_PERIODIC_TX` | 1 | Enable periodic TX feature |
| `FEB_CAN_MAX_PERIODIC_TX_SLOTS` | 8 | Maximum periodic TX slots |

## Using with Pack/Unpack Library

This library is designed to work with the generated pack/unpack functions from `FEB_CAN_Library_SN4`:

```c
#include "feb_can_lib.h"
#include "FEB_CAN_Library_SN4/gen/feb_can.h"

// TX: Pack struct and send
struct feb_can_lvpdb_flags_bus_voltage_lv_current_t tx_msg = {
    .flags = 0x00000001,
    .bus_voltage = 1234,
    .lv_current = 567,
};
uint8_t buffer[FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_LENGTH];
feb_can_lvpdb_flags_bus_voltage_lv_current_pack(buffer, &tx_msg, sizeof(buffer));
FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_FRAME_ID,
                FEB_CAN_ID_STD, buffer, sizeof(buffer));

// RX: Unpack received data
void on_rx(FEB_CAN_Instance_t inst, uint32_t id, FEB_CAN_ID_Type_t type,
           const uint8_t *data, uint8_t len, void *user)
{
    struct feb_can_bms_accumulator_voltage_t msg;
    feb_can_bms_accumulator_voltage_unpack(&msg, data, len);
    float voltage = msg.total_pack_voltage * 0.01f;  // Apply scaling if needed
}
```

## Thread-Safety

| Function | Thread-Safe | ISR-Safe | Notes |
|----------|-------------|----------|-------|
| `FEB_CAN_TX_Send()` | Yes | No | Uses queue in FreeRTOS mode |
| `FEB_CAN_TX_SendFromISR()` | N/A | Yes | For interrupt context |
| `FEB_CAN_TX_SendSlot()` | Yes | No | Triggers registered slot |
| `FEB_CAN_TX_Register()` | No | No | Call during init only |
| `FEB_CAN_RX_Register()` | No | No | Call during init only |
| `FEB_CAN_TX_Process()` | No | No | Single-task only |
| `FEB_CAN_RX_Process()` | No | No | Single-task only |
| `FEB_CAN_TX_ProcessPeriodic()` | No | No | Single-task only |

## Troubleshooting

### No messages received
1. Verify CAN bus termination (120 ohm resistors at both ends)
2. Check CAN transceiver wiring and power
3. Verify baud rate matches other devices on bus
4. Ensure RX interrupts are enabled in CubeMX
5. Check that `FEB_CAN_RX_Process()` is being called
6. Verify filters are configured (`FEB_CAN_Filter_UpdateFromRegistry()`)

### TX fails or times out
1. Check that at least one other device is on the bus (CAN requires ACK)
2. Verify TX interrupt is enabled
3. Check `FEB_CAN_GetHalErrorCount()` for HAL errors
4. Ensure `FEB_CAN_TX_Process()` is being called (FreeRTOS mode)

### Messages lost or callbacks not firing
1. Increase `FEB_CAN_RX_QUEUE_SIZE` if queue overflows
2. Check `FEB_CAN_GetRxQueueOverflowCount()`
3. Ensure callback is registered for correct CAN ID
4. Verify filter type matches your needs (exact vs mask vs wildcard)

## Files

| File | Purpose |
|------|---------|
| `feb_can_lib.h` | Public API |
| `feb_can_config.h` | Configuration defaults |
| `feb_can_internal.h` | Internal structures |
| `feb_can.c` | Core initialization and utilities |
| `feb_can_tx.c` | TX implementation |
| `feb_can_rx.c` | RX implementation |
| `feb_can_filter.c` | Filter configuration |
| `CMakeLists.txt` | CMake integration |
