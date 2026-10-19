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
    EX0 = 0;  // 防重入
    key_pending = 1;
}

void EnableKeyInterrupt(void)
{
    IT0 = 1;
    EX0 = 1;
    EA  = 1;
}

// 丢弃未处理的按键事件，并清 INT0 请求标志。
// 用在两处：进 STOP 前清残留；唤醒后丢掉「唤醒那一下」，
// 否则唤醒瞬间会被误当成一次选股。
void ClearKeyPending(void)
{
    IE0 = 0;
    key_pending = 0;
}

// 等按键松开，带上限，防止引脚异常拉低把程序卡死
void WaitButtonRelease(void)
{
    uint16_t t = 0;

    if (P32) return;            // 已经松开了，直接走

    while (!P32 && t < 400) {   // 最长约 4.7s
        delay_ms(10);
        t++;
    }
    delay_ms(80);               // 松开消抖
}

// 返回 1 = 回休眠，0 = 继续保持工作
uint8_t ProcessKey(void)
{
    uint16_t idle = 0;
    uint8_t i;

    while (1) {
        if (key_pending) {
            key_pending = 0;
            idle = 0;

            delay_ms(80);
            if (P32) {
                // 已经松开，当作抖动忽略，顺带清掉残留标志
                ClearKeyPending();
                EX0 = 1;
                continue;
            }

            // 按住不放：量一下时长，超过 0.9s 算长按
            for (i = 0; i < 90; i++) {
                delay_ms(10);
                if (P32) break;
            }

            if (i >= 90) {
                // 长按：立刻切模式并刷新显示，不用等松手
                current_mode = (current_mode == MODE_STOCK) ? MODE_TRADE : MODE_STOCK;
                SavePersistent();
                DisplayStandby();
                WaitButtonRelease();
            } else {
                // 短按：跑一次选股/操盘，内部会依次显示结果页、功德页，
                // 最后自己回待机
                uint8_t low_bat = ExecuteSelect();
                WaitButtonRelease();
                if (low_bat) {
                    OLED_Off();     // 低电量，直接熄屏回休眠
                    ClearKeyPending();
                    EX0 = 1;
                    return 1;
                }
            }

            ClearKeyPending();
            EX0 = 1;
            continue;
        }

        // 没按键，累加空闲时间
        delay_ms(10);
        idle++;
        if (idle >= IDLE_TIMEOUT) {
            OLED_Off();
            return 1;
        }
    }
}


