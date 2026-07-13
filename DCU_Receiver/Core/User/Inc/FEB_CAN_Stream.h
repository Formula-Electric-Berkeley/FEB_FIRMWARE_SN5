/**
 * @file    FEB_CAN_Stream.h
 * @brief   Live CAN-frame console stream for DCU_Receiver
 * @author  Formula Electric @ Berkeley
 *
 * Mirrors the DCU's `dcu|can|stream` feature so a host application sees an
 * identical `can,...` row stream whether it is plugged into the car-side DCU or
 * the receiver. On the DCU the frames come from the local CAN buses; here they
 * come from frames reconstructed off the radio. The emitted row schema
 * (bus,can_id,dlc,d0..d7) and the on/off/status command surface match the DCU
 * byte-for-byte.
 */

#ifndef FEB_CAN_STREAM_H
#define FEB_CAN_STREAM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /**
   * @brief Enable/disable the live console stream.
   * @param on    true to start, false to stop (closes the session with `done`).
   * @param tx_id Transaction id attached to streamed `can` rows; required when
   *              on==true, ignored otherwise. Copied internally.
   */
  void FEB_CAN_Stream_SetStream(bool on, const char *tx_id);

  /** @return true while streaming is active. */
  bool FEB_CAN_Stream_IsStreaming(void);

  /** @return active stream tx_id (empty string when not streaming). */
  const char *FEB_CAN_Stream_GetTxId(void);

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
   * @param rssi  Last-packet RSSI in dBm (ignored when !valid).
   * @param snr   Last-packet SNR in dB (ignored when !valid).
   */
  void FEB_CAN_Stream_EmitSignal(bool valid, int16_t rssi, int8_t snr);

  /**
   * @brief Register the CSV-protocol can-stream-on/off/status handlers.
   *        Call after FEB_Console_Init. Idempotent.
   */
  void FEB_CAN_Stream_RegisterCsvHandlers(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_STREAM_H */
