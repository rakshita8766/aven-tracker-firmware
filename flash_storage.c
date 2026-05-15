/*
 * @file    : flash_storage.c
 * @project : AVEN Tracker
 * @brief   : Offline data storage using STM32WB55 internal flash
 *
 * HOW IT WORKS:
 * - Flash is divided into fixed 256-byte slots (FLASH_MAX_RECORDS = 15)
 * - Each slot has a magic number to identify valid data
 * - 0xFFFFFFFF = erased/empty slot (never written)
 * - 0x00000000 = sent slot (zeroed after sending)
 * - On boot: counts unsent records, cleans up if needed
 * - On save: finds empty slot, writes record
 * - On send: zeros the slot so it can be reused after next erase
 * - Flash can only go 1→0 without erase, full page erase needed to reset to 0xFF
 *
 * OFFLINE → ONLINE flow:
 * ----------------------
 * 1. Network down → Flash_SaveRecord() called → saves to flash
 * 2. Network up   → caller reads records with Flash_ReadRecord()
 *                 → publishes each one via MQTT
 *                 → calls Flash_MarkSent() for each sent record
 *                 → Flash_Init() on next boot cleans up sent records
 */

#include "flash_storage.h"

/* -------------------------------------------------------------------------
 * Internal helper: wait for flash to be ready
 * STM32WB55 flash controller needs time to settle after lock/unlock/erase
 * Never call HAL_FLASH_Program immediately after HAL_FLASH_Lock */
static void Flash_WaitReady(void)
{
    /* Wait until flash is not busy */
    uint32_t timeout = 1000; /* 1 second max */
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) && timeout > 0)
    {
        HAL_Delay(1);
        timeout--;
    }
    /* Small extra settle time for WB55 flash controller */
    HAL_Delay(2);
}

/* -------------------------------------------------------------------------
 * Flash_Init
 * Called once at boot. Scans flash, counts unsent records.
 * If only sent (dirty) records exist and nothing pending → erase for clean start.
 * ------------------------------------------------------------------------- */
AvenStatus_t Flash_Init(FlashState_t *state)
{
    memset(state, 0, sizeof(FlashState_t));

    uint8_t count = 0;   /* unsent records */
    uint8_t dirty = 0;   /* sent but not yet erased records */
    uint8_t clean = 0;   /* erased slots available for writing */
    FlashRecord_t rec;

    for(int i = 0; i < FLASH_MAX_RECORDS; i++)
    {
        uint32_t addr = FLASH_STORAGE_START + (i * FLASH_RECORD_SIZE);
        memcpy(&rec, (void *)addr, sizeof(FlashRecord_t));

        if(rec.magic == FLASH_MAGIC && rec.sent == 0)
        {
            count++;   /* valid unsent record */
        }
        else if(rec.magic == FLASH_MAGIC && rec.sent == 1)
        {
            dirty = 1; /* sent record still in flash = dirty */
        }
        else if(*(uint32_t *)addr == 0xFFFFFFFF)
        {
            clean++;
        }
    }

    /*
     * Only erase if dirty AND no pending records.
     * If there are pending records, keep them — do not erase.
     */
    if((dirty || clean == 0) && count == 0)
    {
        printf("No pending records and flash page needs erase\r\n");
        state->initialized = 1;
        Flash_EraseAll(state);
    }

    state->record_count = count;
    state->initialized  = 1;

    printf("Flash init: %d pending records\r\n", count);
    return AVEN_OK;
}

/* -------------------------------------------------------------------------
 * Flash_EraseAll
 * Erases one reserved flash page.
 * This resets all slots to 0xFFFFFFFF.
 * IMPORTANT: After this call always wait before writing.
 * ------------------------------------------------------------------------- */
AvenStatus_t Flash_EraseAll(FlashState_t *state)
{
    /* Clear any previous flash errors before starting */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    HAL_FLASH_Unlock();
    Flash_WaitReady(); /* wait before erase */

    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Page      = FLASH_STORAGE_PAGE; /* reserved storage page */
    erase.NbPages   = 1;                  /* erase only 1 page = 4KB */

    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);

    HAL_FLASH_Lock();
    Flash_WaitReady(); /* wait after erase before any next operation */

    if(status != HAL_OK)
    {
        printf("Flash erase FAILED page_error=0x%08lX\r\n", page_error);
        return AVEN_ERROR;
    }

    state->record_count = 0;
    printf("Flash erased OK\r\n");
    return AVEN_OK;
}

/* -------------------------------------------------------------------------
 * Flash_SaveRecord
 * Saves one payload string to the next available flash slot.
 * If no clean slot found → erases everything → uses slot 0.
 *
 * Slot states:
 *   0xFFFFFFFF = erased = empty = safe to write
 *   0xABCD1234 = FLASH_MAGIC = has valid data
 *   0x00000000 = zeroed = was sent = needs erase before reuse
 * ------------------------------------------------------------------------- */
