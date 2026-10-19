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
#include "eeprom.h"
#include "adc.h"
#include "keys.h"
#include "font_cn_16.h"

// 屏 128x32，4 页。大字 10x16（5 列字模横向放大 2 倍），汉字 16x16 占两页。

// ============================================================================
//  坐标怎么定
// ----------------------------------------------------------------------------
//  16px 字框是虚的，实际墨迹只有 13~15px：
//      15px  大多数汉字
//      14px  GONG / WAI / JIA / SUI
//      13px  BU
//      10px  数字 0~9 和 '+'
//       6px  数字 1
//  所以别按固定字框步长去排，窄字会留出明显偏大的缝（以前 '+' 看着
//  不对称就是这个原因）。下面每个坐标都按实测墨迹摆，保证：
//    1) 同一行内相邻墨迹间距一致（5px 或 7px）；
//    2) 两行都落在 24..103 这 80px 内，左右边缘对齐。
// ============================================================================

#define CN_ROW_X   24      // 两行共同的左墨迹边缘
#define NUM_STEP   14      // 6 位数字步长 => 5*14+10 = 80px，与汉字行等宽

static void ShowQuestion(void)
{
    // 4 个汉字，墨迹间距固定 7px，占 24..103
    if (current_mode == MODE_TRADE) {
        OLED_ShowCN16(24, 0, F16_YI);
        OLED_ShowCN16(46, 0, F16_WAI);
        OLED_ShowCN16(67, 0, F16_CAO);
        OLED_ShowCN16(89, 0, F16_PAN);
    } else {
        OLED_ShowCN16(23, 0, F16_SUI);
        OLED_ShowCN16(45, 0, F16_JI2);
        OLED_ShowCN16(67, 0, F16_XUAN);
        OLED_ShowCN16(89, 0, F16_GU);
    }
}

// 待机页，只有问题行
void DisplayStandby(void)
{
    OLED_RowGap(0);
    OLED_Clear();
    ShowQuestion();
}

// 结果页：上排问题，下排 6 位代码
void DisplayStock(uint32_t stock_code)
{
    OLED_Clear();
    OLED_RowGap(1);         // 第二行下移 1px，上下留白才均匀
    ShowQuestion();
    OLED_ShowBigNum6Step(CN_ROW_X, 2, stock_code, NUM_STEP);
}

// 末页：上排「光功德 +1」，下排累计值
void DisplayMeritAdd(uint32_t merit)
{
    OLED_Clear();
    OLED_RowGap(1);
    OLED_ShowCN16(24, 0, F16_GUANG);
    OLED_ShowCN16(44, 0, F16_GONG);
    OLED_ShowCN16(63, 0, F16_DE);
    // '+' 墨迹 83..92，两侧各留 5px；'1' 只有 6px 宽，步长 13 正好到 103
    OLED_ShowBigStringStep(83, 0, "+1", 13);
    OLED_ShowBigNum6Step(CN_ROW_X, 2, merit, NUM_STEP);
}

// 低电量：电池图标居中显示约 1.2 秒
void DisplayLowBattery(void)
{
    OLED_RowGap(0);
    OLED_Clear();
    // 32x16 图标由两个 16px 拼成，墨迹 27px 宽，x=48 时居中
    OLED_ShowCN16x2(48, 0, F16_BAT1, F16_BAT2);
    delay_ms(1200);
}

// 9 个操盘动作，都是「X 仓」两个字
static code const uint8_t tradePairs[9][2] = {
    { F16_BU2,  F16_CANG },  // 0: BU CANG
    { F16_KONG, F16_CANG },  // 1: KONG CANG
    { F16_PING, F16_CANG },  // 2: PING CANG
    { F16_JIAN, F16_CANG },  // 3: JIAN CANG
    { F16_BAN,  F16_CANG },  // 4: BAN CANG
    { F16_JIAN2, F16_CANG }, // 5: JIAN2 CANG
    { F16_JIA,  F16_CANG },  // 6: JIA CANG
    { F16_CHI,  F16_CANG },  // 7: CHI CANG
    { F16_MAN,  F16_CANG },  // 8: MAN CANG
};

// 按实测墨迹宽度摆位，两字之间固定 4px，整体居中
static code const uint8_t act_x[9][2] = {
    { 47, 65 },  // 0: BU，墨迹 13px，左边留 1px
    { 47, 66 },  // 1: KONG
    { 47, 66 },  // 2: PING
    { 47, 66 },  // 3: JIAN
    { 47, 66 },  // 4: BAN
    { 47, 66 },  // 5: JIAN2
    { 47, 65 },  // 6: JIA，墨迹 14px
    { 47, 66 },  // 7: CHI
    { 47, 66 },  // 8: MAN
};

void DisplayTradeAction(uint8_t idx)
{
    if (idx >= 9) idx = 0;
    OLED_Clear();
    OLED_RowGap(1);
    ShowQuestion();
    OLED_ShowCN16(act_x[idx][0], 2, tradePairs[idx][0]);
    OLED_ShowCN16(act_x[idx][1], 2, tradePairs[idx][1]);
}

// ---------------------------------------------------------------------------
// 开机自检页（仅 ADC_SELFTEST=1 时编译）
// 上排 = 光照 CH3 原始值，下排 = 带隙 CH15 原始值
//
// 本机实测（10 位，满量程 1023）：
//   强光直射   light=1023（饱和）  bg=369
//   完全遮挡   light= 867          bg=367
//   → ADC 是 10 位；bg=369 算得 VDD≈3.30V，按 12 位算则是 13.2V，不可能
//   → LDR 在上臂：光越强读数越大，与下臂接法的单调性相反
//
// 重新标定时看两行数字：
//   · 上排强光明显大于遮挡        正常
//   · 两值都在 1010 以上        分压比失衡，摆幅挤没了
//   · 两值都在 50 以下          P3.3 对地短路
//   · 两行恒为 0               ADC 没跑起来，先查 ADC_Init()
// 带隙正常范围：VDD 3.3V 约 369，3.7V 约 330。
// ---------------------------------------------------------------------------
#if ADC_SELFTEST
void DisplaySelftest(void)
{
    uint8_t k;
    OLED_RowGap(1);
    OLED_Clear();
    OLED_ShowBigNum6Step(CN_ROW_X, 0, (uint32_t)ReadLightADC(),   NUM_STEP);
    OLED_ShowBigNum6Step(CN_ROW_X, 2, (uint32_t)ReadBandgapADC(), NUM_STEP);
    // 最多显示 3 秒，按一下按键可立即跳过
    for (k = 0; k < 30; k++) {
        if (!P32) break;
        delay_ms(100);
    }
    WaitButtonRelease();
}
#endif
