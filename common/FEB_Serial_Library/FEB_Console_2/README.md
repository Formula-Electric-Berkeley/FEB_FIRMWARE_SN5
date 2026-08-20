# feb_console_2

The serial console for FEB boards. One command supplies both a person at a
terminal and the CSV protocol of the website.

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_console_2)
```

## Quick start

```cpp
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"

using namespace feb::console;

namespace
{
void cmd_lvpdb_rails(Interaction &io, std::span<char *const>)
{
  static constexpr Column kCols[] = {{"Rail", 6}, {"Volts", 8}};
  Table t(io, kCols, "LV Rails");
  t.cell("5V");
  t.cell("%.2f", read_5v());
  t.end_row();
}

constexpr std::array<Command, 1> kLvpdbSubcommands = {{
    {.name = "rails", .description = "Rail voltages", .handler = cmd_lvpdb_rails},
}};

constexpr std::array<Command, 1> kBoardCommands = {{
    group("lvpdb", "LVPDB board commands", kLvpdbSubcommands),
}};
} // namespace

inline constexpr auto kAll = concat(kSystemCommands, kBoardCommands);
static_assert(!has_duplicate_names(kAll));

inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};

extern "C" void LVPDB_Console_ProcessLine(const char *line, size_t len)
{
  kConsole.process_line(line, len);
}
```

Then send each received line to the console:

```c
if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, buf, sizeof(buf), &len, 10))
{
  LVPDB_Console_ProcessLine(buf, len);
}
```

You do not initialize or register anything. But you must call `FEB_Time_Init()`
first, because each CSV row contains a time in microseconds.

## The file of a board

One pair of files holds the commands of a board:

```
<Board>/Core/User/Src/<Board>_Commands.cpp   the handlers and the tree
<Board>/Core/User/Inc/<Board>_Commands.h     <Board>_Console_ProcessLine
```

The header is C, because the task that receives the lines is C. It declares
that one function. Everything else is in an anonymous namespace, thus no
handler reaches the linker.

The file reads from the leaves to the root, because a `Command` must name a
handler that exists already: the handlers of a level, then the table of that
level, then the level above it, and `kAll` and the `Console` at the end.

A board with a large command puts it in its own file. The tables stay in
`<Board>_Commands.cpp`, and the other file gives it the handlers.

## The names

A handler is `cmd_` and the path of its command. Each space and each hyphen
becomes `_`:

| Command | Handler |
|---|---|
| `bms status` | `cmd_bms_status` |
| `bms cell-stats` | `cmd_bms_cell_stats` |
| `dcu radio config freq` | `cmd_dcu_radio_config_freq` |

Thus the name of a function gives its command, and a command gives the function
that answers it. Rename the command, rename the handler.

`cmd_` means that the function is a `Command::Handler`. A function that serves
more than one handler does not take the prefix, for example
`list_state_options()` or `set_pingpong_mode()`.

A table of children is `k<Path>Subcommands` with the same path in CamelCase:
`kRadioConfigSubcommands` for `dcu radio config`. The table of the board is
`k<Board>Commands`, and `kAll` is that table with `kSystemCommands`.

## The two transports

```
text:  lvpdb rails
csv:   LVPDB csv <tx_id> lvpdb rails
```

A space and a `|` are both separators. Many separators together operate as one
separator. Double quotation marks group a token that contains separators, and
the console removes the quotation marks. Thus `tx "a b"` gives one argument, and
`tx a b` gives two arguments.

The console answers a CSV request in this sequence:

```
csv,<tx_id>,<board>,<us>,ack
csv,<tx_id>,<board>,<us>,<type>[,<body>]     ← zero or more rows
csv,<tx_id>,<board>,<us>,done
```

The console sends `ack` and `done` automatically. A `*` in the board field is a
broadcast to all boards. If the board name is different, the console ignores the
line and sends no answer.

A handler writes text for a person. The transport then selects the format. In
CSV mode the console sends each completed line as a `print` row. Thus one
handler is sufficient for both transports.

## The response types

| Type | Body | Sent by |
|---|---|---|
| `ack` / `done` | none | the dispatcher |
| `print` | `<text>` | `io.print()`, `io.println()` |
| `option` | `<label>,<command>,<description>` | `io.option()`, the command lists |
| `flags` | `<tokens>` | `io.flags()` |
| `table` | `title,<t>` / `head,<cols>` / `row,<cells>` / `end` | `Table` |
| `kv` | the same four forms, always two columns | `KVTable` |
| `log` | `<level>,<tag>,<message>` | `io.log()`, `log capture` |
| `error` | `<level>,<code>[,<detail>]` | `io.error()`, the `arg_*` functions |
| other | set by the caller | `io.emit()` |

### The escape rules

Escape the backslash first. If you do not do this, you cannot reverse the
operation.

```
\ → \\      CR → \r      LF → \n      , → \,
```

The console escapes all data that it composes: `print`, `println`, `log`,
`option`, `error`, and the cells of a `Table`. Thus the host can divide each row
at the commas that have no escape.

`emit()` is the one exception, because the caller composes its body. A comma in
the format string is a field separator. Use `escape_field()` on free text before
you put it in an `emit()` body:

```cpp
char desc[64];
escape_field(cmd.description, desc, sizeof(desc));
io.emit("command", "%s,%s", cmd.name, desc);
```

## How to build a command tree

`Command` is one type at each level. Thus `group()` operates from the top of the
console to the bottom, and no level needs a function that only forwards.

```cpp
constexpr std::array<Command, 2> kStreamSubcommands = {{
    {.name = "on",  .description = "Start the stream", .handler = cmd_stream_on},
    {.name = "off", .description = "Stop the stream",  .handler = cmd_stream_off},
}};

