# BMS ECU Simulator

Runs the **real, unmodified** BMS state machine (`Core/User/Src/FEB_SM.cpp`) on
your laptop against a simulated accumulator and car. No board, no ARM
toolchain, no HV. A full drive or charge session runs in milliseconds.

```bash
./scripts/bms-sim.sh                                   # build + run every scenario
./scripts/bms-sim.sh list                              # what scenarios exist
./scripts/bms-sim.sh run precharge_too_fast            # one scenario, full SM log
./scripts/bms-sim.sh run nominal_drive_cycle --csv drive.csv -v   # + CSV trace, debug logs
```

CI runs every scenario on each PR that touches `BMS/` (`.github/workflows/bms-sim.yml`).

## How it works

```
 ┌───────────────────────────── bms_sim (host process) ─────────────────────────────┐
 │                                                                                  │
 │  FEB_SM.cpp  (firmware, compiled as-is)                                          │
 │      │ calls FEB_HW_*, FEB_CAN_*, FEB_ADBMS_*, HAL_GetTick, FEB_Time_Us          │
 │      ▼                                                                           │
 │  src/bindings.cpp  ── host implementations of those C functions                  │
 │      │                                                                           │
 │      ▼                                                                           │
 │  src/plant.cpp     ── cells (OCV + IR + thermal), AIR-/AIR+/precharge contactors,│
 │                       shutdown loop, precharge RC, IVT, IMD latch, DASH, charger,│
 │                       ADBMS task stand-in                                        │
 │                                                                                  │
 │  src/sim.cpp       ── simulated clock + task schedule (SM 1 ms, ADBMS 10 ms,     │
 │                       charger process 100 ms), logger, CSV trace                 │
 │  scenarios/        ── scripted sessions with expectations                        │
 └──────────────────────────────────────────────────────────────────────────────────┘
```

* `shim/` shadows `stm32f4xx_hal.h`, `main.h` and `feb_log.h` so firmware
  headers compile on the host. Firmware `LOG_*` lines come out stamped with
  simulated time.
* This is a **link seam**: the sim supplies its own definitions of the C
  functions the firmware calls. Nothing in `Core/` changes, and the firmware
  build doesn't know the sim exists.
* `FEB_Time_Us()` advances simulated time by 1 µs per call. That way the SM's
  busy-wait majority vote on the shutdown line really samples a time-varying
  input, so you can inject a 200 µs glitch and watch it get rejected.
* Each scenario runs in its own forked process, because the firmware keeps its
  state in file-scope statics.

## Writing a scenario

Add to `scenarios/scenarios.cpp`:

```cpp
SIM_SCENARIO(my_scenario, "One line: setup -> expected outcome")
{
  power_up_car(s);                         // boot, IMD reset, DASH/PCU on CAN
  if (!to_drive(s, c)) return;             // TSMS on, precharge, R2D

  s.plant().load_current_a = 400.0;        // poke the world...
  c.expect_reaches(s, BMS_STATE_FAULT_BMS, 200);   // ...check the firmware's response
}
```

Here is what you can change in the world (`s.plant()`):

| Area | Knobs |
| --- | --- |
| Shutdown loop | `external_loop_closed`, `shutdown_sense_glitch(start_us, end_us)` |
| IMD | `isolation_fault`, `imd_reset()`, `params.imd_in_loop` |
| Contactors | `air_minus/air_plus/precharge.fault = Contactor::Fault::{WeldedClosed, StuckOpen}` |
| Precharge | `precharge_resistor_shorted`, `precharge_resistor_open`, `params.precharge_tau_s`, `params.bus_bleed_tau_s` |
| Pack | `soc[i]`, `temp_c[i]`, `cell_v_offset[i]`, `load_current_a` |
| CAN nodes | `ivt_alive`, `dash_alive`, `r2d`, `others_on_can`, `charger.present`, `charger.hw_failure` |
| Cell monitor | `afe.alive` (task hung / isoSPI chain dead) |

These are the checks: `c.expect_state`, `c.expect_reaches`, `c.expect_holds`,
`c.expect_visited` and `c.expect(cond, fmt, ...)`.

## Fidelity: what is real and what is modelled

**Real firmware:** the whole state machine, including every transition, debounce,
timeout, fault route and recovery path.

**Modelled stand-ins** (they follow the real module's contract but are not its code):

* **ADBMS task:** scans every 100 ms, sets sticky fault flags after 3
  consecutive bad scans, and applies the temperature limit for the active
  profile. There is no isoSPI, PEC or thermistor maths.
* **Charger status:** uses the same decision order as `FEB_CAN_Charger.cpp`,
  including the `done_charging` latch and its re-arm on unplug.
* **Freshness checks** for the IVT, DASH, heartbeats and charger use the real
  modules' comparisons.

**Wiring assumptions** need checking against the SN5 schematic (see the top of
`src/plant.hpp`):

* `SHS_IN` is upstream of the BMS relay.
* The contactor coils are powered downstream of it.
* The IMD latch is in series with the loop.

**Plant numbers** are placeholders until they're replaced with measured values:

* 140s pack with 13 Ah cells at 1.2 mΩ
* 1 s precharge time constant
* 2 s bus bleed time constant
* 15/10 ms contactor close/open times

## CHARACTERIZATION scenarios

These scenarios pin down *current* behaviour that may deserve a design review.
They are not a claim that the behaviour is right:

* `welded_precharge_blocks_health_check`: if the precharge relay is welded
  before TSMS, the BMS sits in BUS_HEALTH_CHECK indefinitely. It raises no fault
  and gives no diagnostic.
* `quick_estop_release_trips_precharge_too_fast`: after a DRIVE trip, if the
  loop is restored before the DC bus bleeds down, re-precharge starts from about
  60% bus voltage. It then hits 90% in under 2 s, which trips the inrush check
  and latches a fault that needs a power cycle.
* `charging_hot_cell_soft_stops`: the 40 °C soft stop always fires before the
  45 °C CHARGING-profile hard fault can latch (that needs 3 scans). So the hard
  limit acts as a backstop that is never reached in practice.

## Next steps

* Bring more firmware modules under simulation, starting with
  `FEB_CAN_Charger.cpp` and the ADBMS validators. Each one needs its own
  lower-level seam (the CAN library or SPI).
* Publish the BMS's CAN frames to SocketCAN (`vcan0`) so DASH/PCU tools and
  other ECU sims can talk to it.
* Replay recorded CAN logs from the car as scenario inputs.
* Replace placeholder plant numbers with measured pack data.
