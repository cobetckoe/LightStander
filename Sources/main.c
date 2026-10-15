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

    // The ADC must be powered and timed before the very first read.
    // Without this the cold-boot path ran InitRandomSeed() (32 reads) on a
    // powered-down, untimed ADC: every read timed out and returned the 4096
    // error sentinel, so the seed was constant and any later check that
    // trusted a raw reading could mistake 4096 for a real value.
    ADC_Init();

    OLED_Init();
    LoadPersistent();
    InitRandomSeed();
    EnableKeyInterrupt();

    // Boot straight into the standby question page
    DisplayStandby();

    while (1) {
        EnterStopMode();
        // Already awake (OLED re-initialised inside EnterStopMode)
        DisplayStandby();
        // Active loop: wait for user action
        while (1) {
            if (ProcessKey()) break;  // executed selection, back to sleep
        }
    }
}

void SYS_Init(void)
{
    // GPIO config
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x08;                 // P3.3 high-Z for ADC
    P5M0 = 0x30; P5M1 = 0x30;   // P5.4/P5.5 open-drain for I2C
}

void delay_ms(uint16_t ms)
{
    uint16_t i;
    do {
        i = 2048;  // ~1.17ms at 24MHz (power-of-2, avoids div)
        while (--i);
    } while (--ms);
}
