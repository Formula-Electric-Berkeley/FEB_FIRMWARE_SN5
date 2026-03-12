/**
 ******************************************************************************
 * @file           : FEB_RES_EBS_Commands.c
 * @brief          : RES_EBS custom console commands
 ******************************************************************************
 */

#include "FEB_RES_EBS_Commands.h"

#include "FEB_CAN_PingPong.h"
#include "FEB_RES_EBS_Board.h"
#include "feb_console.h"
#include "feb_uart.h"
#include "feb_uart_log.h"

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
  LOG_RAW("pingpong commands:\r\n");
  LOG_RAW("  pingpong|ping|<1-4>      Start ping mode on channel\r\n");
  LOG_RAW("  pingpong|pong|<1-4>      Start pong mode on channel\r\n");
  LOG_RAW("  pingpong|stop|<1-4|all>  Stop channel or all channels\r\n");
  LOG_RAW("  pingpong|status          Show ping/pong status\r\n");
  LOG_RAW("Frame IDs: ch1=0x%02X ch2=0x%02X ch3=0x%02X ch4=0x%02X\r\n", FEB_PINGPONG_FRAME_ID_1,
          FEB_PINGPONG_FRAME_ID_2, FEB_PINGPONG_FRAME_ID_3, FEB_PINGPONG_FRAME_ID_4);
}

static void print_tps_help(void)
{
  LOG_RAW("tps commands:\r\n");
  LOG_RAW("  tps|init                Initialize TPS2482 over I2C1\r\n");
  LOG_RAW("  tps|read                Read TPS voltages/current\r\n");
  LOG_RAW("  tps|status              Show TPS init state and GPIO pins\r\n");
}

static void print_relay_help(void)
{
  LOG_RAW("relay commands:\r\n");
  LOG_RAW("  relay|on                Drive %s high\r\n", RES_EBS_Relay_GetName());
  LOG_RAW("  relay|off               Drive %s low\r\n", RES_EBS_Relay_GetName());
  LOG_RAW("  relay|status            Show relay output and %s input\r\n", RES_EBS_TSActivation_GetName());
}

static const char *log_level_to_string(FEB_UART_LogLevel_t level)
{
  switch (level)
  {
  case FEB_UART_LOG_ERROR:
    return "error";
  case FEB_UART_LOG_WARN:
    return "warn";
  case FEB_UART_LOG_INFO:
    return "info";
  case FEB_UART_LOG_DEBUG:
    return "debug";
  case FEB_UART_LOG_TRACE:
    return "verbose";
  case FEB_UART_LOG_NONE:
  default:
    return "none";
  }
}

static void set_log_level_and_print(FEB_UART_LogLevel_t level)
{
  FEB_UART_SetLogLevel(FEB_UART_INSTANCE_1, level);
  LOG_RAW("Log level set to %s\r\n", log_level_to_string(level));
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
    LOG_RAW("Usage: pingpong|ping|<channel>\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    LOG_RAW("Error: channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_PING);
  if (FEB_CAN_PingPong_GetMode((uint8_t)channel) != PINGPONG_MODE_PING)
  {
    LOG_RAW("Failed to start PING mode on channel %d\r\n", channel);
    return;
  }

  LOG_RAW("Channel %d (0x%02lX): PING mode\r\n", channel, (unsigned long)frame_ids[channel - 1]);
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
    LOG_RAW("Usage: pingpong|pong|<channel>\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    LOG_RAW("Error: channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_PONG);
  if (FEB_CAN_PingPong_GetMode((uint8_t)channel) != PINGPONG_MODE_PONG)
  {
    LOG_RAW("Failed to start PONG mode on channel %d\r\n", channel);
    return;
  }

  LOG_RAW("Channel %d (0x%02lX): PONG mode\r\n", channel, (unsigned long)frame_ids[channel - 1]);
}

