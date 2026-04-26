/**
 ******************************************************************************
 * @file           : user_diskio_spi.c
 * @brief          : SPI SD card disk I/O for FatFs
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Adapted from the reference implementation at:
 * https://github.com/kiwih/cubeide-sd-card
 *
 * This file provides the low-level SPI transactions required by FatFs.
 ******************************************************************************
 */

#include "user_diskio_spi.h"
#include "main.h"
#include "spi.h"
#include "feb_log.h"
#include "cmsis_os.h"
#include <stdbool.h>

#define TAG_SD_SPI "[SD_SPI]"

extern SPI_HandleTypeDef SD_SPI_HANDLE;

#define SPI_TIMEOUT_MS 100U

static void set_spi_prescaler(uint32_t prescaler)
{
  SD_SPI_HANDLE.Init.BaudRatePrescaler = prescaler;
  (void)HAL_SPI_Init(&SD_SPI_HANDLE);
}

/* SD spec requires 100–400 kHz for CMD0/CMD8/ACMD41; switch to full speed once initialized. */
#define FCLK_SLOW() set_spi_prescaler(SPI_BAUDRATEPRESCALER_256)
#define FCLK_FAST() set_spi_prescaler(SPI_BAUDRATEPRESCALER_32)

#define CS_HIGH() HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)
#define CS_LOW() HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)

#define CMD0 (0U)
#define CMD1 (1U)
#define ACMD41 (0x80U + 41U)
#define CMD8 (8U)
#define CMD9 (9U)
#define CMD12 (12U)
#define ACMD13 (0x80U + 13U)
#define CMD16 (16U)
#define CMD17 (17U)
#define CMD18 (18U)
#define CMD24 (24U)
#define CMD25 (25U)
#define CMD55 (55U)
#define CMD58 (58U)
#define ACMD23 (0x80U + 23U)

#define CT_MMC 0x01U
#define CT_SD1 0x02U
#define CT_SD2 0x04U
#define CT_SDC (CT_SD1 | CT_SD2)
#define CT_BLOCK 0x08U

static volatile DSTATUS sd_status_flags = STA_NOINIT;
static BYTE card_type = 0U;
static uint32_t spi_timer_start = 0U;
static uint32_t spi_timer_delay = 0U;
static bool s_hal_err_logged = false;

static void spi_timer_on(uint32_t wait_ms)
{
  spi_timer_start = HAL_GetTick();
  spi_timer_delay = wait_ms;
}

static bool spi_timer_active(void)
{
  return (HAL_GetTick() - spi_timer_start) < spi_timer_delay;
}

static BYTE xchg_spi(BYTE data)
{
  BYTE rx_data = 0xFFU;
  HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &rx_data, 1U, SPI_TIMEOUT_MS);
  if (status != HAL_OK && !s_hal_err_logged)
  {
    s_hal_err_logged = true;
    LOG_E(TAG_SD_SPI, "xchg_spi HAL err=%d state=0x%02X errcode=0x%08lX",
          (int)status, (unsigned)SD_SPI_HANDLE.State, (unsigned long)SD_SPI_HANDLE.ErrorCode);
  }
  return rx_data;
}

static void rcvr_spi_multi(BYTE *buff, UINT count)
{
  for (UINT i = 0; i < count; i++)
  {
    buff[i] = xchg_spi(0xFFU);
  }
}

#if _USE_WRITE == 1
static int xmit_spi_multi(const BYTE *buff, UINT count)
{
  return HAL_SPI_Transmit(&SD_SPI_HANDLE, (uint8_t *)buff, count, SPI_TIMEOUT_MS) == HAL_OK;
}
#endif

static int wait_ready(UINT wait_ms)
{
  BYTE response = 0U;
  uint32_t start = HAL_GetTick();

  do
  {
    response = xchg_spi(0xFFU);
  } while (response != 0xFFU && (HAL_GetTick() - start) < (uint32_t)wait_ms);

  return response == 0xFFU;
}

static void deselect_card(void)
{
  CS_HIGH();
  (void)xchg_spi(0xFFU);
}

static int select_card(void)
{
  CS_LOW();
  (void)xchg_spi(0xFFU);
  if (wait_ready(500U))
  {
    return 1;
  }

  deselect_card();
  return 0;
}

static int rcvr_datablock(BYTE *buff, UINT count)
{
  BYTE token = 0xFFU;

  spi_timer_on(200U);
  do
  {
    token = xchg_spi(0xFFU);
  } while (token == 0xFFU && spi_timer_active());

  if (token != 0xFEU)
  {
    return 0;
  }

  rcvr_spi_multi(buff, count);
  (void)xchg_spi(0xFFU);
  (void)xchg_spi(0xFFU);

  return 1;
}

