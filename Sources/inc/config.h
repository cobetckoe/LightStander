//<<AICUBE_USER_HEADER_REMARK_BEGIN>>
////////////////////////////////////////
// 在此添加用户文件头说明信息  
// 文件名称: config.h
////////////////////////////////////////
//<<AICUBE_USER_HEADER_REMARK_END>>


#ifndef __CONFIG_H__
#define __CONFIG_H__


//<<AICUBE_USER_DEFINE_BEGIN>>
// 在此添加用户宏定义  
//<<AICUBE_USER_DEFINE_END>>


#define __ENCODING              "GB2312" //DO NOT DELETE or MODIFY


#define HIRC                    24000000UL
#define FOSC                    24000000UL
#define SYSCLK                  FOSC
#define MAIN_Fosc               FOSC

#include <stc8h.h>
#include <intrins.h>

#include "def.h"


//<<AICUBE_USER_INCLUDE_BEGIN>>
// 开机自检页开关：置 1 时开机先显示光照/带隙的原始 ADC 值约 3 秒。
// 只在排查硬件或标定阈值时打开；量产必须置 0，否则每次开机都多等 3 秒。
#define ADC_SELFTEST    0
//<<AICUBE_USER_INCLUDE_END>>





void SYS_Init(void);
void delay_ms(uint16_t ms);



//<<AICUBE_USER_EXTERNAL_DECLARE_BEGIN>>
// 在此添加用户外部函数和外部变量声明  
//<<AICUBE_USER_EXTERNAL_DECLARE_END>>


#endif
