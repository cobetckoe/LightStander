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
#include "adc.h"

static uint16_t rand_state = 0x55AA;

void InitRandomSeed(void)
{
    uint16_t seed = 0;
    uint8_t i, j;
    uint16_t adc;
    for (i = 0; i < 32; i++) {
        adc = ReadLightADC();
        seed ^= adc;
        seed = (seed << 3) | (seed >> 13);  // rotate
        for (j = 0; j < 20; j++) {
            _nop_();
        }
    }
    seed ^= 0x5AA5;
    if (seed == 0) seed = 0x1234;
    rand_state = seed;
}

// Re-seed with fresh light ADC entropy on each button press
void ReSeedFromLight(void)
{
    uint8_t i;
    uint16_t adc;
    for (i = 0; i < 8; i++) {
        adc = ReadLightADC();
        rand_state ^= adc;
        rand_state = rand_state * 0x3D + 0x27;
        _nop_(); _nop_();
    }
}

// 增量必须是奇数。偶数增量会让状态最低位恒定，可达状态只剩 32768
// 个而不是 65536，等于把周期砍半。
uint8_t FastRand(void)
{
    rand_state = rand_state * 0x9D65 + 0x3B7B;
    return (uint8_t)(rand_state ^ (rand_state >> 8));
}

