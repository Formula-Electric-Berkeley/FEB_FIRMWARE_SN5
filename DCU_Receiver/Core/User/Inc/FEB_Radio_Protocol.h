/**
 * @file    FEB_Radio_Protocol.h
 * @brief   Wire format for CAN-over-LoRa packets (TX = DCU, RX = DCU_Receiver)
 * @author  Formula Electric @ Berkeley
 *
 * ============================================================================
 *  !!! KEEP THIS FILE BYTE-IDENTICAL BETWEEN DCU/ AND DCU_Receiver/ !!!
 *  Both ends parse the same bytes; any divergence silently corrupts frames.
 *  It is intentionally header-only (static inline) so there is no .c to add to
 *  either CubeMX-generated CMakeLists.
 * ============================================================================
 *
 * PACKET TYPE REGISTRY (byte 0 = magic = packet type). Every packet on the link
 * begins with one of these so the receiver can dispatch by type. Add new types
 * here with a fresh magic; pick distinctive non-ASCII bytes so they never
 * collide with the plain-text PING/PONG link-check packets.
 *
 *   0xFB  FEB_RADIO_MAGIC_BATCH  — batch of N CAN frames (the CAN format)
 *   0xFA  FEB_RADIO_MAGIC_TEXT   — RESERVED: ASCII message (e.g. receiver->DCU)
 *   (0x20..0x7E first bytes are reserved for the ASCII PING/PONG demo traffic)
 *
 * --- 0xFB  batch of CAN frames ----------------------------------------------
 *   Amortizes the per-packet preamble across many frames (the single biggest
 *   throughput win):
 *   [0]     0xFB
 *   [1]     frame_count N  (1..FEB_RADIO_BATCH_MAX_FRAMES)
 *   then N records, each:
 *     [0..3] can_id (uint32 LE; 29-bit max, upper bits zero)
 *     [4]    meta = (id_type << 7) | (bus << 4) | (dlc & 0x0F)
 *              bit 7    : 1 = extended 29-bit id, 0 = standard 11-bit
 *              bits 6-4 : source bus (1 = CAN1, 2 = CAN2)
 *              bits 3-0 : dlc (0..8)
 *     [5..]  payload (dlc bytes)
 *   Record size = 5 + dlc (max 13 bytes for an 8-byte frame).
 */

#ifndef FEB_RADIO_PROTOCOL_H
#define FEB_RADIO_PROTOCOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FEB_RADIO_MAGIC_BATCH 0xFBU /* batched multi-frame CAN packet */
#define FEB_RADIO_MAGIC_TEXT 0xFAU  /* RESERVED: ASCII message packet */

/* Cap frames/batch so a full batch stays well under the 255-byte LoRa limit:
 * 2 (header) + 16 * 13 (max record) = 210 bytes. */
#define FEB_RADIO_BATCH_MAX_FRAMES 16U
#define FEB_RADIO_BATCH_HEADER_BYTES 2U
#define FEB_RADIO_RECORD_MAX_BYTES 13U /* 4 id + 1 meta + 8 data */
#define FEB_RADIO_BATCH_MAX_BYTES \
  (FEB_RADIO_BATCH_HEADER_BYTES + FEB_RADIO_BATCH_MAX_FRAMES * FEB_RADIO_RECORD_MAX_BYTES)

/* ============================================================================
 * Encoder (TX side) — incremental batch builder
 * ============================================================================ */

typedef struct
{
  uint8_t *buf; /* caller-owned, >= FEB_RADIO_BATCH_MAX_BYTES */
  uint8_t cap;  /* capacity of buf */
  uint8_t len;  /* bytes used so far */
} FEB_Radio_BatchBuilder_t;

/** Begin a new batch in @p buf. Writes magic + zero count; len becomes 2. */
static inline void FEB_Radio_BatchBegin(FEB_Radio_BatchBuilder_t *b, uint8_t *buf, uint8_t cap)
{
  b->buf = buf;
  b->cap = cap;
  b->len = FEB_RADIO_BATCH_HEADER_BYTES;
  buf[0] = FEB_RADIO_MAGIC_BATCH;
  buf[1] = 0; /* frame_count, bumped by each Add */
}

