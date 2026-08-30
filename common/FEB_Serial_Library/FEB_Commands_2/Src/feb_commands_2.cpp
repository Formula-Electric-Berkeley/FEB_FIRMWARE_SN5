/**
 ******************************************************************************
 * @file           : feb_commands_2.cpp
 * @brief          : Default system commands for feb_console_2
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_commands_2.hpp"

#include "feb_log.h"
#include "feb_uart.h"
#include "feb_version.h"

#include "main.h"

#include <cstdio>

namespace feb::console::sys
{
namespace
{

constexpr struct
{
  const char *name;
  FEB_Log_Level_t level;
} kLogLevels[] = {
    {"none", FEB_LOG_NONE}, {"error", FEB_LOG_ERROR}, {"warn", FEB_LOG_WARN},
    {"info", FEB_LOG_INFO}, {"debug", FEB_LOG_DEBUG}, {"trace", FEB_LOG_TRACE},
};

const char *log_level_name(FEB_Log_Level_t level)
{
  for (const auto &e : kLogLevels)
  {
    if (e.level == level)
    {
      return e.name;
    }
  }
  return "?";
}

Interaction::Handle s_log_capture;
bool s_capturing = false;

bool log_tap(FEB_Log_Level_t level, const char *tag, const char *msg, std::size_t len)
{
  if (!s_log_capture.valid() || s_capturing)
  {
    return false;
  }

  char tag_field[48];
  char msg_field[kRowBufferSize / 2];
  escape_field((tag != nullptr) ? tag : "", tag_field, sizeof(tag_field));
  escape_into_field(msg, len, msg_field, sizeof(msg_field));

  s_capturing = true;
  s_log_capture.emit("log", "%s,%s,%s", log_level_name(level), tag_field, msg_field);
  s_capturing = false;
  return true; /* claimed: do not also render as text */
}

void log_capture_stop()
{
  FEB_Log_SetTap(nullptr);
  if (s_log_capture.valid())
  {
    s_log_capture.emit("done");
    s_log_capture = Interaction::Handle{};
  }
}

void copy_volatile(char *dst, const volatile char *src, std::size_t n)
{
  std::size_t i = 0;
  for (; i + 1 < n && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  dst[i] = '\0';
}

} // namespace

void echo(Interaction &io, std::span<char *const> args)
{
  for (std::size_t i = 1; i < args.size(); i++)
  {
    io.print("%s%s", (i > 1) ? " " : "", args[i]);
  }
  io.print("\r\n");
}

void help(Interaction &io, std::span<char *const> args)
{
  if (args.size() >= 2)
  {
    const Command *cmd = io.console().find(args[1]);
    if (cmd != nullptr)
    {
      io.option(*cmd);
    }
    else
    {
      io.print("Unknown command: %s\r\n", args[1]);
    }
    return;
  }

  io.print("%s commands (space or | between words):\r\n", feb_build_info.board_name);
  for (const Command &cmd : io.console().commands())
  {
    if (!cmd.text_hidden)
    {
      io.option(cmd);
    }
  }
}

void commands(Interaction &io, std::span<char *const>)
{
  for (const Command &cmd : io.console().commands())
  {
    if (cmd.csv_hidden)
    {
      continue;
    }
    io.option(cmd);
  }
}

void hello(Interaction &io, std::span<char *const>)
{
  io.print("Hello from %s\r\n", feb_build_info.board_name);
}

void version(Interaction &io, std::span<char *const>)
{
  char flash_utc[sizeof(feb_flash_info.flash_utc)];
  char flasher_user[sizeof(feb_flash_info.flasher_user)];
  char flasher_host[sizeof(feb_flash_info.flasher_host)];
  copy_volatile(flash_utc, feb_flash_info.flash_utc, sizeof(flash_utc));
  copy_volatile(flasher_user, feb_flash_info.flasher_user, sizeof(flasher_user));
  copy_volatile(flasher_host, feb_flash_info.flasher_host, sizeof(flasher_host));

  static constexpr Column kCols[] = {{"Field", 8}, {"Value", 58}};
  Table t(io, kCols, "FEB Firmware", false);

  t.cell("Board");
  t.cell("%s", feb_build_info.board_name);
  t.end_row();

  t.cell("Version");
  t.cell("%s (board) | %s (repo) | %s (common)", feb_build_info.version_string, feb_build_info.repo_version_string,
         feb_build_info.common_version_string);
  t.end_row();

  t.cell("Commit");
  t.cell("%s (%s)%s", feb_build_info.commit_short, feb_build_info.branch, feb_build_info.dirty ? " [DIRTY]" : "");
  t.end_row();

  t.cell("SHA");
  t.cell("%s", feb_build_info.commit_full);
  t.end_row();

  t.cell("Built");
  t.cell("%s by %s@%s", feb_build_info.build_utc, feb_build_info.build_user, feb_build_info.build_host);
  t.end_row();

  t.cell("Flashed");
  if (FEB_Version_IsUnflashed())
  {
    t.cell("(unflashed - programmed without flash-patcher)");
  }
  else
  {
    t.cell("%s by %s@%s", flash_utc, flasher_user, flasher_host);
  }
  t.end_row();

#ifdef __GNUC__
  t.cell("Compiler");
  t.cell("GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
  t.end_row();
#endif
}

void uptime(Interaction &io, std::span<char *const>)
{
  const std::uint32_t ms = HAL_GetTick();
  const std::uint32_t sec = ms / 1000;
  const std::uint32_t min = sec / 60;
  io.print("Uptime: %lu ms (%lu:%02lu:%02lu)\r\n", (unsigned long)ms, (unsigned long)(min / 60),
           (unsigned long)(min % 60), (unsigned long)(sec % 60));
}

void reboot(Interaction &io, std::span<char *const>)
{
  io.print("Rebooting...\r\n");
  io.flush(); /* never returns, so the dispatcher will not flush for us */
  FEB_UART_Flush(io.console().uart(), 100);
  NVIC_SystemReset();
}

void log_capture(Interaction &io, std::span<char *const> args)
{
  if (args.size() >= 2 && iequal(args[1], "off"))
  {
    log_capture_stop();
    io.print("Log capture: off\r\n");
    io.option("start", "log capture", "Stream log output as csv log rows");
    return;
  }

  if (!io.is_csv())
  {
    io.error("error", "csv_only", "%s needs a csv transaction", io.path());
    return;
  }

  log_capture_stop();
  s_log_capture = io.detach();
  FEB_Log_SetTap(log_tap);
}

void log(Interaction &io, std::span<char *const>)
{
  io.println("Log level: %s", log_level_name(FEB_Log_GetLevel()));
}

void log_set(Interaction &io, std::span<char *const> args)
{
  for (const auto &e : kLogLevels)
  {
    if (iequal(args[0], e.name))
    {
      FEB_Log_SetLevel(e.level);
      io.println("Log level set to: %s", e.name);
      return;
    }
  }
}

} // namespace feb::console::sys
