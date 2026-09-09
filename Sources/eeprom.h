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

