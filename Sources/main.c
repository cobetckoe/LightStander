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
#include "oled.h"
#include "random.h"
#include "eeprom.h"
#include "modes.h"
#include "keys.h"
#include "display.h"
#include "stop.h"

void main(void)
{
    SYS_Init();

    // ADC 必须先上电并配置好时序才能读。少了这一句，冷启动时
    // InitRandomSeed() 的 32 次读取会全部超时返回 4096，种子变成定值。
    ADC_Init();

    OLED_Init();
    LoadPersistent();
    InitRandomSeed();
    EnableKeyInterrupt();

#if ADC_SELFTEST
    DisplaySelftest();
#endif

    DisplayStandby();           // 开机直接进待机页

    while (1) {
        EnterStopMode();
        // 醒来时 OLED 已在 EnterStopMode 里重新初始化过
        DisplayStandby();
        while (1) {
            if (ProcessKey()) break;    // 已经执行完一次选择，回休眠
        }
    }
}

void SYS_Init(void)
{
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x08;                // P3.3 高阻，留给 ADC
    P5M0 = 0x30; P5M1 = 0x30;   // P5.4/P5.5 开漏，给 I2C 用
}

void delay_ms(uint16_t ms)
{
    uint16_t i;
    do {
        i = 2048;               // 24MHz 下约 1.17ms，取 2 的幂省一次除法
        while (--i);
    } while (--ms);
}