#if _USE_WRITE == 1
static int xmit_datablock(const BYTE *buff, BYTE token)
{
  BYTE response = 0U;

  if (!wait_ready(500U))
  {
    return 0;
  }

  (void)xchg_spi(token);
  if (token != 0xFDU)
  {
    if (!xmit_spi_multi(buff, 512U))
    {
      return 0;
    }
    (void)xchg_spi(0xFFU);
    (void)xchg_spi(0xFFU);

    response = xchg_spi(0xFFU);
    if ((response & 0x1FU) != 0x05U)
    {
      return 0;
    }
  }

  return 1;
}
#endif

/* SN4-style power-on CMD0: raw frame + deep response poll. Some cards take
 * significantly longer to emit the first R1 while transitioning SD->SPI mode,
 * so the 10-try poll in send_cmd() is not sufficient for the very first CMD0. */
static BYTE power_on_cmd0(void)
{
  CS_HIGH();
  osDelay(1U);
  for (BYTE i = 0; i < 10U; i++)
  {
    (void)xchg_spi(0xFFU);
  }

  CS_LOW();
  osDelay(1U);

  (void)xchg_spi((BYTE)(0x40U | CMD0));
  (void)xchg_spi(0x00U);
  (void)xchg_spi(0x00U);
  (void)xchg_spi(0x00U);
  (void)xchg_spi(0x00U);
  (void)xchg_spi(0x95U);

  BYTE response = 0xFFU;
  uint32_t tries = 0x1FFFU;
  do
  {
    response = xchg_spi(0xFFU);
  } while ((response & 0x80U) != 0U && --tries != 0U);

  CS_HIGH();
  (void)xchg_spi(0xFFU);

  return response;
}

static BYTE send_cmd(BYTE cmd, DWORD arg)
{
  BYTE crc = 0x01U;
  BYTE response = 0xFFU;
  BYTE tries = 10U;

  if ((cmd & 0x80U) != 0U)
  {
    cmd &= 0x7FU;
    response = send_cmd(CMD55, 0U);
    if (response > 1U)
    {
      return response;
    }
  }

  if (cmd != CMD12)
  {
    deselect_card();
    if (!select_card())
    {
      return 0xFFU;
    }
  }

  (void)xchg_spi((BYTE)(0x40U | cmd));
  (void)xchg_spi((BYTE)(arg >> 24));
  (void)xchg_spi((BYTE)(arg >> 16));
  (void)xchg_spi((BYTE)(arg >> 8));
  (void)xchg_spi((BYTE)arg);

  if (cmd == CMD0)
  {
    crc = 0x95U;
  }
  if (cmd == CMD8)
  {
    crc = 0x87U;
  }
  (void)xchg_spi(crc);

  if (cmd == CMD12)
  {
    (void)xchg_spi(0xFFU);
  }

  do
  {
    response = xchg_spi(0xFFU);
  } while ((response & 0x80U) != 0U && --tries != 0U);

  return response;
}

