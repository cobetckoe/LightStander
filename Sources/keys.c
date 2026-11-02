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
    // 这里绝对不能关 EX0。
    //
    // 原来写的是 EX0 = 0 防重入，但它是个一次性开关：中断一触发就把
    // 按键通道关了，而 ClearKeyPending() 只清标志、WaitButtonRelease()
    // 只等松手，没有任何一条路径负责把它重新打开。于是只要中断在某个
    // 「清 pending」的地方先触发，EX0 就永久停在 0。
    //
    // 两个已知现象都是它造成的：
    //   1) 唤醒后停在待机页，再按没反应，等 4.5s 空闲超时熄屏，
    //      重新唤醒一次才恢复 —— 所以表现为「反复唤醒几次才能选股」。
    //   2) 入睡瞬间来一个毛刺，EX0 被关掉，MCU 带着 EX0=0 进 STOP，
    //      没有唤醒源，整机黑屏，只能拔电池重上电。
    //
    // 中断体只有一次字节赋值，边沿触发下重复进入也只是把同一个标志
    // 再置一次，不存在重入问题，本来就不需要互斥。
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

// 回到「等下一次按键」的状态：丢掉残留事件，重新开中断。
static void KeyIdle(void)
{
    ClearKeyPending();
    EX0 = 1;
}

// 返回 1 = 回休眠，0 = 继续保持工作
uint8_t ProcessKey(void)
{
    uint16_t idle = 0;
    uint8_t i;

    while (1) {
        if (!key_pending) {
            // 没按键，累加空闲时间，到点就熄屏回去睡
            delay_ms(10);
            idle++;
            if (idle >= IDLE_TIMEOUT) {
                OLED_Off();
                return 1;
            }
            continue;
        }

        key_pending = 0;
        idle = 0;
        delay_ms(80);           // 消抖

        if (P32) {              // 已经松开，当作抖动忽略
            KeyIdle();
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
            // 最后自己回待机。低电量时它只亮一下电池图标就返回。
            if (ExecuteSelect()) {
                OLED_Off();     // 低电量，直接熄屏回休眠
                KeyIdle();
                return 1;
            }
            WaitButtonRelease();
        }

        KeyIdle();
    }
}


