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
#ifndef __KEYS_H__
#define __KEYS_H__

#include "config.h"

void EnableKeyInterrupt(void);
uint8_t ProcessKey(void);   // 0=stay active, 1=back to sleep
void WaitButtonRelease(void);   // 等待按键释放（带上限，防卡死）
void ClearKeyPending(void);     // 丢弃未处理的按键事件并清 IE0

#endif
