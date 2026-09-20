#ifndef __KEYS_H__
#define __KEYS_H__

#include "config.h"

void EnableKeyInterrupt(void);
uint8_t ProcessKey(void);   // 0=stay active, 1=back to sleep


#endif
