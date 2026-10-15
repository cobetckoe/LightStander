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
#include "eeprom.h"
#include "adc.h"
#include "oled.h"
#include "display.h"
#include "keys.h"

void EnterStopMode(void)
{
sleep:
    // Close peripherals to save power
    // Flush any pending EEPROM writes before powering down
    FlushSave();

    ADC_CONTR = 0;
    OLED_Off();

    // Put P3.3 back to high-Z for the ADC, rest as in SYS_Init
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00; P3M1 = 0x08;
    P5M0 = 0x00; P5M1 = 0x00;

    // P3.2 must be high before STOP; otherwise a wake-up level of 0
    // on P3.2 (the classic ORL P3,#08H trick) would look like a key
    // that is already held and the wait loop below would never exit.
    // Set P3.2 = P3.3 = 1 to guarantee a clean wake-up edge.
    P32 = 1;
    P33 = 1;

    // Enable INT0 wake-up
    IT0 = 1;
    EX0 = 1;
    EA  = 1;

    // Clear any stale INT0 request flag before entering STOP,
    ClearKeyPending();

    // Enter STOP mode
    PCON |= 0x02;
    _nop_();
    _nop_();

    // === MCU wakes up here ===

    // Re-init pins (same configuration as SYS_Init)
    SYS_Init();

    // Let the pin settle after wake-up before the debounce wait
    delay_ms(120);
    WaitButtonRelease();
    // Discard the wake-up key press (waking up must not select).
    ClearKeyPending();
    // Re-enable INT0 after discarding the wake-up press
    IT0 = 1;
    EX0 = 1;
    EA  = 1;
    delay_ms(40);

    // Re-init peripherals
    ADC_Init();

    // Still too low? show the icon and go back to sleep
    if (CheckBattery()) {
        OLED_Init();
        DisplayLowBattery();
        goto sleep;
    }

    OLED_Init();
}

