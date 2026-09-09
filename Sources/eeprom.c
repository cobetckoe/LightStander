#include "config.h"
#include "STC8G_H_EEPROM.h"
#include "eeprom.h"

// Dual backup for power-loss protection
// Slot A: 0x0000, Slot B: 0x0010
#define EEPROM_SLOT_A   0x0000
#define EEPROM_SLOT_B   0x0010
#define SLOT_SIZE       5

uint32_t light_merit = 0;
uint8_t  current_mode = MODE_STOCK;

static uint8_t save_pending = 0;

static uint8_t VerifySlot(uint16_t addr, uint32_t merit, uint8_t mode)
{
    uint8_t buf[SLOT_SIZE];
    EEPROM_read_n(addr, buf, SLOT_SIZE);
    if (buf[0] != (uint8_t)(merit))        return 0;
    if (buf[1] != (uint8_t)(merit >> 8))   return 0;
    if (buf[2] != (uint8_t)(merit >> 16))  return 0;
    if (buf[3] != (uint8_t)(merit >> 24))  return 0;
    if (buf[4] != mode)                     return 0;
    return 1;
}

static uint8_t ReadSlot(uint16_t addr, uint32_t *merit, uint8_t *mode)
{
    uint8_t buf[SLOT_SIZE];
    EEPROM_read_n(addr, buf, SLOT_SIZE);
    *mode = buf[4];
    *merit = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
             ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    // Valid if mode is either MODE_STOCK(0) or MODE_TRADE(1)
    // and merit is not 0xFFFFFFFF (erased EEPROM)
    if (*mode > MODE_TRADE) return 0;
    if (*merit == 0xFFFFFFFF) return 0;
    return 1;
}

static void WriteSlot(uint16_t addr, uint32_t merit, uint8_t mode)
{
    uint8_t buf[SLOT_SIZE];
    buf[0] = (uint8_t)(merit);
    buf[1] = (uint8_t)(merit >> 8);
    buf[2] = (uint8_t)(merit >> 16);
    buf[3] = (uint8_t)(merit >> 24);
    buf[4] = mode;
    EEPROM_SectorErase(addr);
    EEPROM_write_n(addr, buf, SLOT_SIZE);
}

void LoadPersistent(void)
{
    uint32_t merit_a, merit_b;
    uint8_t mode_a, mode_b;
    uint8_t valid_a, valid_b;

    valid_a = ReadSlot(EEPROM_SLOT_A, &merit_a, &mode_a);
    valid_b = ReadSlot(EEPROM_SLOT_B, &merit_b, &mode_b);

    if (valid_a) {
        light_merit = merit_a;
        current_mode = mode_a;
    } else if (valid_b) {
        light_merit = merit_b;
        current_mode = mode_b;
    } else {
        // Both invalid, initialize
        light_merit = 0;
        current_mode = MODE_STOCK;
        SavePersistent();
    }
}

void SavePersistent(void)
{
    uint8_t ea_save = EA;
    uint8_t retry = 0;

    EA = 0;

    // Write slot A first
    do {
        WriteSlot(EEPROM_SLOT_A, light_merit, current_mode);
        retry++;
    } while (!VerifySlot(EEPROM_SLOT_A, light_merit, current_mode) && retry < 3);

    // Write slot B
    retry = 0;
    do {
        WriteSlot(EEPROM_SLOT_B, light_merit, current_mode);
        retry++;
    } while (!VerifySlot(EEPROM_SLOT_B, light_merit, current_mode) && retry < 3);

    EA = ea_save;
}

void SaveMeritDeferred(void)
{
    save_pending++;
    if (save_pending >= 10) {
        save_pending = 0;
        SavePersistent();
    }
}

// Force flush any pending merit save to EEPROM immediately.
// 用于在进入低功耗或关机前确保最新数据已写入。
void FlushSave(void)
{
    save_pending = 0;
    SavePersistent();
}
