#include "config.h"
#include "oled.h"
#include "display.h"
#include "modes.h"
#include "eeprom.h"
#include "font_cn_16.h"

#define IDLE_TIMEOUT  500

static volatile uint8_t key_pending = 0;

void INT0_ISR(void) interrupt 0
{
    EX0 = 0;  // Disable interrupt to prevent re-entry
    key_pending = 1;
}

void EnableKeyInterrupt(void)
{
    IT0 = 1;
    EX0 = 1;
    EA  = 1;
}

// Return: 1 = go back to sleep, 0 = stay active
uint8_t ProcessKey(void)
{
    uint16_t idle = 0;
    uint8_t i, key;

    while (1) {
        key = key_pending;
        key_pending = 0;

        if (key) {
            idle = 0;
            // Debounce: wait 80ms then check
            delay_ms(80);
            if (P32) {
                // Released, ignore
                EX0 = 1;
                continue;
            }

            // Button pressed, wait for release
            for (i = 0; i < 100; i++) {
                delay_ms(10);
                if (P32) break;
            }

            if (i >= 100) {
                // Long press (>1s) - toggle mode
                current_mode = (current_mode == MODE_STOCK) ? MODE_TRADE : MODE_STOCK;
                SavePersistent();
                OLED_Clear();
                // Show mode name centered: 4 CN chars x 16px = 64px, x=32
                if (current_mode == MODE_TRADE) {
                    OLED_ShowCN16x2(32, 0, F16_YI, F16_WAI);
                    OLED_ShowCN16x2(64, 0, F16_CAO, F16_PAN);
                } else {
                    OLED_ShowCN16x2(32, 0, F16_SUI, F16_JI2);
                    OLED_ShowCN16x2(64, 0, F16_XUAN, F16_GU);
                }
                delay_ms(1500);
            } else {
                // Short press
                ExecuteSelect();
            }

            // Wait for release before re-enabling interrupt
            while (!P32);
            delay_ms(80);  // Debounce release
            key_pending = 0;
            EX0 = 1;
            DisplayStandby();
            continue;
        }

        // No key, idle count
        delay_ms(10);
        idle++;
        if (idle >= IDLE_TIMEOUT) {
            OLED_Off();
            return 1;
        }
    }
}

// Delay for specified ms, return 1 if key pressed early
uint8_t delay_or_key_ms(uint16_t ms)
{
    uint16_t j;
    for (j = 0; j < ms; j += 10) {
        delay_ms(10);
        if (!P32) return 1;
    }
    return 0;
}