static void cmd_stop(int argc, char *argv[])
{
  int channel;

  if (argc < 2)
  {
    LOG_RAW("Usage: pingpong|stop|<channel|all>\r\n");
    return;
  }

  if (strcasecmp_local(argv[1], "all") == 0)
  {
    FEB_CAN_PingPong_Reset();
    LOG_RAW("All ping/pong channels stopped\r\n");
    return;
  }

  channel = atoi(argv[1]);
  if (channel < 1 || channel > (int)FEB_PINGPONG_NUM_CHANNELS)
  {
    LOG_RAW("Error: channel must be 1-4 or all\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)channel, PINGPONG_MODE_OFF);
  LOG_RAW("Channel %d stopped\r\n", channel);
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

  LOG_RAW("CAN Ping/Pong Status:\r\n");
  LOG_RAW("%-3s %-7s %-5s %10s %10s %12s\r\n", "Ch", "FrameID", "Mode", "TX Count", "RX Count", "Last RX");
  LOG_RAW("--- ------- ----- ---------- ---------- ------------\r\n");

  for (channel = 1; channel <= (int)FEB_PINGPONG_NUM_CHANNELS; channel++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)channel);
    const char *mode_name = ((unsigned int)mode < 3U) ? mode_names[mode] : "UNK";

    LOG_RAW("%-3d 0x%03lX  %-5s %10lu %10lu %12ld\r\n", channel, (unsigned long)frame_ids[channel - 1], mode_name,
            (unsigned long)FEB_CAN_PingPong_GetTxCount((uint8_t)channel),
            (unsigned long)FEB_CAN_PingPong_GetRxCount((uint8_t)channel),
            (long)FEB_CAN_PingPong_GetLastCounter((uint8_t)channel));
  }
}

static void cmd_tps_init(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  if (RES_EBS_TPS_Init())
  {
    LOG_RAW("TPS init OK\r\n");
  }
  else
  {
    LOG_RAW("TPS init FAILED\r\n");
  }
}

static void cmd_tps_read(int argc, char *argv[])
{
  RES_EBS_TPS_Status_t status;

  (void)argc;
  (void)argv;

  if (!RES_EBS_TPS_IsInitialized() && !RES_EBS_TPS_Init())
  {
    LOG_RAW("TPS not initialized; tps|init failed\r\n");
    return;
  }

  if (!RES_EBS_TPS_Read(&status))
  {
    LOG_RAW("TPS read FAILED\r\n");
    return;
  }

  LOG_RAW("TPS 0x%02X id=0x%04X bus=%.3fV shunt=%.3fmV current=%.3fA pg=%u alert=%u\r\n", status.i2c_address,
          status.device_id, (double)status.bus_voltage_v, (double)status.shunt_voltage_mv, (double)status.current_a,
          (unsigned int)(status.power_good == GPIO_PIN_SET), (unsigned int)(status.alert == GPIO_PIN_SET));
  LOG_RAW("TPS raw bus=0x%04X shunt=0x%04X current=0x%04X\r\n", status.bus_voltage_raw, status.shunt_voltage_raw,
          status.current_raw);
}

static void cmd_tps_status(int argc, char *argv[])
{
  RES_EBS_TPS_Status_t status;
  GPIO_PinState power_good = GPIO_PIN_RESET;
  GPIO_PinState alert = GPIO_PIN_RESET;

  (void)argc;
  (void)argv;

  if (RES_EBS_TPS_IsInitialized() && RES_EBS_TPS_Read(&status))
  {
    LOG_RAW("TPS init=%u addr=0x%02X id=0x%04X pg=%u alert=%u bus=%.3fV current=%.3fA\r\n",
            (unsigned int)status.initialized, status.i2c_address, status.device_id,
            (unsigned int)(status.power_good == GPIO_PIN_SET), (unsigned int)(status.alert == GPIO_PIN_SET),
            (double)status.bus_voltage_v, (double)status.current_a);
  }
  else
  {
    RES_EBS_TPS_GetPinStates(&power_good, &alert);
    LOG_RAW("TPS init=%u addr=0x%02X pg=%u alert=%u\r\n", (unsigned int)RES_EBS_TPS_IsInitialized(),
            (unsigned int)0x40U, (unsigned int)(power_good == GPIO_PIN_SET), (unsigned int)(alert == GPIO_PIN_SET));
  }
}