DSTATUS USER_SPI_initialize(BYTE pdrv)
{
  BYTE ocr[4] = {0};
  BYTE ty = 0U;

  if (pdrv != 0U)
  {
    return STA_NOINIT;
  }

  if ((sd_status_flags & STA_NODISK) != 0U)
  {
    LOG_W(TAG_SD_SPI, "init: STA_NODISK set, flags=0x%02X", (unsigned)sd_status_flags);
    return sd_status_flags;
  }

  uint32_t t_start = HAL_GetTick();
  LOG_I(TAG_SD_SPI, "init: pdrv=%u flags=0x%02X", (unsigned)pdrv, (unsigned)sd_status_flags);

  s_hal_err_logged = false;
  FCLK_SLOW();

  BYTE r0 = 0xFFU;
  for (BYTE attempt = 0; attempt < 3U; attempt++)
  {
    r0 = power_on_cmd0();
    LOG_D(TAG_SD_SPI, "CMD0 -> 0x%02X (expect 0x01) attempt=%u", (unsigned)r0, (unsigned)attempt);
    if (r0 == 1U)
    {
      break;
    }
    osDelay(pdMS_TO_TICKS(10));
  }
  if (r0 == 1U)
  {
    spi_timer_on(1000U);
    BYTE r8 = send_cmd(CMD8, 0x1AAU);
    LOG_D(TAG_SD_SPI, "CMD8 -> 0x%02X", (unsigned)r8);
    if (r8 == 1U)
    {
      for (BYTE i = 0; i < 4U; i++)
      {
        ocr[i] = xchg_spi(0xFFU);
      }
      LOG_D(TAG_SD_SPI, "CMD8 OCR: %02X %02X %02X %02X",
            (unsigned)ocr[0], (unsigned)ocr[1], (unsigned)ocr[2], (unsigned)ocr[3]);

      if (ocr[2] == 0x01U && ocr[3] == 0xAAU)
      {
        uint32_t acmd41_start = HAL_GetTick();
        uint32_t iters = 0U;
        BYTE r41 = 0xFFU;
        while (spi_timer_active() && (r41 = send_cmd(ACMD41, 1UL << 30)) != 0U)
        {
          iters++;
        }
        LOG_D(TAG_SD_SPI, "ACMD41 done: iters=%lu elapsed=%lu ms last=0x%02X timer_active=%d",
              (unsigned long)iters,
              (unsigned long)(HAL_GetTick() - acmd41_start),
              (unsigned)r41,
              (int)spi_timer_active());

        if (spi_timer_active())
        {
          BYTE r58 = send_cmd(CMD58, 0U);
          if (r58 == 0U)
          {
            for (BYTE i = 0; i < 4U; i++)
            {
              ocr[i] = xchg_spi(0xFFU);
            }
            ty = ((ocr[0] & 0x40U) != 0U) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
            LOG_D(TAG_SD_SPI, "CMD58 -> 0x00 OCR=%02X%02X%02X%02X ty=0x%02X",
                  (unsigned)ocr[0], (unsigned)ocr[1], (unsigned)ocr[2], (unsigned)ocr[3],
                  (unsigned)ty);
          }
          else
          {
            LOG_D(TAG_SD_SPI, "CMD58 -> 0x%02X (nonzero, ty stays 0)", (unsigned)r58);
          }
        }
      }
      else
      {
        LOG_D(TAG_SD_SPI, "CMD8 OCR mismatch (expect ..01 AA), ty stays 0");
      }
    }
    else
    {
      BYTE init_cmd = CMD1;
      BYTE r41_probe = send_cmd(ACMD41, 0U);
      if (r41_probe <= 1U)
      {
        ty = CT_SD1;
        init_cmd = ACMD41;
      }
      else
      {
        ty = CT_MMC;
      }
      LOG_D(TAG_SD_SPI, "CMD8 rejected, legacy init: probe=0x%02X init_cmd=0x%02X ty=0x%02X",
            (unsigned)r41_probe, (unsigned)init_cmd, (unsigned)ty);

      uint32_t legacy_start = HAL_GetTick();
      uint32_t iters = 0U;
      while (spi_timer_active() && send_cmd(init_cmd, 0U) != 0U)
      {
        iters++;
      }

      if (!spi_timer_active() || send_cmd(CMD16, 512U) != 0U)
      {
        ty = 0U;
      }
      LOG_D(TAG_SD_SPI, "legacy init end: iters=%lu elapsed=%lu ms ty=0x%02X timer_active=%d",
            (unsigned long)iters,
            (unsigned long)(HAL_GetTick() - legacy_start),
            (unsigned)ty,
            (int)spi_timer_active());
    }
  }

  card_type = ty;
  deselect_card();

  if (ty != 0U)
  {
    FCLK_FAST();
    sd_status_flags &= (BYTE)~STA_NOINIT;
    LOG_I(TAG_SD_SPI, "init OK: card_type=0x%02X elapsed=%lu ms flags=0x%02X",
          (unsigned)ty, (unsigned long)(HAL_GetTick() - t_start), (unsigned)sd_status_flags);
  }
  else
  {
    sd_status_flags = STA_NOINIT;
    LOG_E(TAG_SD_SPI, "init FAIL: ty=0 elapsed=%lu ms flags=0x%02X",
          (unsigned long)(HAL_GetTick() - t_start), (unsigned)sd_status_flags);
  }

  return sd_status_flags;
}

DSTATUS USER_SPI_status(BYTE pdrv)
{
  if (pdrv != 0U)
  {
    return STA_NOINIT;
  }

  LOG_D(TAG_SD_SPI, "status: flags=0x%02X", (unsigned)sd_status_flags);
  return sd_status_flags;
}

