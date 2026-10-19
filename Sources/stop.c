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
    for (;;) {
        // 先等按键松手，再熄屏。顺序反过来会让屏幕在黑着的情况下
        // 多等好几秒，看起来像死机。
        WaitButtonRelease();
        ClearKeyPending();

        FlushSave();            // 睡觉前把待写的功德值落盘

        ADC_CONTR = 0;

        // 先熄屏，再把待机页写进显存。
        //
        // 写显存走的是 I2C 逻辑事务，只要面板 VDD 在（它一直挂着电池）
        // 就能写，跟电荷泵开没开无关——电荷泵只负责给 OLED 像素的驱动
        // 高压。所以先关屏再画，这一帧用户完全看不到；而醒来时显存里
        // 已经是正确的待机页，OLED_Wake 一开屏就是它，不会先清空再出字。
        // 那个「先清空再出字」就是用户说的闪一下。
        OLED_Off();
        DisplayStandby();

        // P3.3 留给 ADC 做高阻输入，其余引脚恢复 SYS_Init 的配置
        P1M0 = 0x00; P1M1 = 0x00;
        P3M0 = 0x00; P3M1 = 0x08;

        // 休眠时 I2C 两根线保持高电平。面板的 VDD 这时仍接在电池上，
        // 不存在幻影供电的问题；拉低只会经模块上的上拉电阻白耗电流。
        P5M0 = 0x00; P5M1 = 0x00;

        P32 = 1;
        P33 = 1;

        // INT0 用**低电平**触发入睡，醒来后再切回下降沿。
        //
        // 用下降沿入睡有个隐藏的竞态：ClearKeyPending() 和 PCON|=0x02
        // 之间只要来一个下降沿，就会进中断，而中断里为了防重入关了 EX0。
        // 结果 MCU 带着 EX0=0 进了 STOP，再也没有唤醒源。此时屏已经被
        // OLED_Off() 关掉，用户看到的就是黑屏，只有拔电池才能恢复。
        //
        // 低电平触发没有这个问题：只要引脚还是低，请求就一直挂着，
        // 进不了（或立刻退出）STOP；引脚已经变高，说明抖动过去了，
        // 下次真按下时自然还会再来。
        IT0 = 0;
        EX0 = 1;
        EA  = 1;

        // 引脚此刻仍然被拉着就先别睡，交回主循环
        if (!P32) {
            IT0 = 1;
            OLED_Wake();        // 屏刚关掉、显存里是待机页，点亮就行
            return;
        }
        delay_ms(10);           // 等接触抖动过去
        ClearKeyPending();
        P32 = 1;

        PCON |= 0x02;
        _nop_();
        _nop_();

        // ---- 唤醒后从这里继续 ----

        IT0 = 1;
        SYS_Init();             // 引脚回到工作配置
        ClearKeyPending();      // 唤醒那一下按键不算一次选择
        IT0 = 1;
        EX0 = 1;
        EA  = 1;

        ADC_Init();

        // 面板只是关了电荷泵，VDD 一直挂着电池，内部状态和显存都还在，
        // 所以走快速唤醒而不是冷启动那一整套。差出这一秒多，手感区别很大。
        OLED_Wake();

        // 醒来时手指可能还按在键上，等它松开再走，否则接着的
        // ProcessKey() 会把这次按住当成一次新的选择。
        WaitButtonRelease();
        ClearKeyPending();

        // 电量太低就亮一下电池图标再睡回去
        if (CheckBattery()) {
            DisplayLowBattery();
            // 图标看完先熄屏再回循环开头。否则循环开头的 DisplayStandby()
            // 会在亮着的屏幕上演一次「清空再出字」，又能看到闪一下。
            OLED_Off();
            continue;
        }
        return;
    }
}