static void cmd_tps(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_tps_help();
    return;
  }

  if (strcasecmp_local(argv[1], "init") == 0)
  {
    cmd_tps_init(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(argv[1], "read") == 0)
  {
    cmd_tps_read(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(argv[1], "status") == 0)
  {
    cmd_tps_status(argc - 1, argv + 1);
  }
  else
  {
    LOG_RAW("Unknown tps subcommand: %s\r\n", argv[1]);
    print_tps_help();
  }
}

static void cmd_relay(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_relay_help();
    return;
  }

  if (strcasecmp_local(argv[1], "on") == 0)
  {
    RES_EBS_Relay_Set(true);
    LOG_RAW("Relay ON via %s\r\n", RES_EBS_Relay_GetName());
  }
  else if (strcasecmp_local(argv[1], "off") == 0)
  {
    RES_EBS_Relay_Set(false);
    LOG_RAW("Relay OFF via %s\r\n", RES_EBS_Relay_GetName());
  }
  else if (strcasecmp_local(argv[1], "status") == 0)
  {
    LOG_RAW("Relay %s via %s, %s=%u\r\n", RES_EBS_Relay_Get() ? "ON" : "OFF", RES_EBS_Relay_GetName(),
            RES_EBS_TSActivation_GetName(), (unsigned int)RES_EBS_TSActivation_Get());
  }
  else
  {
    LOG_RAW("Unknown relay subcommand: %s\r\n", argv[1]);
    print_relay_help();
  }
}

static void cmd_loge(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  set_log_level_and_print(FEB_UART_LOG_ERROR);
}

static void cmd_logw(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  set_log_level_and_print(FEB_UART_LOG_WARN);
}

static void cmd_logi(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  set_log_level_and_print(FEB_UART_LOG_INFO);
}

static void cmd_logd(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  set_log_level_and_print(FEB_UART_LOG_DEBUG);
}

static void cmd_logv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  set_log_level_and_print(FEB_UART_LOG_TRACE);
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
    LOG_RAW("Unknown pingpong subcommand: %s\r\n", subcmd);
    print_pingpong_help();
  }
}

static const FEB_Console_Cmd_t pingpong_cmd = {
    .name = "pingpong",
    .help = "CAN ping/pong test: pingpong|ping|1, pingpong|pong|1, pingpong|status",
    .handler = cmd_pingpong,
};

static const FEB_Console_Cmd_t tps_cmd = {
    .name = "tps",
    .help = "TPS2482 console: tps|init, tps|read, tps|status",
    .handler = cmd_tps,
};

static const FEB_Console_Cmd_t relay_cmd = {
    .name = "relay",
    .help = "Relay control on Driverless_System_Relay: relay|on, relay|off, relay|status",
    .handler = cmd_relay,
};

static const FEB_Console_Cmd_t loge_cmd = {
    .name = "loge",
    .help = "Set UART log level to error",
    .handler = cmd_loge,
};

static const FEB_Console_Cmd_t logw_cmd = {
    .name = "logw",
    .help = "Set UART log level to warn",
    .handler = cmd_logw,
};

static const FEB_Console_Cmd_t logi_cmd = {
    .name = "logi",
    .help = "Set UART log level to info",
    .handler = cmd_logi,
};

static const FEB_Console_Cmd_t logd_cmd = {
    .name = "logd",
    .help = "Set UART log level to debug",
    .handler = cmd_logd,
};

static const FEB_Console_Cmd_t logv_cmd = {
    .name = "logv",
    .help = "Set UART log level to verbose/trace",
    .handler = cmd_logv,
};

int RES_EBS_RegisterCommands(void)
{
  if (FEB_Console_Register(&pingpong_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&tps_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&relay_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&loge_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&logw_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&logi_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&logd_cmd) != 0)
  {
    return -1;
  }
  if (FEB_Console_Register(&logv_cmd) != 0)
  {
    return -1;
  }

  return 0;
}