AvenStatus_t Flash_SaveRecord(FlashState_t *state, const char *payload)
{
    int slot = -1;

    /* Scan for a clean erased slot (0xFFFFFFFF only) */
    for(int i = 0; i < FLASH_MAX_RECORDS; i++)
    {
        uint32_t addr      = FLASH_STORAGE_START + (i * FLASH_RECORD_SIZE);
        uint32_t first_word = *(uint32_t *)addr;

        if(first_word == 0xFFFFFFFF)
        {
            slot = i;
            break;
        }
    }

    /* Do not erase unsent offline records. Retry after upload frees space. */
    if(slot == -1)
    {
        printf("Flash full - cannot save record\r\n");
        return AVEN_ERROR;
    }

    /* Build the record in RAM first */
    FlashRecord_t new_rec;
    memset(&new_rec, 0xFF, sizeof(FlashRecord_t)); /* fill with 0xFF = erased state */
    new_rec.magic     = FLASH_MAGIC;
    new_rec.timestamp = HAL_GetTick();
    new_rec.sent      = 0;
    strncpy(new_rec.payload, payload, 235);
    new_rec.payload[235] = '\0';

    /* Clear any flash errors, unlock, wait to settle */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_FLASH_Unlock();
    Flash_WaitReady(); /* CRITICAL: must wait after unlock on STM32WB55 */

    uint32_t write_addr = FLASH_STORAGE_START + (slot * FLASH_RECORD_SIZE);
    uint64_t *data      = (uint64_t *)&new_rec;
    uint32_t words      = sizeof(FlashRecord_t) / 8; /* 248 / 8 = 31 */

    for(uint32_t i = 0; i < words; i++)
    {
        HAL_StatusTypeDef status = HAL_FLASH_Program(
                                       FLASH_TYPEPROGRAM_DOUBLEWORD,
                                       write_addr + (i * 8),
                                       data[i]);

        if(status != HAL_OK)
        {
            HAL_FLASH_Lock();
            printf("Flash write FAILED at word %lu error=0x%08lX\r\n",
                   i, HAL_FLASH_GetError());
            return AVEN_ERROR;
        }
    }

    HAL_FLASH_Lock();
    Flash_WaitReady(); /* wait after write before any next operation */

    state->record_count++;
    printf("Flash saved slot=%d count=%d\r\n", slot, state->record_count);
    return AVEN_OK;
}

/* -------------------------------------------------------------------------
 * Flash_ReadRecord
 * Reads one record from flash by index.
 * Returns AVEN_ERROR if index out of range or magic does not match.
 * ------------------------------------------------------------------------- */
AvenStatus_t Flash_ReadRecord(FlashState_t *state,
                               uint8_t index,
                               FlashRecord_t *record)
{
    if(index >= FLASH_MAX_RECORDS)
    {
        printf("Flash read: index %d out of range\r\n", index);
        return AVEN_ERROR;
    }

    uint32_t addr = FLASH_STORAGE_START + (index * FLASH_RECORD_SIZE);
    memcpy(record, (void *)addr, sizeof(FlashRecord_t));

    if(record->magic != FLASH_MAGIC)
    {
        return AVEN_ERROR; /* empty or zeroed slot */
    }

    return AVEN_OK;
}

/* -------------------------------------------------------------------------
 * Flash_MarkSent
 * Zeros the entire slot after a record has been successfully sent.
 * Flash can go 1→0 without erase so we write all zeros.
 * Slot will be reused after next Flash_EraseAll restores it to 0xFFFFFFFF.
 * ------------------------------------------------------------------------- */
AvenStatus_t Flash_MarkSent(FlashState_t *state, uint8_t index)
{
    if(index >= FLASH_MAX_RECORDS)
    {
        printf("MarkSent: index %d out of range\r\n", index);
        return AVEN_ERROR;
    }

    /* Verify record exists before marking */
    FlashRecord_t rec;
    if(Flash_ReadRecord(state, index, &rec) != AVEN_OK)
    {
        printf("MarkSent: no valid record at index %d\r\n", index);
        return AVEN_ERROR;
    }

    /* Zero the entire slot — all 248 bytes — not just first 8 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_FLASH_Unlock();
    Flash_WaitReady();

    uint32_t base_addr = FLASH_STORAGE_START + (index * FLASH_RECORD_SIZE);
    uint64_t zero      = 0;
    uint32_t words     = sizeof(FlashRecord_t) / 8; /* 31 doublewords */

    for(uint32_t i = 0; i < words; i++)
    {
        HAL_StatusTypeDef status = HAL_FLASH_Program(
                                       FLASH_TYPEPROGRAM_DOUBLEWORD,
                                       base_addr + (i * 8),
                                       zero);

        if(status != HAL_OK)
        {
            HAL_FLASH_Lock();
            printf("MarkSent: write FAILED at word %lu\r\n", i);
            return AVEN_ERROR;
        }
    }

    HAL_FLASH_Lock();
    Flash_WaitReady();

    if(state->record_count > 0)
        state->record_count--;

    printf("Flash record %d marked sent count=%d\r\n", index, state->record_count);
    return AVEN_OK;
}
/* Flash_GetCount, Returns number of unsent records currently in flash. */
uint8_t Flash_GetCount(FlashState_t *state)
{
    return state->record_count;
}
