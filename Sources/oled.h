#ifndef __OLED_H__
#define __OLED_H__

#include "config.h"

void OLED_Init(void);
void OLED_Off(void);
void OLED_Clear(void);
void OLED_ShowBigString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowBigNum6(uint8_t x, uint8_t page, uint32_t num);
// 16x16 Chinese character display
void OLED_ShowCN16(uint8_t x, uint8_t page, uint8_t idx);
void OLED_ShowCN16x2(uint8_t x, uint8_t page, uint8_t idx1, uint8_t idx2);

#endif

