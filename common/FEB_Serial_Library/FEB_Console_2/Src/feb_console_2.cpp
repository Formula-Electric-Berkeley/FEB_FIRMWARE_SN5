/**
 ******************************************************************************
 * @file           : feb_console_2.cpp
 * @brief          : FEB Console Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_console_2.hpp"

#include "feb_time.h"
#include "feb_version.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace feb::console
{
namespace
{

int uart_write(FEB_UART_Instance_t inst, const char *data, std::size_t len)
{
  return FEB_UART_Write(inst, reinterpret_cast<const std::uint8_t *>(data), len);
}

std::size_t u64_to_decimal(std::uint64_t v, char *out, std::size_t cap)
{
  if (cap == 0)
  {
    return 0;
  }
  char tmp[21];
  std::size_t i = 0;
  if (v == 0)
  {
    tmp[i++] = '0';
  }
  while (v > 0)
  {
    tmp[i++] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  const std::size_t n = (i < cap - 1) ? i : (cap - 1);
  for (std::size_t j = 0; j < n; j++)
  {
    out[j] = tmp[i - 1 - j];
  }
  out[n] = '\0';
  return n;
}

std::size_t escape_into(const char *in, std::size_t in_len, char *out, std::size_t cap)
{
  std::size_t o = 0;
  for (std::size_t i = 0; i < in_len && cap > 0; i++)
  {
    const char c = in[i];
    const char *esc = nullptr;
    switch (c)
    {
    case '\\':
      esc = "\\\\";
      break;
    case '\r':
      esc = "\\r";
      break;
    case '\n':
      esc = "\\n";
      break;
    case ',':
      esc = "\\,";
      break;
    default:
      break;
    }

    if (esc != nullptr)
    {
      if (o + 2 >= cap)
      {
        break;
      }
      out[o++] = esc[0];
      out[o++] = esc[1];
    }
    else
    {
      if (o + 1 >= cap)
      {
        break;
      }
      out[o++] = c;
    }
  }
  if (cap > 0)
  {
    out[o] = '\0';
  }
  return o;
}

bool is_delim(char c)
{
  return c == ' ' || c == '|';
}

/* Each token is terminated only after the read cursor
   has stepped past the separator, which would otherwise be overwritten. */
int parse_args(char *line, char *argv[], int max_args)
{
  int argc = 0;
  char *w = line;
  const char *p = line;

  while (argc < max_args)
  {
    while (is_delim(*p))
    {
      p++;
    }
    if (*p == '\0')
    {
      break;
    }

    argv[argc++] = w;
    bool quoted = false;
    while (*p != '\0' && (quoted || !is_delim(*p)))
    {
      if (*p == '"')
      {
        quoted = !quoted;
        p++;
        continue;
      }
      *w++ = *p++;
    }

    if (*p != '\0')
    {
      p++;
    }
    *w++ = '\0';
  }

  return argc;
}

bool tx_id_is_valid(const char *s)
{
  if (s == nullptr || s[0] == '\0')
  {
    return false;
  }
  std::size_t n = 0;
  for (const char *p = s; *p != '\0'; p++, n++)
  {
    if (n >= kTxIdMaxLen || *p == ',' || *p == '|' || *p == ' ' || *p == '"' || *p == '\r' || *p == '\n')
    {
      return false;
    }
  }
  return true;
}

const char *board_name()
{
  return (feb_build_info.board_name != nullptr) ? feb_build_info.board_name : "?";
}

#if defined(FEB_CONSOLE_UNICODE_TABLE) && FEB_CONSOLE_UNICODE_TABLE
constexpr const char *kBoxH = "─";
constexpr const char *kBoxV = "│";
constexpr const char *kBoxTL = "┌";
constexpr const char *kBoxTM = "┬";
constexpr const char *kBoxTR = "┐";
constexpr const char *kBoxML = "├";
constexpr const char *kBoxMM = "┼";
constexpr const char *kBoxMR = "┤";
constexpr const char *kBoxBL = "└";
constexpr const char *kBoxBM = "┴";
constexpr const char *kBoxBR = "┘";
#else
constexpr const char *kBoxH = "-";
constexpr const char *kBoxV = "|";
constexpr const char *kBoxTL = "+";
constexpr const char *kBoxTM = "+";
constexpr const char *kBoxTR = "+";
constexpr const char *kBoxML = "+";
constexpr const char *kBoxMM = "+";
constexpr const char *kBoxMR = "+";
constexpr const char *kBoxBL = "+";
constexpr const char *kBoxBM = "+";
constexpr const char *kBoxBR = "+";
#endif

