#ifndef __RANDOM_H__
#define __RANDOM_H__

#include "config.h"

void    InitRandomSeed(void);void ReSeedFromLight(void);uint8_t FastRand(void);
uint8_t RandRange(uint8_t min_val, uint8_t max_val);

#endif

