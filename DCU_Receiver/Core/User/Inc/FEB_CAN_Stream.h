/**
 * @file    FEB_CAN_Stream.h
 * @brief   Live CAN-frame console stream for DCU_Receiver
 * @author  Formula Electric @ Berkeley
 *
 * Mirrors the DCU's `dcu|can|stream` feature so a host application sees an
 * identical `can,...` row stream whether it is plugged into the car-side DCU or
 * the receiver. On the DCU the frames come from the local CAN buses; here they
 * come from frames reconstructed off the radio. The emitted row schema
 * (bus,can_id,dlc,d0..d7) matches the DCU byte-for-byte.
 */

#ifndef FEB_CAN_STREAM_H
#define FEB_CAN_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @return true while streaming is active. */
  bool FEB_CAN_Stream_IsStreaming(void);

  /**
   * @brief Stop the stream, emitting the `done`
   */
  void FEB_CAN_Stream_Stop(void);

  /**
   * @brief Emit one CAN frame as a `can,...` row if streaming is active.
   *
   * Schema matches the DCU exactly: bus,can_id,dlc,d0,...,d7 (hex bytes, empty
   * fields beyond dlc). No-op when streaming is off.
   */
  void FEB_CAN_Stream_EmitFrame(uint8_t bus, uint32_t can_id, uint8_t dlc, const uint8_t *data);

  /**
   * @brief Emit a `signal` row (radio link quality) if streaming is active.
   *
   * Schema: signal,<rssi>,<snr>, or signal,nan,nan when the link is down. Meant
   * to be called periodically (~2 Hz) so a host watching the CAN stream also
   * sees link health. No-op when streaming is off.
   *
   * @param valid false when no packet has arrived recently — emits nan,nan so
   *              the UI can show "no link" instead of a stale reading.
   */
  void FEB_CAN_Stream_EmitSignal(bool valid, int16_t rssi, int8_t snr);

#ifdef __cplusplus
}

#include "feb_console_2.hpp"

void FEB_CAN_Stream_Start(feb::console::Interaction &io);

#endif

#endif /* FEB_CAN_STREAM_H */