bool board_matches(const char *addr)
{
  if (addr == nullptr)
  {
    return false;
  }
  if (addr[0] == '*' && addr[1] == '\0')
  {
    return true;
  }
  return (feb_build_info.board_name != nullptr) && iequal(addr, feb_build_info.board_name);
}

/* "radio rx $timeout_ms:int" -> "radio rx <timeout_ms>". */
std::size_t render_placeholders(const char *command, char *out, std::size_t cap)
{
  std::size_t o = 0;
  const char *p = command;

  while (*p != '\0' && o + 1 < cap)
  {
    if (*p != '$')
    {
      out[o++] = *p++;
      continue;
    }

    p++;
    out[o++] = '<';
    while (*p != '\0' && *p != '|' && *p != ' ' && *p != ':' && o + 1 < cap)
    {
      out[o++] = *p++;
    }
    if (*p == ':') /* discard ":type" */
    {
      while (*p != '\0' && *p != '|' && *p != ' ')
      {
        p++;
      }
    }
    if (o + 1 < cap)
    {
      out[o++] = '>';
    }
  }

  out[o] = '\0';
  return o;
}

int write_row(FEB_UART_Instance_t uart, const char *tx_id, const char *type, const char *body, std::size_t body_len)
{
  char us_str[24];
  u64_to_decimal(FEB_Time_Us(), us_str, sizeof(us_str));

  char buf[kRowBufferSize];
  int n = std::snprintf(buf, sizeof(buf), "csv,%s,%s,%s,%s", tx_id, board_name(), us_str, type);
  if (n < 0)
  {
    return n;
  }
  /* Always leave room for the trailing CRLF even if the prefix alone
     overflowed */
  if (static_cast<std::size_t>(n) > sizeof(buf) - 2)
  {
    n = static_cast<int>(sizeof(buf)) - 2;
  }

  if (body != nullptr && static_cast<std::size_t>(n) + 1 < sizeof(buf) - 2)
  {
    buf[n++] = ',';
    const std::size_t room = sizeof(buf) - 2 - static_cast<std::size_t>(n);
    const std::size_t take = (body_len < room) ? body_len : room;
    std::memcpy(buf + n, body, take);
    n += static_cast<int>(take);
  }

  buf[n++] = '\r';
  buf[n++] = '\n';
  return uart_write(uart, buf, static_cast<std::size_t>(n));
}

int write_row_v(FEB_UART_Instance_t uart, const char *tx_id, const char *type, const char *fmt, std::va_list ap)
{
  char body[kRowBufferSize];
  int n = std::vsnprintf(body, sizeof(body), fmt, ap);
  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(body))
  {
    n = static_cast<int>(sizeof(body)) - 1;
  }
  return write_row(uart, tx_id, type, body, static_cast<std::size_t>(n));
}

} // namespace

std::size_t escape_field(const char *in, char *out, std::size_t cap)
{
  return escape_into(in, (in != nullptr) ? std::strlen(in) : 0, out, cap);
}

std::size_t escape_into_field(const char *in, std::size_t in_len, char *out, std::size_t cap)
{
  return escape_into(in, (in != nullptr) ? in_len : 0, out, cap);
}

/* @p path is the prefix before cmd.name, or nullptr at the top. */
namespace
{
void run(Interaction &io, const Command &cmd, std::span<char *const> args, const char *path)
{
  char full[96];
  if (path != nullptr && path[0] != '\0')
  {
    std::snprintf(full, sizeof(full), "%s %s", path, cmd.name);
  }
  else
  {
    std::snprintf(full, sizeof(full), "%s", cmd.name);
  }

  if (args.size() >= 2)
  {
    for (const Command &child : cmd.subs)
    {
      if (iequal(child.name, args[1]))
      {
        run(io, child, args.subspan(1), full);
        return;
      }
    }
  }

  if (cmd.handler != nullptr)
  {
    if (cmd.read_only)
    {
      io.flags("read_only");
    }
    const char *prev = io.path();
    io.set_path(full);
    cmd.handler(io, args);
    io.set_path(prev);
  }
  else if (args.size() >= 2)
  {
    io.error("error", "unknown_subcommand", "%s", args[1]);
  }

  for (const Command &child : cmd.subs)
  {
    if (io.is_csv() ? child.csv_hidden : child.text_hidden)
    {
      continue;
    }
    io.option(child, full);
  }
}
} // namespace

