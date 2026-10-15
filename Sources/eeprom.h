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
#ifndef __EEPROM_H__
#define __EEPROM_H__

#include "config.h"

#define MODE_STOCK  0
#define MODE_TRADE  1

extern uint32_t light_merit;
extern uint8_t  current_mode;

void LoadPersistent(void);
void SavePersistent(void);
void SaveMeritDeferred(void);
void FlushSave(void);

#endif