constexpr std::array<Command, 1> kCanSubcommands = {{
    {.name = "stream", .description = "Live CAN stream", .handler = cmd_stream_status,
     .subs = kStreamSubcommands},
}};
```

A command can have a handler and children at the same time. If `args[1]` is the
name of a child, the child operates. If not, the handler operates and the
console then lists the children. Thus `can stream` can report the status, and
`can stream on` can change the state. You do not have to give the two commands
different names.

| Field | Function |
|---|---|
| `name` | The console compares this name without case. |
| `description` | The console shows this text in the lists and in `option` rows. |
| `handler` | `void(Interaction&, std::span<char *const>)`. The args start at the name of the command. |
| `params` | The table of arguments. The dispatcher parses them before the handler. |
| `subs` | The table of children. |
| `text_hidden` | Do not show this command in the text lists. |
| `csv_hidden` | Do not show this command in the CSV lists. |

`static_assert(!has_duplicate_names(kAll))` examines the children at each level,
and `static_assert(params_fit(kAll))` examines the count of the params.

`text_hidden` and `csv_hidden` remove a command from the lists in that mode.
This includes `help` and `commands` at the top, and the list of children below.
A command that you hide is not disabled. The console obeys the command if the
host sends the name.

NOTE: The `args` tokens are temporary. They point into a buffer on the stack of
the dispatcher, and the next line replaces that buffer. Copy each value that
must stay valid after the handler returns.

`io.flags()` tells the host something about the response that the tables do not
give it, for example `io.flags("slow")`:

```
csv,t1,BMS,…,ack
csv,t1,BMS,…,flags,slow
csv,t1,BMS,…,print,TX count: 42
csv,t1,BMS,…,done
```

Flag rows are cumulative. The host must collect all the tokens that it receives
for one transaction.

### The arguments

A command declares its arguments one time, as data. The table goes above the
handler that reads it, and the name is `k` and the path of the command:

```cpp
constexpr std::array kBmsCellParams = {param_int("bank", 1, FEB_NBANKS),
                                       param_int("cell", 1, FEB_NUM_CELLS_PER_BANK)};

