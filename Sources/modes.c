/*
 * LightStander -- STC8G1K08A firmware
 *
 * Copyright (C) 2026 Kanchuan Industrial
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "oled.h"
#include "font_cn_16.h"
#include "random.h"
#include "adc.h"
#include "display.h"
#include "eeprom.h"
#include "bitmap.h"
#include "keys.h"

#define RESULT_PAGE_HOLD_MS    1800
#define MERIT_PAGE_HOLD_MS    1000
#define LOW_LIGHT_WAIT_MS      600

// Stock count per prefix. Index order matches prefix_list below, and the
// running order is the same as the old if-ladder, so the distribution is
// unchanged. Kept as data so RandPrefix is a table walk, not 11 compares.
static uint16_t code prefix_weight[12] = {
    875, 235, 627, 113, 621, 506, 122, 977, 42, 979, 471, 346
};
static uint16_t code prefix_list[12] = {
    600, 601, 603, 605, 688, 0, 1, 2, 3, 300, 301, 920
};

static uint8_t code *GetBitmap(uint16_t prefix)
{
    switch (prefix) {
        case 600: return bitmap_600;
        case 601: return bitmap_601;
        case 603: return bitmap_603;
        case 605: return bitmap_605;
        case 688: return bitmap_688;
        case 0:   return bitmap_000;
        case 1:   return bitmap_001;
        case 2:   return bitmap_002;
        case 3:   return bitmap_003;
        case 300: return bitmap_300;
        case 301: return bitmap_301;
        case 920: return bitmap_920;
        default:  return NULL;
    }
}

static uint8_t BitmapCheck(uint8_t code *bmp, uint16_t suffix)
{
    uint8_t byte_idx = suffix >> 3;
    uint8_t bit_idx  = suffix & 0x07;
    return (bmp[byte_idx] >> bit_idx) & 0x01;
}

static uint16_t Rand16(void)
{
    return (uint16_t)FastRand() | ((uint16_t)FastRand() << 8);
}

static uint16_t RandPrefix(void)
{
    // Weighted random based on stock count per prefix.
    // 600(875) 601(235) 603(627) 605(113) 688(621)
    // 000(506) 001(122) 002(977) 003(42) 300(979) 301(471) 920(346)
    // Total: 5914. Table-driven so the linker stores 12 words of data
    // instead of an 11-deep ladder of 16-bit compares.
    uint16_t r = Rand16() % 5914;
    uint8_t i;
    for (i = 0; i < 12; i++) {
        if (r < prefix_weight[i]) return prefix_list[i];
        r -= prefix_weight[i];
    }
    return 920;
}

static void SelectStock(void)
{
    uint16_t prefix;
    uint8_t code *bitmap;
    uint8_t tries = 0;
    uint16_t suffix;
    uint32_t stock_val;

    prefix = RandPrefix();
    bitmap = GetBitmap(prefix);
    if (bitmap == NULL) {
        return;
    }

    do {
        suffix = (uint16_t)FastRand() | ((uint16_t)FastRand() << 8);
        suffix = suffix % 1000;
        tries++;
        if (tries > 50) {
            // Fallback: just use whatever we got
            suffix = 1;
            break;
        }
    } while (!BitmapCheck(bitmap, suffix));

    stock_val = (uint32_t)prefix * 1000 + suffix;
    DisplayStock(stock_val);
    // EX0 stays off while the result page is shown; otherwise key interrupts
    // queue key_pending and ProcessKey mistakes them for a new selection.
    delay_ms(RESULT_PAGE_HOLD_MS);
}

// Randomly select one of 9 trading actions
static void SelectTradeAction(void)
{
    uint8_t idx;
    idx = FastRand() % 9;

    DisplayTradeAction(idx);
    // Same as above: keep EX0 off while the page is shown.
    delay_ms(RESULT_PAGE_HOLD_MS);
}

uint8_t ExecuteSelect(void)
{
    // Low battery: show the icon then go back to sleep right away (ret 1)
    if (CheckBattery()) {
        DisplayLowBattery();
        return 1;
    }

    if (!IsLightSufficient()) {
        uint16_t wait = 0;
        OLED_Clear();
        // 5 chars, 15px ink, 4px gap => 18..108 (91px, centred)
        OLED_ShowCN16(18, 0, F16_QING);
        OLED_ShowCN16(37, 0, F16_ZHAN);
        OLED_ShowCN16(56, 0, F16_ZAI);
        OLED_ShowCN16(75, 0, F16_GUANG);
        OLED_ShowCN16(94, 0, F16_LI);
        // Poll: up to LOW_LIGHT_WAIT_MS, or return at once on a key press
        while (wait < (LOW_LIGHT_WAIT_MS / 10)) {
            delay_ms(10);
            wait++;
            if (!P32) {
                WaitButtonRelease();    // bounded wait, avoids a stuck pin
                break;
            }
        }
        DisplayStandby();   // restore the question row
        return 0;
    }

    // Re-seed RNG with fresh light entropy before each selection
    ReSeedFromLight();

    // Execute based on current mode
    if (current_mode == MODE_TRADE) {
        SelectTradeAction();
    } else {
        SelectStock();
    }

    light_merit++;
    SaveMeritDeferred();

    // Last page: row 1 "GUANG GONG DE +1", row 2 total merit
    DisplayMeritAdd(light_merit);
    delay_ms(MERIT_PAGE_HOLD_MS);
    DisplayStandby();
    return 0;
}
