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
#include "display.h"
#include "modes.h"
#include "eeprom.h"

#define IDLE_TIMEOUT  450

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

// 丢弃尚未处理的按键事件，并清 INT0 请求标志。
// 用于：① 进 STOP 前清掉残留请求；② 唤醒后丢掉“用于唤醒的那一次按键”，
// 否则唤醒瞬间 ProcessKey 会把唤醒按键误当成一次选股操作。
void ClearKeyPending(void)
{
    IE0 = 0;
    key_pending = 0;
}

// 等待按键释放（带上限，防止引脚异常拉低导致死机），末尾带消抖
void WaitButtonRelease(void)
{
    uint16_t t = 0;

    if (P32) return;    // 已经释放，直接退出，避免误判导致卡住

    while (!P32 && t < 400) {   // 最长约 4.7s
        delay_ms(10);
        t++;
    }
    delay_ms(80);               // 释放消抖
}

// Return: 1 = go back to sleep, 0 = stay active
uint8_t ProcessKey(void)
{
    uint16_t idle = 0;
    uint8_t i;

    while (1) {
        if (key_pending) {
            key_pending = 0;
            idle = 0;

            // Debounce: wait 80ms then check
            delay_ms(80);
            if (P32) {
                // Released / glitch, ignore and clear stale flag
                ClearKeyPending();
                EX0 = 1;
                continue;
            }

            // Button held: measure hold time (>0.9s = long press)
            for (i = 0; i < 90; i++) {
                delay_ms(10);
                if (P32) break;
            }

            if (i >= 90) {
                // 长按：立即切换模式并刷新显示，避免“必须松手后才看到切换”的现象
                current_mode = (current_mode == MODE_STOCK) ? MODE_TRADE : MODE_STOCK;
                SavePersistent();
                DisplayStandby();
                WaitButtonRelease();
            } else {
                // 短按：执行一次选股/操盘（内部依次显示结果页与功德页，最后自动回待机）
                uint8_t low_bat = ExecuteSelect();
                WaitButtonRelease();
                if (low_bat) {
                    OLED_Off();     // 低电量：立即熄屏回休眠
                    ClearKeyPending();
                    EX0 = 1;
                    return 1;
                }
            }

            ClearKeyPending();
            EX0 = 1;
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


