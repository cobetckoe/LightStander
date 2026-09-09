#ifndef __ADC_H__
#define __ADC_H__

#include "config.h"

void    ADC_Init(void);
uint16_t ReadLightADC(void);
uint8_t  IsLightSufficient(void);

#endif
