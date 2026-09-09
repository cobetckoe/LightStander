/* 16x16 Chinese font for LightStander */
#ifndef __F16_H__
#define __F16_H__

#include "config.h"

#define F16_W 16
#define F16_H 16
#define F16_B 32

// 股 请 站 在 光 里 不 足 补 空 平 减 半 建 加 持 满 仓 随 机 选 意 外 操 盘 功 德
enum { F16_GU, F16_QING, F16_ZHAN, F16_ZAI, F16_GUANG, F16_LI, F16_BU, F16_ZU,
       F16_BU2, F16_KONG, F16_PING, F16_JIAN, F16_BAN, F16_JIAN2, F16_JIA,
       F16_CHI, F16_MAN, F16_CANG,
       F16_SUI, F16_JI2, F16_XUAN, F16_YI, F16_WAI, F16_CAO, F16_PAN,
       F16_GONG, F16_DE,
       F16_CNT };

extern code const uint8_t F16Dat[F16_CNT][F16_B];

#endif