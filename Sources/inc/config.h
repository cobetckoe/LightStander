#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MAIN_Fosc   24000000L

#include "Type_def.h"
#include "STC8H.h"
#include "def.h"
#include <intrins.h>

void SYS_Init(void);
void delay_ms(uint16_t ms);

#endif
