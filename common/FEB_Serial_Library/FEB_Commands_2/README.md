# feb_commands_2

The default system commands for [feb_console_2](../FEB_Console_2/README.md).
Each board has the same eight commands. Thus the website can be sure that they
are available.

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_commands_2)
```

```cpp
#include "feb_commands_2.hpp"

inline constexpr auto kAll = concat(kSystemCommands, kBoardCommands);
static_assert(!has_duplicate_names(kAll));
inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};
```

`kSystemCommands` is `inline constexpr`. Thus `concat()` puts it into the table
of the board at compile time. The board registers nothing at run time.

## The commands

| Command | In `help` | In `commands` | Notes |
|---|:-:|:-:|---|
| `echo <text>` | ✓ | | for a person |
| `help [command]` | ✓ | | the text list of commands |
| `commands` | | | the list for the website |
| `hello` | ✓ | ✓ | board discovery. Answers a `*` broadcast. |
| `version` | ✓ | ✓ | a table of the version, the commit, and the build and flash data |
| `uptime` | ✓ | ✓ | the milliseconds after the start |
| `reboot` | ✓ | ✓ | a software reset |
| `log [level]` | ✓ | ✓ | shows the level. Each level is a subcommand. |

`help` and `commands` answer the same question for different users. `help`
removes the commands that have `text_hidden` and shows a title. `commands`
removes the commands that have `csv_hidden`. Both use `io.option()`. Thus the
commands of a board are in the lists automatically.

The two commands do not list the subcommands. They list the top level only. The
host then sends a command to receive its children as `option` rows.

The log levels are a fixed set. Thus each level is a subcommand: `log none`,
`log error`, `log warn`, `log info`, `log debug`, and `log trace`. The `log`
command with no level shows the current level.

`log capture` is not in the two lists. It is a function for a machine, and a
host that needs it knows the name.

## Discovery

The website sends `commands` one time. It then sends one request for each level
that the user opens:

```
-> DCU_Receiver|csv|c001|commands
<- csv,c001,DCU_Receiver,…,option,dcu,dcu,DCU_Receiver board commands
<- csv,c001,DCU_Receiver,…,option,version,version,Firmware version…

-> DCU_Receiver|csv|c002|dcu
<- csv,c002,DCU_Receiver,…,option,radio,dcu radio,Radio status and config
<- csv,c002,DCU_Receiver,…,option,can,dcu can,CAN state and live stream
```

## log capture

`log capture` sends the output of `LOG_I`, `LOG_D`, and the other log functions
into the CSV data as `log` rows. The console does not show the output as text.
The command is not in the lists, but the host can send it:

```
-> DCU_Receiver|csv|cap1|log capture
<- csv,cap1,DCU_Receiver,…,ack
<- csv,cap1,DCU_Receiver,…,log,info,RADIO,packet rx\, rssi=-71
<- csv,cap1,DCU_Receiver,…,log,error,RFM95,crc fail\, dropped (rfm95.c:412)

-> DCU_Receiver|csv|cap2|log capture off
<- csv,cap2,DCU_Receiver,…,ack
<- csv,cap1,DCU_Receiver,…,done       ← this closes the capture transaction
<- csv,cap2,DCU_Receiver,…,done
```

The command keeps its transaction open with `io.detach()`. It then installs a
tap in `FEB_Log`.

The command operates in CSV mode only, because text mode has no transaction to
keep open. A new capture closes the previous capture first.
