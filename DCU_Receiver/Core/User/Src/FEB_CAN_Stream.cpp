/**
******************************************************************************
* @file           : FEB_CAN_Stream.cpp
* @brief          : Live CAN-frame console stream for DCU_Receiver
* @author         : Formula Electric @ Berkeley
******************************************************************************
*/

#include "FEB_CAN_Stream.h"

#include <cstdio>

using namespace feb::console;

namespace
{
volatile bool s_active = false;
Interaction::Handle s_stream;

int format_row(char *out, std::size_t cap, std::uint8_t bus, std::uint32_t can_id, std::uint8_t dlc,
               const std::uint8_t *data)
{
  static const char hex[] = "0123456789ABCDEF";

  if (dlc > 8U)
  {
    dlc = 8U;
  }

  char data_field[32];
  std::size_t pos = 0;
  for (std::uint8_t i = 0; i < 8U; i++)
  {
    if (i > 0)
    {
      data_field[pos++] = ',';
    }
    if (i < dlc)
    {
      data_field[pos++] = hex[(data[i] >> 4) & 0x0F];
      data_field[pos++] = hex[data[i] & 0x0F];
    }
  }
  data_field[pos] = '\0';

  const int n =
      std::snprintf(out, cap, "%u,0x%lX,%u,%s", (unsigned)bus, (unsigned long)can_id, (unsigned)dlc, data_field);
  return (n < 0 || (std::size_t)n >= cap) ? -1 : n;
}

} // namespace

/* ============================================================================
 * Stream control
 * ============================================================================ */

void FEB_CAN_Stream_Start(Interaction &io)
{
  FEB_CAN_Stream_Stop();
  if (!io.is_csv())
  {
    io.error("error", "csv_only", "%s needs a csv transaction", io.path());
    return;
  }

  s_stream = io.detach();
  s_active = true;
}

void FEB_CAN_Stream_Stop(void)
{
  if (!s_active)
  {
    return;
  }
  s_active = false;
  if (s_stream.valid())
  {
    s_stream.emit("done");
    s_stream = Interaction::Handle{};
  }
}

bool FEB_CAN_Stream_IsStreaming(void)
{
  return s_active;
}

void FEB_CAN_Stream_EmitFrame(std::uint8_t bus, std::uint32_t can_id, std::uint8_t dlc, const std::uint8_t *data)
{
  if (!s_active || !s_stream.valid())
  {
    return;
  }
  char row[64];
  if (format_row(row, sizeof(row), bus, can_id, dlc, data) > 0)
  {
    s_stream.emit("can", "%s", row);
  }
}

void FEB_CAN_Stream_EmitSignal(bool valid, std::int16_t rssi, std::int8_t snr)
{
  if (!s_active || !s_stream.valid())
  {
    return;
  }
  if (valid)
  {
    s_stream.emit("signal", "%d,%d", (int)rssi, (int)snr);
  }
  else
  {
    s_stream.emit("signal", "nan,nan");
  }
}
