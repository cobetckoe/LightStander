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

// 各前缀的股票数，顺序与 prefix_list 一致，也与旧 if 梯子相同，
// 所以选股分布不变。做成表是为了让 RandPrefix 变成查表，而不是 11 次比较。
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
    // 按各前缀股票数加权，总数 5914
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
            // 50 次都没命中就给个兵底值，避免小概率死循环
            suffix = 1;
            break;
        }
    } while (!BitmapCheck(bitmap, suffix));

    stock_val = (uint32_t)prefix * 1000 + suffix;
    DisplayStock(stock_val);
    // 结果页期间 EX0 保持关着，否则按键会攒下 key_pending，
    // 被 ProcessKey 当成一次新的选择。
    delay_ms(RESULT_PAGE_HOLD_MS);
}

// 从 9 个操盘动作里随机挑一个
static void SelectTradeAction(void)
{
    uint8_t idx;
    idx = FastRand() % 9;

    DisplayTradeAction(idx);
    delay_ms(RESULT_PAGE_HOLD_MS);
}

uint8_t ExecuteSelect(void)
{
    // 低电量：显示图标后立刻回休眠
    if (CheckBattery()) {
        DisplayLowBattery();
        return 1;
    }

    if (!IsLightSufficient()) {
        uint16_t wait = 0;
        OLED_Clear();
        // 5 个字，墨迹 15px、间距 4px，占 18..108（91px，居中）
        OLED_ShowCN16(18, 0, F16_QING);
        OLED_ShowCN16(37, 0, F16_ZHAN);
        OLED_ShowCN16(56, 0, F16_ZAI);
        OLED_ShowCN16(75, 0, F16_GUANG);
        OLED_ShowCN16(94, 0, F16_LI);
        // 最多等 600ms，按一下键可以提前走
        while (wait < (LOW_LIGHT_WAIT_MS / 10)) {
            delay_ms(10);
            wait++;
            if (!P32) {
                WaitButtonRelease();
                break;
            }
        }
        DisplayStandby();
        return 0;
    }

    // 每次选股前用新的光照熵重播种
    ReSeedFromLight();

    if (current_mode == MODE_TRADE) {
        SelectTradeAction();
    } else {
        SelectStock();
    }

    light_merit++;
    SaveMeritDeferred();

    // 末页：上排「光功德 +1」，下排累计值
    DisplayMeritAdd(light_merit);
    delay_ms(MERIT_PAGE_HOLD_MS);
    DisplayStandby();
    return 0;
}
