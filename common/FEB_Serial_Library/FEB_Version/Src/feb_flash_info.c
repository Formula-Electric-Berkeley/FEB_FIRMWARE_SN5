/**
 ******************************************************************************
 * @file           : feb_flash_info.c
 * @brief          : Default flash-time metadata placeholder
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Defines feb_flash_info in the .feb_flash_info section reserved by each
 * board's linker script. scripts/flash-patcher.py rewrites this region
 * on every flash to carry the real UTC timestamp + flasher identity.
 *
 * Triple protection against being dropped by the linker:
 *   1. __attribute__((used))             - don't remove even if unused
 *   2. __attribute__((section(...)))     - land in the right region
 *   3. KEEP(*(.feb_flash_info)) in .ld   - survive --gc-sections
 *
 ******************************************************************************
 */

#include "feb_version.h"

#include <string.h>

#define FEB_FLASH_INFO_PLACEHOLDER_UTC "UNFLASHED-0000-00-00T00:00:00Z"
#define FEB_FLASH_INFO_PLACEHOLDER_USER "unknown"
#define FEB_FLASH_INFO_PLACEHOLDER_HOST "unknown"

const volatile FEB_Flash_Info_t feb_flash_info __attribute__((used, section(".feb_flash_info"))) = {
    .magic = FEB_FLASH_INFO_MAGIC,
    .schema = FEB_FLASH_INFO_SCHEMA,
    .flash_utc = FEB_FLASH_INFO_PLACEHOLDER_UTC,
    .flasher_user = FEB_FLASH_INFO_PLACEHOLDER_USER,
    .flasher_host = FEB_FLASH_INFO_PLACEHOLDER_HOST,
    .reserved = {0},
};

bool FEB_Version_IsUnflashed(void)
{
  /* Read byte-wise through a volatile pointer so the compiler can't
   * cache the placeholder away. strcmp on a volatile array would UB,
   * so copy to a local first. */
  char local_utc[sizeof(feb_flash_info.flash_utc)];
  for (size_t i = 0; i < sizeof(local_utc); i++)
  {
    local_utc[i] = feb_flash_info.flash_utc[i];
  }
  return strncmp(local_utc, "UNFLASHED", 9) == 0;
}