void subcmds_dispatch(Interaction &io, std::span<char *const> args, std::span<const Command> subs, const char *path)
{
  if (args.size() >= 2)
  {
    for (const Command &c : subs)
    {
      if (iequal(c.name, args[1]))
      {
        run(io, c, args.subspan(1), path);
        return;
      }
    }
    io.error("error", "unknown_subcommand", "%s", args[1]);
  }

  for (const Command &c : subs)
  {
    if (io.is_csv() ? c.csv_hidden : c.text_hidden)
    {
      continue;
    }
    io.option(c, path);
  }
}

// MARK: Interaction

Interaction::Interaction(const Console &console, const char *tx_id, bool csv)
    : console_(&console), csv_(csv), detached_(false), line_len_(0)
{
  if (tx_id != nullptr)
  {
    std::strncpy(tx_id_, tx_id, kTxIdMaxLen);
    tx_id_[kTxIdMaxLen] = '\0';
  }
  else
  {
    tx_id_[0] = '\0';
  }
}

Interaction::~Interaction()
{
  flush();
}

int Interaction::emit_row(const char *type, const char *body, std::size_t body_len)
{
  return write_row(console_->uart(), tx_id_, type, body, body_len);
}

Interaction::Handle Interaction::detach()
{
  /* Text mode has no transaction to hold open. Hand back an invalid handle instead. */
  if (!csv_)
  {
    return Handle{};
  }

  detached_ = true;
  Handle h;
  h.console_ = console_;
  std::memcpy(h.tx_id_, tx_id_, sizeof(h.tx_id_));
  return h;
}

int Interaction::Handle::emit(const char *type, const char *fmt, ...)
{
  if (!valid())
  {
    return -1;
  }
  std::va_list ap;
  va_start(ap, fmt);
  const int r = write_row_v(console_->uart(), tx_id_, type, fmt, ap);
  va_end(ap);
  return r;
}

int Interaction::Handle::emit(const char *type)
{
  return valid() ? write_row(console_->uart(), tx_id_, type, nullptr, 0) : -1;
}

const char *Interaction::arg_str(std::span<char *const> args, std::size_t index)
{
  if (index >= args.size())
  {
    error("error", "missing_argument", "%s", path_);
    return nullptr;
  }
  return args[index];
}

bool Interaction::arg_int(std::span<char *const> args, std::size_t index, long *out, long min, long max)
{
  const char *tok = arg_str(args, index);
  if (tok == nullptr)
  {
    return false;
  }

  char *end = nullptr;
  const long v = std::strtol(tok, &end, 0);
  if (end == tok || *end != '\0')
  {
    error("error", "not_an_integer", "%s", tok);
    return false;
  }
  if (v < min || v > max)
  {
    error("error", "out_of_range", "%ld..%ld", min, max);
    return false;
  }

  *out = v;
  return true;
}

bool Interaction::arg_float(std::span<char *const> args, std::size_t index, float *out)
{
  const char *tok = arg_str(args, index);
  if (tok == nullptr)
  {
    return false;
  }

  char *end = nullptr;
  const float v = std::strtof(tok, &end);
  if (end == tok || *end != '\0')
  {
    error("error", "not_a_number", "%s", tok);
    return false;
  }

  *out = v;
  return true;
}

bool Interaction::arg_bool(std::span<char *const> args, std::size_t index, bool *out)
{
  const char *tok = arg_str(args, index);
  if (tok == nullptr)
  {
    return false;
  }

  if (iequal(tok, "on") || iequal(tok, "true") || iequal(tok, "yes") || iequal(tok, "1"))
  {
    *out = true;
    return true;
  }
  if (iequal(tok, "off") || iequal(tok, "false") || iequal(tok, "no") || iequal(tok, "0"))
  {
    *out = false;
    return true;
  }

  error("error", "not_a_boolean", "%s", tok);
  return false;
}