void cmd_bms_cell(Interaction &io, std::span<char *const>)
{
  const long bank = io.param_int(0);
  const long cell = io.param_int(1);
  /* both are present and inside the range */
}
```

```cpp
{.name = "cell", .description = "One cell in detail", .handler = cmd_bms_cell, .params = kBmsCellParams},
```

The dispatcher parses each param before it calls the handler. If a value is
absent or not correct, it sends the `error` row and the handler does not
operate. Thus a handler examines no arguments.

| Declaration | The handler reads | Error code if the value is not correct |
|---|---|---|
| `param_int(name, min, max)` | `io.param_int(i)` | `missing_argument`, `not_an_integer`, `out_of_range` |
| `param_float(name)` | `io.param_float(i)` | `missing_argument`, `not_a_number` |
| `param_bool(name)` | `io.param_bool(i)` | `missing_argument`, `not_a_boolean` |
| `param_str(name)` | `io.param_str(i)` | `missing_argument` |

`i` is the number of the param. The first one is `0`. A param that the command
does not declare, or a type that it does not have, gives `0` or `nullptr`.

`param_int` uses base 0. Thus `0x0A` is correct. The examination is strict, and
it refuses `9x`. It does not read `9` from that text.

The console builds the text of the option from the same table. Thus
`bms cell $bank:int:1:10 $cell:int:1:18` goes to the website, which then knows
the range of each field, and a person sees `bms cell <bank> <cell>`.

A command has `kMaxParams` params at the most.

NOTE: The tokens are temporary. They point into a buffer on the stack of the
dispatcher, and the next line replaces that buffer. This includes the value of
`param_str()`. Copy each value that must stay valid after the handler returns.

## Interaction

| Function | Text mode | CSV mode |
|---|---|---|
| `print(fmt, …)` | Writes to the UART. | Sends a `print` row for each completed line. |
| `println(fmt, …)` | The same, and adds CRLF. | The same. |
| `option(…)` | Shows `  cmd  - description`. | Sends an `option` row. |
| `flags(tokens)` | Does nothing. | Sends a `flags` row. |
| `log(level, tag, fmt, …)` | Shows `level tag message`. | Sends a `log` row. |
| `error(level, code[, fmt, …])` | Shows `level: code (detail)`. | Sends an `error` row. |
| `emit(type, fmt, …)` | Shows `type,body`. | Sends a row of that type. |
| `path()` | Gives the command path. | The same. |
| `is_csv()` | Gives `false`. | Gives `true`. |
| `flush()` | Does nothing. | Sends the incomplete line. |

Use `path()`. Do not write a command string in the handler, because the two can
become different.

Only a handler that does not return must call `flush()`. The `reboot` command
calls it before the reset.

### Tables

```cpp
static constexpr Column kCols[] = {{"Cell", 8}, {"Volts", 8}};
Table t(io, kCols, "Cell Voltages");
t.cell("B%u C%02u", bank, cell);
t.cell("%.3f", (double)volts);
t.end_row();
```

A table sends each row when the row is complete. Thus a table of 40 rows uses
the same quantity of RAM as a table of 2 rows. This is the reason that you must
give the widths first. The console cannot measure a column that it did not
receive.

The `width` value changes the text mode only. A CSV row always contains the full
value. In text mode the console cuts a value that is too long, because a table
that is not in alignment is more difficult to read. The destructor closes the
table. Thus a handler that returns early cannot leave a table open.

### KVTable

Most commands report a set of pairs, not a grid. `KVTable` is that table: the
columns are `Field` and `Value`, you give only the two widths, and the header is
hidden.

```cpp
KVTable t(io, 8, 58, "FEB Firmware");
t.row("Board", "%s", board);
t.row("Version", "%s (%s)", version, commit);
```

`row()` is one `cell()` for each column and an `end_row()`. The rows carry the
type `kv`, not `table`. Thus the host shows a set of pairs from the type alone
and does not examine the header. Everything else operates as `Table` does, and
`cell()` and `end_row()` remain available for a row that `row()` cannot compose.

### Transactions that stay open

`io.detach()` keeps a transaction open after the handler returns, and stops the
automatic `done`. The `Handle` contains the tx_id and the
console. Thus the rows go to the UART that received the request.

`detach()` operates in CSV mode only. Text mode has no transaction to keep open.
In text mode `detach()` gives an invalid handle and changes nothing. Examine
`is_csv()` first if the command must tell the user:

```cpp
if (!io.is_csv())
{
  io.error("error", "csv_only", "%s needs a csv transaction", io.path());
  return;
}
```

```cpp
Interaction::Handle s_stream;

void stream_stop()
{
  if (s_stream.valid())
  {
    s_stream.emit("done");
    s_stream = Interaction::Handle{};
  }
}

void cmd_stream_on(Interaction &io, std::span<char *const>)
{
  stream_stop();               // a second `on` must close the first
  s_stream = io.detach();      // stops the `done` of this transaction
}

void cmd_stream_off(Interaction &io, std::span<char *const>)
{
  stream_stop();
  io.println("stream: off");
}

/* from the task that makes the data */
void on_frame(const char *row)
{
  if (s_stream.valid())
  {
    s_stream.emit("can", "%s", row);
  }
}
```

## The limits

| Limit | Value | Result if the data is larger |
|---|---|---|
| `kRowBufferSize` | 320 | The console cuts the row body at approximately 280 bytes. |
| `kLineBufferSize` | 192 | The console cuts the request line. |
| `kMaxArgs` | 32 | The console discards the subsequent tokens. Use quotation marks to keep a phrase together. |
| `kTxIdMaxLen` | 32 | The console refuses the id with `invalid_tx_id`. |
| `Table` cell, `KVTable` value | 80 bytes | The console cuts the cells of a column that is wider than approximately 76. |
| The depth of the tree | 160 bytes of stack for each level | none |

If one `print` line is longer than the row buffer, the console sends more than
one `print` row. The host cannot identify the difference between this condition
and true line ends.