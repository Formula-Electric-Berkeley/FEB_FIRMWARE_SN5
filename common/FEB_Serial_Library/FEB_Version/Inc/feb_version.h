/**
 ******************************************************************************
 * @file           : feb_version.h
 * @brief          : FEB Firmware Version & Provenance Metadata
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Exposes two structures that together identify a flashed firmware image:
 *
 *   feb_build_info   - baked in at compile time by cmake/FEB_Version.cmake
 *                      (per-board version, commit hash, branch, dirty flag,
 *                      build UTC timestamp, builder identity).
 *
 *   feb_flash_info   - stamped into a reserved flash section by
 *                      scripts/flash-patcher.py just before programming
 *                      (flash UTC timestamp, flasher identity, magic).
 *
 * The shared `version` console command reads both and prints / emits CSV.
 * A default placeholder value for feb_flash_info is linked into every
 * build so firmware that was never flashed through flash.sh (e.g. loaded
 * by STM32CubeIDE) still prints a legible "UNFLASHED" tag rather than
 * garbage.
 *
 ******************************************************************************
 */

#ifndef FEB_VERSION_H
#define FEB_VERSION_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /* Magic word validated by flash-patcher.py before overwriting the
   * placeholder. Chosen so a stale or wrong-target ELF fails the patch
   * step instead of silently corrupting flash. */
#define FEB_FLASH_INFO_MAGIC 0xFEB1A5F1U

  /* Schema version for feb_flash_info. Bump if the layout ever changes;
   * flash-patcher.py checks this and refuses mismatches. */
#define FEB_FLASH_INFO_SCHEMA 1U

  /* Fixed on-flash layout - 128 bytes total, matches linker .feb_flash_info
   * reservation. All char arrays are null-terminated. Do not reorder or
   * resize without updating flash-patcher.py and bumping FEB_FLASH_INFO_SCHEMA. */
  typedef struct
  {
    uint32_t magic;        /**< FEB_FLASH_INFO_MAGIC */
    uint32_t schema;       /**< FEB_FLASH_INFO_SCHEMA */
    char flash_utc[32];    /**< ISO-8601 UTC, e.g. "2026-04-19T14:30:00Z" */
    char flasher_user[24]; /**< $USER at flash time */
    char flasher_host[32]; /**< hostname at flash time */
    uint8_t reserved[32];  /**< reserved for future fields, zero today */
  } FEB_Flash_Info_t;

  /* Compile-time size check - keep the section exactly 128 bytes. */
  typedef char feb_flash_info_size_check[(sizeof(FEB_Flash_Info_t) == 128) ? 1 : -1];

  /* Compile-time build metadata. Defined per-board by the generated
   * feb_build_info.c (see cmake/FEB_Version.cmake). All strings are
   * static literals in .rodata, guaranteed non-NULL by the generator. */
  typedef struct
  {
    const char *board_name;     /**< e.g. "BMS" */
    const char *version_string; /**< "MAJOR.MINOR.PATCH" for this board */
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    const char *repo_version_string;   /**< repo-wide VERSION */
    const char *common_version_string; /**< common/VERSION */
    const char *commit_short;          /**< 7-char git SHA */
    const char *commit_full;           /**< 40-char git SHA */
    const char *branch;                /**< git branch at build time */
    bool dirty;                        /**< working tree had uncommitted changes */
    const char *build_utc;             /**< ISO-8601 UTC */
    const char *build_user;            /**< $USER at build time */
    const char *build_host;            /**< hostname at build time */
  } FEB_Build_Info_t;

  /* Each board's CMake-generated feb_build_info.c defines this symbol. */
  extern const FEB_Build_Info_t feb_build_info;

  /* The default placeholder lives in feb_flash_info.c; flash-patcher.py
   * overwrites it at flash time. `volatile` so a power-cycle-aware
   * firmware re-reads after reset; `const` so the struct lands in flash. */
  extern const volatile FEB_Flash_Info_t feb_flash_info;

  /* Returns true if feb_flash_info still carries the default
   * "UNFLASHED" placeholder (i.e. flash-patcher.py never ran on this
   * binary). Safe to call before printing the flash block. */
  bool FEB_Version_IsUnflashed(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_VERSION_H */