int Interaction::flush()
{
  if (!csv_ || line_len_ == 0)
  {
    return 0;
  }

  std::size_t len = line_len_;
  if (len > 0 && line_[len - 1] == '\r')
  {
    len--;
  }
  line_len_ = 0;

  char body[kRowBufferSize];
  const std::size_t n = escape_into(line_, len, body, sizeof(body));
  return emit_row("print", body, n);
}

int Interaction::print(const char *fmt, ...)
{
  char buf[kRowBufferSize];
  std::va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(buf))
  {
    n = static_cast<int>(sizeof(buf)) - 1;
  }

  if (!csv_)
  {
    return uart_write(console_->uart(), buf, static_cast<std::size_t>(n));
  }

  for (int i = 0; i < n; i++)
  {
    if (buf[i] == '\n')
    {
      flush();
      continue;
    }
    if (line_len_ + 1 >= sizeof(line_))
    {
      flush();
    }
    line_[line_len_++] = buf[i];
  }
  return n;
}

int Interaction::emit(const char *type, const char *fmt, ...)
{
  char body[kRowBufferSize];
  std::va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);

  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(body))
  {
    n = static_cast<int>(sizeof(body)) - 1;
  }

  if (!csv_)
  {
    return print("%s,%s\r\n", type, body);
  }

  flush();
  return emit_row(type, body, static_cast<std::size_t>(n));
}

int Interaction::println(const char *fmt, ...)
{
  char buf[kRowBufferSize];
  std::va_list ap;
  va_start(ap, fmt);
  const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0)
  {
    return n;
  }
  return print("%s\r\n", buf);
}

int Interaction::log(const char *level, const char *tag, const char *fmt, ...)
{
  char raw[kRowBufferSize / 2];
  std::va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(raw, sizeof(raw), fmt, ap);
  va_end(ap);

  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(raw))
  {
    n = static_cast<int>(sizeof(raw)) - 1;
  }

  if (!csv_)
  {
    return print("%s %s %s\r\n", level, (tag != nullptr) ? tag : "", raw);
  }

  char tag_field[48];
  char msg_field[kRowBufferSize / 2];
  escape_field((tag != nullptr) ? tag : "", tag_field, sizeof(tag_field));
  escape_field(raw, msg_field, sizeof(msg_field));

  flush();
  char body[kRowBufferSize];
  int b = std::snprintf(body, sizeof(body), "%s,%s,%s", (level != nullptr) ? level : "info", tag_field, msg_field);
  if (b < 0)
  {
    return b;
  }
  if (static_cast<std::size_t>(b) >= sizeof(body))
  {
    b = static_cast<int>(sizeof(body)) - 1;
  }
  return emit_row("log", body, static_cast<std::size_t>(b));
}

int Interaction::error(const char *level, const char *code)
{
  if (!csv_)
  {
    return print("%s: %s\r\n", level, code);
  }

  flush();
  char body[kRowBufferSize];
  const int n = std::snprintf(body, sizeof(body), "%s,%s", level, code);
  return (n < 0) ? n : emit_row("error", body, static_cast<std::size_t>(n));
}

int Interaction::error(const char *level, const char *code, const char *fmt, ...)
{
  char detail[kRowBufferSize / 2];
  std::va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(detail, sizeof(detail), fmt, ap);
  va_end(ap);

  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(detail))
  {
    n = static_cast<int>(sizeof(detail)) - 1;
  }

  if (!csv_)
  {
    return print("%s: %s (%s)\r\n", level, code, detail);
  }

  char escaped[kRowBufferSize / 2];
  escape_field(detail, escaped, sizeof(escaped));

  flush();
  char body[kRowBufferSize];
  int b = std::snprintf(body, sizeof(body), "%s,%s,%s", level, code, escaped);
  if (b < 0)
  {
    return b;
  }
  if (static_cast<std::size_t>(b) >= sizeof(body))
  {
    b = static_cast<int>(sizeof(body)) - 1;
  }
  return emit_row("error", body, static_cast<std::size_t>(b));
}

int Interaction::flags(const char *flags)
{
  if (!csv_ || flags == nullptr || flags[0] == '\0')
  {
    return 0;
  }

  char escaped[kRowBufferSize / 2];
  const std::size_t n = escape_field(flags, escaped, sizeof(escaped));

  flush();
  return emit_row("flags", escaped, n);
}

