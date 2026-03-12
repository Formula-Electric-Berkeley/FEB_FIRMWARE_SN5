/**
 ******************************************************************************
 * @file           : FEB_RES_EBS_Commands.c
 * @brief          : RES_EBS custom console commands
 ******************************************************************************
 */

#include "FEB_RES_EBS_Commands.h"

#include "FEB_CAN_PingPong.h"
#include "feb_console.h"

#include <ctype.h>
#include <stdlib.h>

static int strcasecmp_local(const char *a, const char *b)
{
  while (*a && *b)
  {
    int diff = tolower((unsigned char)*a) - tolower((unsigned char)*b);
    if (diff != 0)
    {
      return diff;
    }
    a++;
    b++;
  }

  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static void print_pingpong_help(void)
{
  FEB_Console_Printf("pingpong commands:\r\n");
  FEB_Console_Printf("  pingpong|ping|<1-4>      Start ping mode on channel\r\n");
  FEB_Console_Printf("  pingpong|pong|<1-4>      Start pong mode on channel\r\n");
  FEB_Console_Printf("  pingpong|stop|<1-4|all>  Stop channel or all channels\r\n");
  FEB_Console_Printf("  pingpong|status          Show ping/pong status\r\n");
  FEB_Console_Printf("Frame IDs: ch1=0x%02X ch2=0x%02X ch3=0x%02X ch4=0x%02X\r\n", FEB_PINGPONG_FRAME_ID_1,
                     FEB_PINGPONG_FRAME_ID_2, FEB_PINGPONG_FRAME_ID_3, FEB_PINGPONG_FRAME_ID_4);
}

static void cmd_ping(int argc, char *argv[])
{
  int channel;
  static const uint32_t frame_ids[FEB_PINGPONG_NUM_CHANNELS] = {
      FEB_PINGPONG_FRAME_ID_1,
      FEB_PINGPONG_FRAME_ID_2,
      FEB_PINGPONG_FRAME_ID_3,
      FEB_PINGPONG_FRAME_ID_4,
  };

  if (argc < 2)
  {
    FEB_Console_Printf("Usage: pingpong|ping|<channel>\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    FEB_Console_Printf("Error: channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_PING);
  if (FEB_CAN_PingPong_GetMode((uint8_t)channel) != PINGPONG_MODE_PING)
  {
    FEB_Console_Printf("Failed to start PING mode on channel %d\r\n", channel);
    return;
  }

  FEB_Console_Printf("Channel %d (0x%02lX): PING mode\r\n", channel, (unsigned long)frame_ids[channel - 1]);
}

static void cmd_pong(int argc, char *argv[])
{
  int channel;
  static const uint32_t frame_ids[FEB_PINGPONG_NUM_CHANNELS] = {
      FEB_PINGPONG_FRAME_ID_1,
      FEB_PINGPONG_FRAME_ID_2,
      FEB_PINGPONG_FRAME_ID_3,
      FEB_PINGPONG_FRAME_ID_4,
  };

  if (argc < 2)
  {
    FEB_Console_Printf("Usage: pingpong|pong|<channel>\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    FEB_Console_Printf("Error: channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_PONG);
  if (FEB_CAN_PingPong_GetMode((uint8_t)channel) != PINGPONG_MODE_PONG)
  {
    FEB_Console_Printf("Failed to start PONG mode on channel %d\r\n", channel);
    return;
  }

  FEB_Console_Printf("Channel %d (0x%02lX): PONG mode\r\n", channel, (unsigned long)frame_ids[channel - 1]);
}

static void cmd_stop(int argc, char *argv[])
{
  int channel;

  if (argc < 2)
  {
    FEB_Console_Printf("Usage: pingpong|stop|<channel|all>\r\n");
    return;
  }

  if (strcasecmp_local(argv[1], "all") == 0)
  {
    FEB_CAN_PingPong_Reset();
    FEB_Console_Printf("All ping/pong channels stopped\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    FEB_Console_Printf("Error: channel must be 1-4 or all\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_OFF);
  FEB_Console_Printf("Channel %d stopped\r\n", channel);
}

static void cmd_status(void)
{
  int channel;
  static const char *const mode_names[] = {"OFF", "PING", "PONG"};
  static const uint32_t frame_ids[FEB_PINGPONG_NUM_CHANNELS] = {
      FEB_PINGPONG_FRAME_ID_1,
      FEB_PINGPONG_FRAME_ID_2,
      FEB_PINGPONG_FRAME_ID_3,
      FEB_PINGPONG_FRAME_ID_4,
  };

  FEB_Console_Printf("CAN Ping/Pong Status:\r\n");
  FEB_Console_Printf("%-3s %-7s %-5s %10s %10s %12s\r\n", "Ch", "FrameID", "Mode", "TX Count", "RX Count",
                     "Last RX");
  FEB_Console_Printf("--- ------- ----- ---------- ---------- ------------\r\n");

  for (channel = 1; channel <= (int)FEB_PINGPONG_NUM_CHANNELS; channel++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)channel);
    const char *mode_name = ((unsigned int)mode < 3U) ? mode_names[mode] : "UNK";

    FEB_Console_Printf("%-3d 0x%03lX  %-5s %10lu %10lu %12ld\r\n", channel, (unsigned long)frame_ids[channel - 1],
                       mode_name, (unsigned long)FEB_CAN_PingPong_GetTxCount((uint8_t)channel),
                       (unsigned long)FEB_CAN_PingPong_GetRxCount((uint8_t)channel),
                       (long)FEB_CAN_PingPong_GetLastCounter((uint8_t)channel));
  }
}

static void cmd_pingpong(int argc, char *argv[])
{
  const char *subcmd;

  if (argc < 2)
  {
    print_pingpong_help();
    return;
  }

  subcmd = argv[1];

  if (strcasecmp_local(subcmd, "ping") == 0)
  {
    cmd_ping(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "pong") == 0)
  {
    cmd_pong(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "stop") == 0)
  {
    cmd_stop(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "status") == 0)
  {
    cmd_status();
  }
  else
  {
    FEB_Console_Printf("Unknown pingpong subcommand: %s\r\n", subcmd);
    print_pingpong_help();
  }
}

static const FEB_Console_Cmd_t pingpong_cmd = {
    .name = "pingpong",
    .help = "CAN ping/pong test: pingpong|ping|1, pingpong|pong|1, pingpong|status",
    .handler = cmd_pingpong,
};

int RES_EBS_RegisterCommands(void)
{
  return FEB_Console_Register(&pingpong_cmd);
}
