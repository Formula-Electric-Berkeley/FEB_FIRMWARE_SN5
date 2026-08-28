# feb::can

The publish/subscribe layer for FEB boards. A message's frame id, length,
cadence and signal layout all come from the DBC. A board only says how to fill
its signals and where to read them.

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_can)
```

This sits on top of the C driver documented in [README.md](README.md). That
driver is still what moves frames, and nothing here replaces it.

## Quick start

Transmitting. The whole of `DASH_CAN_Publish.cpp`:

```cpp
#include "DASH_CAN.h"
#include "DASH_IO.h"
#include "DASH_RTD.h"
#include "feb_can_publisher.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

namespace
{
bool fill_dash_state(feb_can_dash_state_t &m)
{
  const IO_States_t io = FEB_IO_GetLastIOStates();
  m.button1 = io.button_rtd;
  m.buzzer = io.buzzer_enabled;
  m.ready_to_drive = FEB_State_GetLastRTD();
  return true;
}

fc::Publisher<fm::DashState> state_tx{fill_dash_state};
} // namespace
```

That is a complete 100 ms publisher. There is no init call, no registration, no
frame id and no pack call, because `DashState` already carries all of it.

Receiving, at the point the value is used:

```cpp
low_voltage = fc::rx<fm::LvpdbLv24vBusAnd12vBusVoltages>.v().lv_24v_voltage;
```

## The file of a board

Five files at `Core/User/Src`, board-specific work under `Core/User/Src/App`:

| File | Holds |
|---|---|
| `<BOARD>_Main.cpp` | initialisation |
| `<BOARD>_Tasks.cpp` | every FreeRTOS task body |
| `<BOARD>_CAN.cpp` | bring-up and the ready flag |
| `<BOARD>_CAN_Publish.cpp` | fill functions and publishers |
| `<BOARD>_Commands.cpp` | console commands |

Plus one contract at `Core/User/Inc/Config/feb_board_config.hpp`:

```cpp
namespace feb::can
{
inline constexpr Node kThisNode = Node::kDash;
inline constexpr FEB_CAN_Instance_t kVehicleBus = FEB_CAN_INSTANCE_2;
inline constexpr std::size_t kMaxPayload = 8;
} // namespace feb::can
```

`kVehicleBus` is the peripheral this board reaches the bus through, not a
separate bus. DASH uses CAN2, every other board uses CAN1. The DBC describes one
logical network and carries no bus field.

## Publishing

```cpp
fc::Publisher<fm::DashState> tx{fill};                 // period from the DBC
fc::Publisher<fm::DashState> tx{fill, 100};            // override the period
fc::Publisher<fm::DashState> tx{fill, 100, 75};        // and pin the offset
fc::Publisher<fm::ChargerCmd> tx{fill};                // no cycle time: one-shot only
```

The constructor registers it. `start()` only re-arms after `stop()`.

A fill returns `false` to skip a cycle: no frame, no error counted. That is how
you gate on a sensor that is not ready yet.

```cpp
bool fill_accel(feb_sn_imu_accel_t &m)
{
  if (!imu_sample_valid()) { return false; }
  m.acceleration_x = feb_sn_imu_accel_x_encode(acceleration_mg[0]);
  return true;
}
```

A message with no cycle time in the DBC is never scheduled. `start()` on one is
a compile error. Call `publish()` instead.

Two static assertions fire at compile time: the board must be the message's
declared sender, and the message must fit `kMaxPayload`.

## Subscribing

`fc::rx<M>` is one object per message for the whole program, however many files
name it. There is no registration to coordinate and no way to end up with two
subscribers on one frame id.

```cpp
fc::rx<fm::BmsState>.v()          // latest decoded payload
fc::rx<fm::BmsState>.fresh()      // arrived within 3 x the DBC cycle time
fc::rx<fm::BmsState>.snapshot()   // coherent copy; use when reading 2+ fields
fc::rx<fm::BmsState>.registered() // false if RX registration failed
```

`fresh()` takes no timeout. The window is `kCycleMs * 3`, derived from the DBC,
so no call site invents a constant.

`v()` returns a reference to live storage the RX task writes. Reading one field
is safe. Reading two can tear, so use `snapshot()` for that:

```cpp
const auto wss = fc::rx<fm::WssRearData>.snapshot();
speed = (wss.wss_left_rear + wss.wss_right_rear) / 2;
```

To act on arrival rather than poll:

```cpp
fc::rx<fm::ResState>.on_receive([](const feb_can_res_state_t &m) {
  if (m.res_estop) { FEB_State_ForceRTDOff(); }
});
```

That handler runs in the RX task with the RX mutex held. Keep it short and do
not call back into the CAN RX path. Set a flag or post to a queue instead.

## The three tasks

```cpp
extern "C"
{
  void StartCanRxTask(void *)  { fc::RunRxTask(&DASH_CAN_Init); }
  void StartCanPubTask(void *) { fc::RunPubTask(&DASH_CAN_IsReady); }
  void StartCanTxTask(void *)  { fc::RunTxTask(); }
}
```

`RunRxTask` runs your bring-up, programs the hardware filters from whatever it
registered, then dispatches. `RunPubTask` ticks the scheduler, gated on your
readiness predicate. `RunTxTask` drains the queue into the mailboxes.

They must be three separate tasks. `FEB_CAN_TX_Process` blocks up to
`FEB_CAN_TX_TIMEOUT_MS` on the mailbox semaphore, so publishers sharing that
task stall behind one bad frame.

Every one of these overrides a `__weak` stub CubeMX emits, so each needs C
linkage. Without it the definition mangles, the linker keeps CubeMX's empty
stub, and the task silently does nothing. Check with:

```
arm-none-eabi-nm BOARD.elf | grep -E " Start[A-Za-z]+$"
```

## What the scheduler does

Publishers sharing a period are spread evenly across it, so three 100 ms
messages land at 0, 33 and 66 ms rather than together. Entries are ordered by
`(period, frame_id)`, both compile-time constants, so the schedule depends only
on which publishers exist -- never on construction or `start()` order.

Pass an explicit offset to opt out. It is honoured verbatim and excluded from
the automatic split.

After a stall the scheduler drops missed cycles instead of queuing a burst,
which bounds what can arrive at the mailboxes at once.

## The limits

| Limit | Value | Result if exceeded |
|---|---|---|
| Filter banks per instance | 14 | Excess subscribed ids are never received. Logged as `Filter overflow`. |
| `FEB_CAN_MAX_RX_HANDLES` | 32 | `FEB_CAN_RX_Register` fails and `registered()` returns false. |
| `canTxQueue` depth (`.ioc`) | 16 | Frames dropped, counted by `FEB_CAN_GetTxQueueOverflowCount()`. |
| `canRxQueue` depth (`.ioc`) | 32 | Frames dropped, counted by `FEB_CAN_GetRxQueueOverflowCount()`. |
| `kMaxPayload` | 8 | Compile error on the offending `Publisher` or `Subscriber`. |
| `FEB_CAN_TX_TIMEOUT_MS` | 100 | The frame is dropped and the TX timeout count increments. |

The filter budget is the one that bites first. DASH subscribes to 7 ids of 14.

Under FreeRTOS the queue depths come from the `.ioc`, not from
`feb_can_config.h`. `FEB_CAN_TX_QUEUE_SIZE` sizes only the bare-metal ring
buffer and `FEB_CAN_RX_QUEUE_SIZE` is unreferenced, so editing either changes
nothing on a board that uses the RTOS.

## Regenerating

`feb_can_traits.hpp` and `feb_can_db.{h,c}` are generated. After editing a
message definition:

```
cd common/FEB_CAN_Library_SN4 && ./generate_can.sh
```

`--check` verifies the committed output is current, and `--validate` runs the
registry checks including the periodic bus-load budget.