int Interaction::option(const Command &cmd, const char *path)
{
  const bool prefixed = (path != nullptr) && (path[0] != '\0');
  char command[96];

  if (prefixed && cmd.args != nullptr)
  {
    std::snprintf(command, sizeof(command), "%s %s %s", path, cmd.name, cmd.args);
  }
  else if (prefixed)
  {
    std::snprintf(command, sizeof(command), "%s %s", path, cmd.name);
  }
  else if (cmd.args != nullptr)
  {
    std::snprintf(command, sizeof(command), "%s %s", cmd.name, cmd.args);
  }
  else
  {
    std::snprintf(command, sizeof(command), "%s", cmd.name);
  }

  return option(cmd.name, command, cmd.description);
}

int Interaction::option(const char *label, const char *command, const char *description)
{
  if (!csv_)
  {
    char text[kRowBufferSize / 2];
    render_placeholders(command, text, sizeof(text));
    if (description != nullptr && description[0] != '\0')
    {
      return print("  %-35s - %s\r\n", text, description);
    }
    return print("  %s\r\n", text);
  }

  char l[kRowBufferSize / 4];
  char c[kRowBufferSize / 4];
  char d[kRowBufferSize / 2];
  escape_field(label, l, sizeof(l));
  escape_field(command, c, sizeof(c));
  escape_field((description != nullptr) ? description : "", d, sizeof(d));

  char body[kRowBufferSize];
  int n = std::snprintf(body, sizeof(body), "%s,%s,%s", l, c, d);
  if (n < 0)
  {
    return n;
  }
  if (static_cast<std::size_t>(n) >= sizeof(body))
  {
    n = static_cast<int>(sizeof(body)) - 1;
  }

  flush();
  return emit_row("option", body, static_cast<std::size_t>(n));
}

// MARK: Table

Table::Table(Interaction &io, std::span<const Column> cols, const char *title, bool show_header)
    : io_(io), cols_(cols), col_(0), row_len_(0)
{
  if (io_.csv_)
  {
    if (title != nullptr)
    {
      row_len_ = 0;
      append_raw("title", 5);
      append_cell_csv(title);
      io_.flush();
      io_.emit_row("table", row_, row_len_);
    }

    row_len_ = 0;
    append_raw("head", 4);
    for (const Column &c : cols_)
    {
      append_cell_csv(c.name);
    }
    io_.flush();
    io_.emit_row("table", row_, row_len_);
  }
  else
  {
    if (title != nullptr)
    {
      append_span_rule(kBoxTL, kBoxTR);
      append_title_row(title);
      append_rule(kBoxML, kBoxTM, kBoxMR);
    }
    else
    {
      append_rule(kBoxTL, kBoxTM, kBoxTR);
    }

    if (show_header)
    {
      row_len_ = 0;
      for (const Column &c : cols_)
      {
        append_text_cell(c.name, c.width);
      }
      append_str(kBoxV);
      io_.print("%s\r\n", row_);

      append_rule(kBoxML, kBoxMM, kBoxMR);
    }
  }
  start_row();
}

Table::~Table()
{
  if (col_ != 0)
  {
    end_row();
  }
  if (io_.csv_)
  {
    io_.emit_row("table", "end", 3);
  }
  else
  {
    append_rule(kBoxBL, kBoxBM, kBoxBR);
  }
}

void Table::start_row()
{
  col_ = 0;
  row_len_ = 0;
  if (io_.csv_)
  {
    append_raw("row", 3);
  }
}

void Table::append_raw(const char *s, std::size_t len)
{
  const std::size_t room = sizeof(row_) - 1 - row_len_;
  const std::size_t take = (len < room) ? len : room;
  std::memcpy(row_ + row_len_, s, take);
  row_len_ += take;
  row_[row_len_] = '\0';
}

void Table::append_cell_csv(const char *text)
{
  append_raw(",", 1);
  char esc[kRowBufferSize / 4];
  const std::size_t n = escape_field(text, esc, sizeof(esc));
  append_raw(esc, n);
}

void Table::append_str(const char *s)
{
  append_raw(s, std::strlen(s));
}

void Table::append_text_cell(const char *text, std::uint8_t width)
{
  char buf[kRowBufferSize / 4];
  const int w = static_cast<int>(width);
  const int n = std::snprintf(buf, sizeof(buf), "%s %-*.*s ", kBoxV, w, w, text);
  if (n > 0)
  {
    append_raw(buf, std::strlen(buf));
  }
}

