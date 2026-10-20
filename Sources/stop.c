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

// 唤醒后恢复中断和引脚配置。三处都要用，抽出来免得漏掉某一条。
static void ResumeAfterWake(void)
{
    SYS_Init();             // 引脚回到工作配置
    IT0 = 1;                // 工作期间用下降沿，配合 ISR 的防重入
    ClearKeyPending();      // 唤醒那一下按键不算一次选择
    EX0 = 1;
    EA  = 1;
    ADC_Init();             // STOP 期间 ADC 掉电了
}

void EnterStopMode(void)
{
    for (;;) {
        // 先等按键松手，再熄屏。顺序反过来会让屏幕在黑着的情况下
        // 多等好几秒，看起来像死机。
        WaitButtonRelease();
        ClearKeyPending();

        FlushSave();            // 睡觉前把待写的功德值落盘
        ADC_CONTR = 0;

        // 熄屏就够了，不必先把待机页写进显存。
        //
        // 以前这里是「先熄屏再画待机页」，指望醒来直接点亮显存里的内容。
        // 面板在睡眠期间有可能自己复位（电池偏弱时漏电流会把它拖到 POR
        // 门限以下），显存跟着一起清零，那一页就白画了。既然唤醒后无论如何
        // 都要重画，睡前这一遍就没有意义，省掉它还能让入睡快一点。
        OLED_Off();

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

        if (P32) {
            delay_ms(10);       // 等接触抖动过去
            ClearKeyPending();
            P32 = 1;
            PCON |= 0x02;
            _nop_();
            _nop_();
        }
        // 引脚还是低就不进 STOP，直接走下面的收尾。
        // 这条路径同样要过 ResumeAfterWake：上面已经把 ADC_CONTR 清零、
        // 引脚也改回了休眠配置，不恢复的话后面每次读 ADC 都超时返回
        // 4096，低电量和弱光判断会静默失效。

        // ---- 唤醒后从这里继续 ----

        ResumeAfterWake();

        // 重发整表配置，全程关屏。这里不能走 OLED_Init：那要先泄放
        // 550ms 残压再清显存，用户会看见一段黑屏加一次重画。
        OLED_Wake();

        // 醒来时手指可能还按在键上，等它松开再走，否则接着的
        // ProcessKey() 会把这次按住当成一次新的选择。
        WaitButtonRelease();
        ClearKeyPending();

        // 电量太低就亮一下电池图标，然后睡回去
        if (CheckBattery()) {
            DisplayLowBattery();
            // 图标看完先熄屏再回循环开头，否则下一轮入睡前那一帧
            // 会在亮着的屏幕上演一次「清空再出字」，又能看到闪一下。
            OLED_Off();
            continue;
        }

        // 正常路径：在黑屏状态下把待机页画好，再一次性开屏。
        // 用户看到的第一帧就是完整的待机页，中间没有空白也没有重画。
        DisplayStandby();
        OLED_On();
        return;
    }
}

