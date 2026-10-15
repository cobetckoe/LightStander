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
#include "font_cn_16.h"

// Screen: 128x32 pixels, 4 pages (page 0-3)
// Big font: 10x16 pixels per glyph (5 source columns doubled); CN16: 16x16, spans 2 pages

// ============================================================================
//  Layout guide -- READ THIS BEFORE CHANGING ANY COORDINATE
// ----------------------------------------------------------------------------
//  The 16px CN cell is a frame; the real ink is only 13..15px wide
//  (measured with Tools/inspect_font.py):
//      15px  most glyphs (GU JI XUAN GUANG DE QING ZHAN ZAI LI KONG ...)
//      14px  GONG/WAI/JIA (ink cols 0..13); SUI (ink cols 1..14)
//      13px  BU (ink cols 1..13)
//      10px  digits 0..9 and '+' (5-column glyph doubled horizontally)
//       6px  digit 1 (middle 3 columns, ink cols 2..7)
//  So do NOT place glyphs by a fixed frame pitch: narrow glyphs would get
//  oversized gaps ("some characters look further apart"), and that is also
//  why the '+' sign used to look asymmetric.
//  Every coordinate below is placed by measured ink so that:
//    1) adjacent ink gaps are constant inside a row (5 or 7px, see below);
//    2) both rows occupy the SAME range 24..103 (80px, centred) so the
//       left and right edges line up exactly.
// ============================================================================

#define CN_ROW_X   24      // common left ink edge of both rows
#define NUM_STEP   14      // 6-digit pitch => 5*14+10 = 80px, as CN row

static void ShowQuestion(void)
{
    // 4 CN chars, constant 7px ink gap, span 24..103:
    //   YI(15) WAI(14) CAO(15) PAN(15) = 59px + 3*7 = 80px
    if (current_mode == MODE_TRADE) {
        OLED_ShowCN16(24, 0, F16_YI);    // ink 24..38
        OLED_ShowCN16(46, 0, F16_WAI);   // ink 46..59
        OLED_ShowCN16(67, 0, F16_CAO);   // ink 67..81
        OLED_ShowCN16(89, 0, F16_PAN);   // ink 89..103
    } else {
        //   SUI(14, ink 1..14) JI(15) XUAN(15) GU(15) = 59px + 3*7 = 80px
        OLED_ShowCN16(23, 0, F16_SUI);   // ink 24..37
        OLED_ShowCN16(45, 0, F16_JI2);   // ink 45..59
        OLED_ShowCN16(67, 0, F16_XUAN);  // ink 67..81
        OLED_ShowCN16(89, 0, F16_GU);    // ink 89..103
    }
}

// Standby page: question row only (random stock / trade action)
void DisplayStandby(void)
{
    OLED_RowGap(0);
    OLED_Clear();
    ShowQuestion();
}

// Stock result page: row 1 question, row 2 the 6-digit code
void DisplayStock(uint32_t stock_code)
{
    OLED_Clear();
    OLED_RowGap(1);         // shift row 2 down 1px so margins look even
    ShowQuestion();         // spans 24..103
    OLED_ShowBigNum6Step(CN_ROW_X, 2, stock_code, NUM_STEP);  // ink 24..103
}

// Last page: row 1 "GUANG GONG DE +1", row 2 total merit
void DisplayMeritAdd(uint32_t merit)
{
    OLED_Clear();
    OLED_RowGap(1);         // shift row 2 down 1px so margins look even
    // 5 glyphs, constant 5px ink gap, span 24..103:
    //   GUANG(15) GONG(14) DE(15) +(10) 1(6) = 60px + 4*5 = 80px
    OLED_ShowCN16(24, 0, F16_GUANG);   // ink 24..38
    OLED_ShowCN16(44, 0, F16_GONG);    // ink 44..57
    OLED_ShowCN16(63, 0, F16_DE);      // ink 63..77
    // '+' ink 83..92: 5px gap on the left, 5px on the right (symmetric)
    // '1' ink is only 6px at cols 2..7, so pitch 13 -> ink 98..103
    OLED_ShowBigStringStep(83, 0, "+1", 13);
    // digit pitch 14 => 6 digits span 24..103, aligned with row above
    OLED_ShowBigNum6Step(CN_ROW_X, 2, merit, NUM_STEP);
}

// Low battery: show the battery icon centred for about 1.2s
void DisplayLowBattery(void)
{
    OLED_RowGap(0);
    OLED_Clear();
    // 32x16 battery icon = two 16px halves butted together (pitch 16).
    // ink is 27px wide: x=48 => 50..76, horizontally centred.
    OLED_ShowCN16x2(48, 0, F16_BAT1, F16_BAT2);
    delay_ms(1200);
}

// 9 trading actions: BU KONG PING JIAN BAN JIAN2 JIA CHI MAN, each + CANG
// Each action is 2 Chinese chars (16px cell each, spans pages 2-3)
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

// Each pair placed by measured ink width for a constant 4px gap, centred.
// 1st-char ink width: BU 13, others 15, JIA 14; 2nd char is CANG (15)
static code const uint8_t act_x[9][2] = {
    { 47, 65 },  // 0: BU (ink 13px at cols 1..13, 1px pad on the left)
    { 47, 66 },  // 1: KONG
    { 47, 66 },  // 2: PING
    { 47, 66 },  // 3: JIAN
    { 47, 66 },  // 4: BAN
    { 47, 66 },  // 5: JIAN2
    { 47, 65 },  // 6: JIA (ink 14px)
    { 47, 66 },  // 7: CHI
    { 47, 66 },  // 8: MAN
};

void DisplayTradeAction(uint8_t idx)
{
    if (idx >= 9) idx = 0;
    OLED_Clear();
    OLED_RowGap(1);         // shift row 2 down 1px so margins look even
    ShowQuestion();
    OLED_ShowCN16(act_x[idx][0], 2, tradePairs[idx][0]);
    OLED_ShowCN16(act_x[idx][1], 2, tradePairs[idx][1]);
}
