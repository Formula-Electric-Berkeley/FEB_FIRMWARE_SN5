/**
 ******************************************************************************
 * @file           : feb_console_2.hpp
 * @brief          : FEB Console Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CONSOLE_2_HPP
#define FEB_CONSOLE_2_HPP

#include "feb_uart.h"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

namespace feb::console
{

inline constexpr std::size_t kMaxArgs = 32;
inline constexpr std::size_t kLineBufferSize = 192;
inline constexpr std::size_t kRowBufferSize = 320;
inline constexpr std::size_t kTxIdMaxLen = 32;

class Console;
class Interaction;

struct Command
{
  using Handler = void (*)(Interaction &io, std::span<char *const> args);

  const char *name;
  const char *description;
  Handler handler = nullptr;
  const char *args = nullptr; // placeholder suffix, e.g. "$timeout_ms:int"

  std::span<const Command> subs{};

  bool text_hidden = false; // omit from text-mode listings (`help`, parent screens)
  bool csv_hidden = false;  // omit from csv listings (`commands`, parent screens)
  bool read_only = false;   // emits `flags,read_only` when run
};

template <std::size_t N>
constexpr Command group(const char *name, const char *description, const std::array<Command, N> &subs)
{
  return {name, description, nullptr, nullptr, subs};
}

/**
 * Dispatch args[1] against @p subs, listing them as options when it matches
 * nothing. @p path is prepended to each listed command. A handler receives args
 * starting at its own name.
 */
void subcmds_dispatch(Interaction &io, std::span<char *const> args, std::span<const Command> subs, const char *path);

constexpr bool iequal(const char *a, const char *b)
{
  auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; };
  for (; *a != '\0' && *b != '\0'; ++a, ++b)
  {
    if (lower(*a) != lower(*b))
    {
      return false;
    }
  }
  return *a == *b;
}

template <std::size_t... Ns> constexpr auto concat(const std::array<Command, Ns> &...tables)
{
  std::array<Command, (Ns + ... + 0)> out{};
  std::size_t i = 0;
  (
      (void)[&] {
        for (const Command &c : tables)
        {
          out[i++] = c;
        }
      }(),
      ...);
  return out;
}

constexpr bool has_duplicate_names(std::span<const Command> cmds)
{
  for (std::size_t i = 0; i < cmds.size(); ++i)
  {
    for (std::size_t j = i + 1; j < cmds.size(); ++j)
    {
      if (iequal(cmds[i].name, cmds[j].name))
      {
        return true;
      }
    }
    if (has_duplicate_names(cmds[i].subs))
    {
      return true;
    }
  }
  return false;
}

class Interaction
{
public:
  class Handle
  {
  public:
    Handle() = default;

    bool valid() const
    {
      return console_ != nullptr;
    }

    const char *tx_id() const
    {
      return tx_id_;
    }

    int emit(const char *type, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
    int emit(const char *type);

  private:
    friend class Interaction;

    const Console *console_ = nullptr;
    char tx_id_[kTxIdMaxLen + 1] = {};
  };

  Interaction(const Console &console, const char *tx_id, bool csv);
  ~Interaction();

  Interaction(const Interaction &) = delete;
  Interaction &operator=(const Interaction &) = delete;

  /**
   * Suppress the automatic `done` and keep this transaction open; the Handle
   * must eventually emit it. CSV-only: returns an invalid handle in text mode.
   */
  Handle detach();

  int print(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  int emit(const char *type, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
  int println(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /** `log,<level>,<tag>,<message>` level is error/warn/info/debug/trace. */
  int log(const char *level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
  int error(const char *level, const char *code);
  int error(const char *level, const char *code, const char *fmt, ...) __attribute__((format(printf, 4, 5)));

  int option(const char *label, const char *command, const char *description = nullptr);

  int flags(const char *flags);

  /** @p path is nullptr for a top-level command. */
  int option(const Command &cmd, const char *path = nullptr);

  /**
   * Each validates args[index] and, on failure, emits the `error` row itself
   * before returning false. @p index counts from args[0] = the command name.
   */
  bool arg_int(std::span<char *const> args, std::size_t index, long *out, long min = LONG_MIN, long max = LONG_MAX);
  bool arg_float(std::span<char *const> args, std::size_t index, float *out);
  /** Accepts on/off, true/false, yes/no, 1/0. */
  bool arg_bool(std::span<char *const> args, std::size_t index, bool *out);
  /** @return nullptr when absent. */
  const char *arg_str(std::span<char *const> args, std::size_t index);

  /** Only handlers that never return (ex: `reboot`) need to call this. */
  int flush();

  bool is_csv() const
  {
    return csv_;
  }

  /** The path this handler was reached by, e.g. "dcu can stream". */
  const char *path() const
  {
    return path_;
  }

  void set_path(const char *path)
  {
    path_ = path;
  }

  const Console &console() const
  {
    return *console_;
  }

private:
  friend class Table;
  friend class Console;

  int emit_row(const char *type, const char *body, std::size_t body_len);

  const Console *console_;
  char tx_id_[kTxIdMaxLen + 1];
  bool csv_;
  bool detached_;
  char line_[kRowBufferSize];
  std::size_t line_len_;
  const char *path_ = "";
};

/** @p width is text-mode cosmetic only; CSV rows carry the full value. */
struct Column
{
  const char *name;
  std::uint8_t width;
};

class Table
{
public:
  Table(Interaction &io, std::span<const Column> cols, const char *title = nullptr, bool show_header = true);
  ~Table();

  Table(const Table &) = delete;
  Table &operator=(const Table &) = delete;

  void cell(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void end_row();

private:
  void start_row();
  void append_raw(const char *s, std::size_t len);
  void append_str(const char *s);
  void append_cell_csv(const char *text);
  void append_text_cell(const char *text, std::uint8_t width);
  void append_rule(const char *left, const char *mid, const char *right);
  void append_span_rule(const char *left, const char *right);
  void append_title_row(const char *title);
  std::size_t inner_width() const;

  Interaction &io_;
  std::span<const Column> cols_;
  std::size_t col_;
  char row_[kRowBufferSize];
  std::size_t row_len_;
};

class Console
{
public:
  constexpr Console(FEB_UART_Instance_t uart, std::span<const Command> commands) : uart_(uart), commands_(commands) {}

  void process_line(const char *line, std::size_t len) const;
  const Command *find(const char *name) const;

  std::span<const Command> commands() const
  {
    return commands_;
  }

  constexpr FEB_UART_Instance_t uart() const
  {
    return uart_;
  }

private:
  FEB_UART_Instance_t uart_;
  std::span<const Command> commands_;
};

/**
 * Escape a value for a field the host splits on. Needed only for emit().
 */
std::size_t escape_field(const char *in, char *out, std::size_t cap);

/** As escape_field(), for a counted buffer that need not be null-terminated. */
std::size_t escape_into_field(const char *in, std::size_t in_len, char *out, std::size_t cap);

} // namespace feb::console

#endif /* FEB_CONSOLE_2_HPP */