std::size_t Table::inner_width() const
{
  std::size_t w = 0;
  for (const Column &c : cols_)
  {
    w += static_cast<std::size_t>(c.width) + 2;
  }
  return cols_.empty() ? 0 : w + cols_.size() - 1; // +1 per interior divider
}

void Table::append_span_rule(const char *left, const char *right)
{
  row_len_ = 0;
  append_str(left);
  for (std::size_t i = 0; i < inner_width(); i++)
  {
    append_str(kBoxH);
  }
  append_str(right);
  io_.print("%s\r\n", row_);
}

void Table::append_title_row(const char *title)
{
  const std::size_t field = (inner_width() >= 2) ? inner_width() - 2 : 0;

  row_len_ = 0;
  append_str(kBoxV);
  append_str(" ");
  std::size_t used = 0;
  for (const char *p = title; *p != '\0' && used < field; p++, used++)
  {
    append_raw(p, 1);
  }
  for (; used < field; used++)
  {
    append_str(" ");
  }
  append_str(" ");
  append_str(kBoxV);
  io_.print("%s\r\n", row_);
}

void Table::append_rule(const char *left, const char *mid, const char *right)
{
  row_len_ = 0;
  bool first = true;
  for (const Column &c : cols_)
  {
    append_str(first ? left : mid);
    for (int i = 0; i < static_cast<int>(c.width) + 2; i++)
    {
      append_str(kBoxH);
    }
    first = false;
  }
  append_str(right);
  io_.print("%s\r\n", row_);
}

void Table::cell(const char *fmt, ...)
{
  if (col_ >= cols_.size())
  {
    return;
  }

  char buf[kRowBufferSize / 4];
  std::va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (io_.csv_)
  {
    append_cell_csv(buf);
  }
  else
  {
    append_text_cell(buf, cols_[col_].width);
  }
  col_++;
}

void Table::end_row()
{
  while (col_ < cols_.size())
  {
    cell("%s", "");
  }

  if (io_.csv_)
  {
    io_.emit_row("table", row_, row_len_);
  }
  else
  {
    append_str(kBoxV);
    io_.print("%s\r\n", row_);
  }
  start_row();
}

// MARK: Console

const Command *Console::find(const char *name) const
{
  if (name == nullptr)
  {
    return nullptr;
  }
  for (const Command &c : commands_)
  {
    if (iequal(c.name, name))
    {
      return &c;
    }
  }
  return nullptr;
}

void Console::process_line(const char *line, std::size_t len) const
{
  if (line == nullptr || len == 0)
  {
    return;
  }

  char buf[kLineBufferSize];
  char *argv[kMaxArgs];

  if (len >= sizeof(buf))
  {
    len = sizeof(buf) - 1;
  }
  std::memcpy(buf, line, len);
  buf[len] = '\0';

  const int argc = parse_args(buf, argv, static_cast<int>(kMaxArgs));
  if (argc == 0)
  {
    return;
  }

  if (argc < 2 || !iequal(argv[1], "csv"))
  {
    Interaction io(*this, nullptr, false);
    const Command *cmd = find(argv[0]);
    if (cmd != nullptr)
    {
      run(io, *cmd, {argv, static_cast<std::size_t>(argc)}, nullptr);
    }
    else
    {
      io.print("Unknown command: %s\r\n", argv[0]);
      io.print("Type 'help' for available commands\r\n");
    }
    return;
  }

  if (!board_matches(argv[0]))
  {
    return;
  }

  if (argc < 4)
  {
    Interaction io(*this, "-", true);
    io.error("error", "malformed");
    return;
  }
  if (!tx_id_is_valid(argv[2]))
  {
    Interaction io(*this, "-", true);
    io.error("error", "invalid_tx_id");
    return;
  }

  Interaction io(*this, argv[2], true);
  io.emit_row("ack", nullptr, 0);

  const Command *cmd = find(argv[3]);
  if (cmd == nullptr)
  {
    io.error("error", "unknown_command", "%s", argv[3]);
  }
  else
  {
    run(io, *cmd, {argv + 3, static_cast<std::size_t>(argc - 3)}, nullptr);
  }

  io.flush();

  /* A detached handler keeps its transaction open. whoever holds the Handle
     emits the `done`. */
  if (!io.detached_)
  {
    io.emit_row("done", nullptr, 0);
  }
}

} // namespace feb::console