/** @return frames added to the batch so far. */
static inline uint8_t FEB_Radio_BatchCount(const FEB_Radio_BatchBuilder_t *b)
{
  return b->buf[1];
}

/**
 * Append one CAN frame to the batch.
 * @return true on success, false if the batch is full (by frame count or by
 *         remaining capacity) — caller should transmit and start a new batch.
 */
static inline bool FEB_Radio_BatchAdd(FEB_Radio_BatchBuilder_t *b, uint32_t can_id, uint8_t id_type, uint8_t bus,
                                      const uint8_t *data, uint8_t dlc)
{
  if (dlc > 8U)
  {
    dlc = 8U;
  }
  if (b->buf[1] >= FEB_RADIO_BATCH_MAX_FRAMES)
  {
    return false;
  }
  const uint8_t need = 5U + dlc;
  if ((uint16_t)b->len + need > b->cap)
  {
    return false;
  }

  uint8_t *p = &b->buf[b->len];
  p[0] = (uint8_t)(can_id & 0xFFU);
  p[1] = (uint8_t)((can_id >> 8) & 0xFFU);
  p[2] = (uint8_t)((can_id >> 16) & 0xFFU);
  p[3] = (uint8_t)((can_id >> 24) & 0xFFU);
  p[4] = (uint8_t)(((id_type & 0x01U) << 7) | ((bus & 0x07U) << 4) | (dlc & 0x0FU));
  for (uint8_t i = 0; i < dlc; i++)
  {
    p[5U + i] = data[i];
  }

  b->len = (uint8_t)(b->len + need);
  b->buf[1]++;
  return true;
}

/* ============================================================================
 * Decoder (RX side) — parse either packet type, invoke callback per frame
 * ============================================================================ */

/** Per-frame callback. @p data points into the RX buffer (valid during call). */
typedef void (*FEB_Radio_FrameFn)(uint32_t can_id, uint8_t id_type, uint8_t bus, const uint8_t *data, uint8_t dlc,
                                  void *ctx);

/**
 * Parse a received LoRa payload and call @p fn once per CAN frame it carries.
 * Handles both 0xFB batch and 0xFE legacy single-frame packets.
 * @return number of frames delivered, or -1 if the packet is malformed.
 */
static inline int FEB_Radio_Parse(const uint8_t *buf, uint8_t len, FEB_Radio_FrameFn fn, void *ctx)
{
  if (buf == NULL || len < 1U)
  {
    return -1;
  }

  if (buf[0] == FEB_RADIO_MAGIC_BATCH)
  {
    if (len < FEB_RADIO_BATCH_HEADER_BYTES)
    {
      return -1;
    }
    uint8_t count = buf[1];
    size_t off = FEB_RADIO_BATCH_HEADER_BYTES;
    for (uint8_t i = 0; i < count; i++)
    {
      if (off + 5U > len)
      {
        return -1; /* truncated record header */
      }
      uint32_t can_id = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) | ((uint32_t)buf[off + 2] << 16) |
                        ((uint32_t)buf[off + 3] << 24);
      uint8_t meta = buf[off + 4];
      uint8_t id_type = (meta >> 7) & 0x01U;
      uint8_t bus = (meta >> 4) & 0x07U;
      uint8_t dlc = meta & 0x0FU;
      if (dlc > 8U || off + 5U + dlc > len)
      {
        return -1; /* malformed dlc or truncated payload */
      }
      fn(can_id, id_type, bus, &buf[off + 5U], dlc, ctx);
      off += 5U + dlc;
    }
    return (int)count;
  }

  return -1; /* unknown magic — not ours */
}

#ifdef __cplusplus
}
#endif

#endif /* FEB_RADIO_PROTOCOL_H */