DRESULT USER_SPI_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  if (pdrv != 0U || count == 0U)
  {
    return RES_PARERR;
  }
  if ((sd_status_flags & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }

  if ((card_type & CT_BLOCK) == 0U)
  {
    sector *= 512UL;
  }

  if (count == 1U)
  {
    if (send_cmd(CMD17, sector) == 0U && rcvr_datablock(buff, 512U))
    {
      count = 0U;
    }
  }
  else
  {
    if (send_cmd(CMD18, sector) == 0U)
    {
      do
      {
        if (!rcvr_datablock(buff, 512U))
        {
          break;
        }
        buff += 512;
      } while (--count != 0U);
      (void)send_cmd(CMD12, 0U);
    }
  }

  deselect_card();
  return (count == 0U) ? RES_OK : RES_ERROR;
}

#if _USE_WRITE == 1
DRESULT USER_SPI_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  if (pdrv != 0U || count == 0U)
  {
    return RES_PARERR;
  }
  if ((sd_status_flags & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }
  if ((sd_status_flags & STA_PROTECT) != 0U)
  {
    return RES_WRPRT;
  }

  if ((card_type & CT_BLOCK) == 0U)
  {
    sector *= 512UL;
  }

  if (count == 1U)
  {
    if (send_cmd(CMD24, sector) == 0U && xmit_datablock(buff, 0xFEU))
    {
      count = 0U;
    }
  }
  else
  {
    if ((card_type & CT_SDC) != 0U)
    {
      (void)send_cmd(ACMD23, count);
    }
    if (send_cmd(CMD25, sector) == 0U)
    {
      do
      {
        if (!xmit_datablock(buff, 0xFCU))
        {
          break;
        }
        buff += 512;
      } while (--count != 0U);

      if (!xmit_datablock(NULL, 0xFDU))
      {
        count = 1U;
      }
    }
  }

  deselect_card();
  return (count == 0U) ? RES_OK : RES_ERROR;
}
#endif

#if _USE_IOCTL == 1
DRESULT USER_SPI_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  BYTE csd[16] = {0};
  DRESULT result = RES_ERROR;

  if (pdrv != 0U)
  {
    return RES_PARERR;
  }
  if ((sd_status_flags & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }

  switch (cmd)
  {
  case CTRL_SYNC:
    if (select_card())
    {
      result = RES_OK;
    }
    break;

  case GET_SECTOR_COUNT:
    if (send_cmd(CMD9, 0U) == 0U && rcvr_datablock(csd, 16U))
    {
      DWORD csize = 0U;
      if ((csd[0] >> 6) == 1U)
      {
        csize = (DWORD)csd[9] | ((DWORD)csd[8] << 8) | ((DWORD)(csd[7] & 63U) << 16);
        *(DWORD *)buff = (csize + 1UL) << 10;
      }
      else
      {
        BYTE n = (BYTE)((csd[5] & 15U) + ((csd[10] & 128U) >> 7) + ((csd[9] & 3U) << 1) + 2U);
        csize = (DWORD)((csd[8] >> 6) | ((WORD)csd[7] << 2) | ((WORD)(csd[6] & 3U) << 10));
        *(DWORD *)buff = (csize + 1UL) << (n - 9U);
      }
      result = RES_OK;
    }
    break;

  case GET_BLOCK_SIZE:
    if ((card_type & CT_SD2) != 0U)
    {
      if (send_cmd(ACMD13, 0U) == 0U)
      {
        (void)xchg_spi(0xFFU);
        if (rcvr_datablock(csd, 16U))
        {
          for (BYTE i = 0U; i < (64U - 16U); i++)
          {
            (void)xchg_spi(0xFFU);
          }
          *(DWORD *)buff = 16UL << (csd[10] >> 4);
          result = RES_OK;
        }
      }
    }
    else
    {
      if (send_cmd(CMD9, 0U) == 0U && rcvr_datablock(csd, 16U))
      {
        if ((card_type & CT_SD1) != 0U)
        {
          *(DWORD *)buff =
              (DWORD)((((csd[10] & 63U) << 1) + ((WORD)(csd[11] & 128U) >> 7) + 1U) << ((csd[13] >> 6) - 1U));
        }
        else
        {
          *(DWORD *)buff = (DWORD)(((WORD)((csd[10] & 124U) >> 2) + 1U) *
                                   ((((csd[11] & 3U) << 3) + ((csd[11] & 224U) >> 5) + 1U)));
        }
        result = RES_OK;
      }
    }
    break;

  default:
    result = RES_PARERR;
    break;
  }

  deselect_card();
  return result;
}
#endif
