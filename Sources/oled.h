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
#ifndef __OLED_H__
#define __OLED_H__

#include "config.h"

void OLED_Init(void);
void OLED_Off(void);
void OLED_Clear(void);
void OLED_ShowBigStringStep(uint8_t x, uint8_t page, const char *str, uint8_t step);
void OLED_ShowBigNum6Step(uint8_t x, uint8_t page, uint32_t num, uint8_t step);
// 16x16 Chinese character display
void OLED_ShowCN16(uint8_t x, uint8_t page, uint8_t idx);
// 两个字紧贴（步长 = 字框 16px），用于拼合宽图形
void OLED_ShowCN16x2(uint8_t x, uint8_t page, uint8_t idx1, uint8_t idx2);
// 1 = 第二行大字整体下移 1px（上下留白更均匀）；0 = 不下移。
// 注意：不再对像素做行屏蔽，否则会裁掉汉字最下面一行。
void OLED_RowGap(uint8_t on);

#endif

