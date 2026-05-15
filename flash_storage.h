/*
 * @file    : flash_storage.h
 * @project : AVEN Tracker
 * @brief   : Offline data storage in STM32 internal flash
 * @date    : 2026-04-25
 */

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include "stm32wbxx_hal.h"
#include "tracker.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

/*
 * STM32WB55 Flash layout:
 * Each page = 4KB = 0x1000
 * Page 254 starts at 0x0807F000
 * Page 255 starts at 0x08080000  ← NOT VALID on WB55 (only 256KB = pages 0-127)
 *
 * CORRECTED: STM32WB55RG has 1MB flash = 256 pages (0 to 255)
 * Page 254 = 0x0807E000
 * Page 255 = 0x0807F000
 * Both pages = 8KB total
 *
 * 15 records × 256 bytes = 3840 bytes → fits in one 4KB page safely
 */

#define FLASH_PAGE_SIZE        0x1000         // 4KB per page
#define FLASH_STORAGE_PAGE     127            // last page in linker-visible 512KB app flash
#define FLASH_STORAGE_START    0x0807F000     // reserved 4KB page start
#define FLASH_STORAGE_SIZE     0x1000         // 4KB
#define FLASH_RECORD_SIZE      256            // bytes per record
#define FLASH_MAX_RECORDS      15             // 15 x 256 = 3840

#define FLASH_MAGIC            0xABCD1234

/*
 * FlashRecord_t — exactly 248 bytes
 * 248 / 8 = 31 doublewords — no truncation on DOUBLEWORD write
 *
 * Layout:
 *   magic      4 bytes
 *   timestamp  4 bytes
 *   sent       1 byte
 *   reserved   3 bytes  (padding to keep alignment)
 *   payload  236 bytes
 *   _pad       4 bytes  (padding to reach 248, multiple of 8)
 * Total = 4+4+1+3+236+0 = 248 ✓
 */
typedef struct {
    uint32_t magic;
    uint32_t timestamp;
    uint8_t  sent;
    uint8_t  reserved[3];
    char     payload[236];
} FlashRecord_t;

/* Compile-time size check — will error if struct is not multiple of 8 */
static_assert(sizeof(FlashRecord_t) % 8 == 0,
              "FlashRecord_t must be a multiple of 8 bytes for DOUBLEWORD writes");

typedef struct {
    uint8_t  record_count;
    uint8_t  initialized;
} FlashState_t;

/* Functions */
AvenStatus_t Flash_Init(FlashState_t *state);
AvenStatus_t Flash_SaveRecord(FlashState_t *state, const char *payload);
AvenStatus_t Flash_ReadRecord(FlashState_t *state, uint8_t index, FlashRecord_t *record);
AvenStatus_t Flash_MarkSent(FlashState_t *state, uint8_t index);
AvenStatus_t Flash_EraseAll(FlashState_t *state);
uint8_t      Flash_GetCount(FlashState_t *state);

#include "lte_driver.h"

#endif /* FLASH_STORAGE_H */
