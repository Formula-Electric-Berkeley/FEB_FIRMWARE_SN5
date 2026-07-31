/**
 ******************************************************************************
 * @file           : feb_commands_2.hpp
 * @brief          : Default system commands for feb_console_2
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_2_HPP
#define FEB_COMMANDS_2_HPP

#include "feb_console_2.hpp"

namespace feb::console
{

namespace sys
{
void echo(Interaction &io, std::span<char *const> args);
void help(Interaction &io, std::span<char *const> args);
void commands(Interaction &io, std::span<char *const> args);
void hello(Interaction &io, std::span<char *const> args);
void version(Interaction &io, std::span<char *const> args);
void uptime(Interaction &io, std::span<char *const> args);
void reboot(Interaction &io, std::span<char *const> args);
void log(Interaction &io, std::span<char *const> args);
/** Sets the level named by args[0], so one handler serves every level entry. */
void log_set(Interaction &io, std::span<char *const> args);
void log_capture(Interaction &io, std::span<char *const> args);
} // namespace sys

inline constexpr std::array<Command, 7> kLogSubcommands = {{
    {.name = "none", .description = "Disable logging", .handler = sys::log_set},
    {.name = "error", .description = "Errors only", .handler = sys::log_set},
    {.name = "warn", .description = "Warnings and errors", .handler = sys::log_set},
    {.name = "info", .description = "Info and above", .handler = sys::log_set},
    {.name = "debug", .description = "Debug and above", .handler = sys::log_set},
    {.name = "trace", .description = "Everything", .handler = sys::log_set},
    {.name = "capture",
     .description = "Stream log output as csv log rows",
     .handler = sys::log_capture,
     .text_hidden = true,
     .csv_hidden = true},
}};

inline constexpr std::array<Command, 8> kSystemCommands = {{
    {.name = "echo",
     .description = "Print arguments: echo <text>",
     .handler = sys::echo,
     .args = "$text",
     .csv_hidden = true,
     .read_only = true},
    {.name = "help",
     .description = "Show this help screen",
     .handler = sys::help,
     .csv_hidden = true,
     .read_only = true},
    {.name = "commands",
     .description = "List commands (for CSV hosts)",
     .handler = sys::commands,
     .text_hidden = true,
     .csv_hidden = true,
     .read_only = true},
    {.name = "hello", .description = "Heartbeat / board discovery", .handler = sys::hello, .read_only = true},
    {.name = "version",
     .description = "Firmware version, commit, build and flash provenance",
     .handler = sys::version,
     .read_only = true},
    {.name = "uptime", .description = "Milliseconds since boot", .handler = sys::uptime, .read_only = true},
    {.name = "reboot", .description = "Perform a software reset", .handler = sys::reboot},
    {.name = "log",
     .description = "Show or set the log level",
     .handler = sys::log,
     .subs = kLogSubcommands,
     .read_only = true},
}};

} // namespace feb::console

#endif /* FEB_COMMANDS_2_HPP */
