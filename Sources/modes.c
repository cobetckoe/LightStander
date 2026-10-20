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

// 前缀表：一个结构体把「权重 / 码值 / 位图表」绑在一起，
// 省掉原来那个 12 分支的 GetBitmap switch。三张并行的表容易改漏一行，
// 合成一张之后加前缀只要在一个地方加一条。
typedef struct {
    uint16_t      weight;
    uint16_t      prefix;
    uint8_t code *bitmap;
} PrefixEnt;

static code const PrefixEnt prefix_tab[12] = {
    { 875, 600, bitmap_600 },
    { 235, 601, bitmap_601 },
    { 627, 603, bitmap_603 },
    { 113, 605, bitmap_605 },
    { 621, 688, bitmap_688 },
    { 506,   0, bitmap_000 },
    { 122,   1, bitmap_001 },
    { 977,   2, bitmap_002 },
    {  42,   3, bitmap_003 },
    { 979, 300, bitmap_300 },
    { 471, 301, bitmap_301 },
    { 346, 920, bitmap_920 },
};

#define PREFIX_CNT      (sizeof(prefix_tab) / sizeof(prefix_tab[0]))

// 位图按位标记某个后缀存在。suffix 一定 < 1000，所以位图 125 字节。
#define BMP_HAS(bmp, n) (((bmp)[(n) >> 3] >> ((n) & 0x07)) & 0x01)

static uint16_t Rand16(void)
{
    return (uint16_t)FastRand() | ((uint16_t)FastRand() << 8);
}

// 按各前缀的股票数加权抽取，总权重就是各只股票总数。
// 以前这里写死 5914，改表时忘掉同步就会偏分布；现在求和。
static uint8_t RandPrefixIdx(void)
{
    uint16_t total = 0;
    uint16_t r;
    uint8_t i;

    for (i = 0; i < PREFIX_CNT; i++) total += prefix_tab[i].weight;

    r = Rand16() % total;
    for (i = 0; i < PREFIX_CNT; i++) {
        if (r < prefix_tab[i].weight) return i;
        r -= prefix_tab[i].weight;
    }
    return PREFIX_CNT - 1;      // 越界兜底，正常走不到
}

static void SelectStock(void)
{
    uint8_t   idx;
    uint8_t code *bitmap;
    uint8_t   tries = 0;
    uint16_t  suffix;
    uint32_t  stock_val;

    idx = RandPrefixIdx();
    bitmap = prefix_tab[idx].bitmap;

    // 抽到不在位图里的代码就重抽。位图只标记真实存在的股票，
    // 所以这里必须校验，否则会显示出不存在的代码。
    do {
        suffix = Rand16() % 1000;
        if (++tries > 50) {
            // 50 次都没命中就给个兵底值，避免小概率死循环
            suffix = 1;
            break;
        }
    } while (!BMP_HAS(bitmap, suffix));

    stock_val = (uint32_t)prefix_tab[idx].prefix * 1000 + suffix;
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

// 低光提示页：5 个汉字，墨迹 15px、间距 4px，占 18..108（91px，居中）
static code const struct { uint8_t x; uint8_t ch; } lowlight_tab[5] = {
    { 18, F16_QING },
    { 37, F16_ZHAN },
    { 56, F16_ZAI  },
    { 75, F16_GUANG },
    { 94, F16_LI   },
};

// 等一小会儿或者按键提前结束。返回前把按键松开，免得残留的按住
// 被上层当成一次新的选择。
static void HoldOrSkip(uint16_t ms)
{
    uint16_t t;

    for (t = 0; t < ms / 10; t++) {
        delay_ms(10);
        if (!P32) {
            WaitButtonRelease();
            return;
        }
    }
}

uint8_t ExecuteSelect(void)
{
    uint8_t i;

    // 低电量：显示图标后立刻回休眠
    if (CheckBattery()) {
        DisplayLowBattery();
        return 1;
    }

    if (!IsLightSufficient()) {
        OLED_RowGap(0);
        OLED_Clear();
        for (i = 0; i < 5; i++) {
            OLED_ShowCN16(lowlight_tab[i].x, 0, lowlight_tab[i].ch);
        }
        HoldOrSkip(LOW_LIGHT_WAIT_MS);
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
