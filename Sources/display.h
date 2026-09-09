#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "config.h"

void DisplayStandby(void);
void DisplayMerit(uint32_t merit);
void DisplayStock(uint32_t stock_code);
void DisplayMeritAdd(void);
void DisplayMsg(const char *msg);
void DisplayTradeAction(uint8_t idx);

#endif
