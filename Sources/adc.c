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
#include "config.h"
#include "STC8G_H_ADC.h"

// ADC channels
#define ADC_CH_LIGHT    3       // P3.3 = ADC3 on STC8G1K08A
#define ADC_CH_BANDGAP  15      // Internal 1.19V reference

// ---- 光照门禁 ----
// 本机接线：VDD ──[LDR]── P3.3 ──[100K]── GND
//           光敏电阻在上臂，100K 在下臂。
//
//   comp = V_P3.3 / 1.19V * 4095
//   V_P3.3 = VDD * 100K / (LDR + 100K)
//
// comp 把电池电压的变化归一化掉了，所以判据与 VDD 无关。
// 光越强 LDR 越小 → P3.3 越高 → comp 越大。
// 具体阈值与判据方向见下方 LIGHT_LDR_ON_BOTTOM 段。
//
// ---------------------------------------------------------------------
// 判据方向：按实际接线二选一，改这一个宏即可
// ---------------------------------------------------------------------
// LDR_ON_BOTTOM = 1 ：VDD --[100K]-- P3.3 --[LDR]-- GND
//                     光越强，LDR 越小，P3.3 越低 → comp 越小
//                     → 判据 comp < 阈值 放行
//
// LDR_ON_BOTTOM = 0 ：VDD --[LDR]-- P3.3 --[100K]-- GND
//                     光越强，LDR 越小，P3.3 越高 → comp 越大
//                     → 判据 comp > 阈值 放行
//
// 注意：这两种接法给出的 comp 值是**互为镜像**的，同一个阈值不可能
// 同时适配。方向配错时症状恰好是一对：光越强越提示「请站在光里」，
// 而完全遮挡反而放行。若遇到这对症状，先怀疑这个宏。
//
// 本机接法：LDR 接在 VDD 与 P3.3 之间（上臂），100K 接在 P3.3 与 GND
// 之间（下臂）。实测两个症状同时成立，只有上臂接法能解释：
//   · 强光   LDR≈5K   → comp≈10800 → 旧判据 comp<4500 永久拦截
//   · 遮挡   LDR≈1M   → comp≈ 1030 → 反而放行
// 故置 0（光越强 comp 越大）。
#define LIGHT_LDR_ON_BOTTOM   0

#if LIGHT_LDR_ON_BOTTOM
// ---- LDR 在下臂：光越强 comp 越小 ----
//   强光 LDR=1K    → comp≈  126  →  通过
//   室内 LDR=10~20K→ comp≈1300~2300 → 通过
//   傍晚 LDR=100K  → comp≈ 6300  →  通过（临界）
//   遮挡 LDR=500K+ → comp≈11500+ →  拦截
//
// 定得偏高是故意的：门禁的职责是「别在黑里选」，而不是「光照不够
// 就不让玩」。宁可放过傍晚，也不要误拦室内。
#define LIGHT_THRESHOLD_ON   8000
#else
// ---- LDR 在上臂：光越强 comp 越大 ----
//   强光 LDR=1K    → comp≈11243 →  通过
//   室内 LDR=10~20K→ comp≈9500~10300 → 通过
//   傍晚 LDR=100K  → comp≈ 5678 →  通过（临界）
//   遮挡 LDR=500K+ → comp≈ 1032~1893 → 拦截
//
// 阈值 4000 对应 LDR≈180K：傍晚及更亮放行，遮挡拦截。
#define LIGHT_THRESHOLD_ON   4000
#endif

void ADC_Init(void)
{
    ADC_InitTypeDef adc;
    adc.ADC_SMPduty  = 12;
    adc.ADC_Speed    = ADC_SPEED_2X13T;
    adc.ADC_AdjResult = ADC_RIGHT_JUSTIFIED;
    adc.ADC_CsSetup  = 0;
    adc.ADC_CsHold   = 1;
    ADC_Inilize(&adc);
    ADC_PowerControl(ENABLE);
    delay_ms(10);
}

uint16_t ReadLightADC(void)
{
    return Get_ADCResult(ADC_CH_LIGHT);
}

uint16_t ReadBandgapADC(void)
{
    return Get_ADCResult(ADC_CH_BANDGAP);
}

// Determine if ambient light is sufficient to allow selection.
// Uses internal bandgap ADC to compensate for battery voltage variation.
uint8_t IsLightSufficient(void)
{
    uint32_t light_sum = 0;
    uint32_t bg_sum = 0;
    uint8_t i;
    uint32_t compensated;

    uint8_t n = 0;      // number of usable samples actually kept

    // 丢弃切换通道后的第一次转换。ADC 的采样保持电容还带着上一个
    // 通道（带隙 CH15）的电荷，切回 CH3 后首读常常是错的，甚至直接
    // 超时返回 4096。先前从没做过这一步，是「偶发判错光照」的一个来源。
    (void)ReadLightADC();

    // Skip invalid samples. Get_ADCResult() returns 4096 on timeout and
    // a 0 would be a dead channel; either value would skew the ratio.
    for (i = 0; i < 4; i++) {
        uint16_t lv = ReadLightADC();
        uint16_t bv = ReadBandgapADC();
        if (lv == 0 || lv >= 4096U) continue;
        if (bv == 0 || bv >= 4096U) continue;
        light_sum += lv;
        bg_sum += bv;
        n++;
    }

    // 一次有效转换都没有：**放行**，不再拦。
    // 原实现在这里 return 0，于是一旦 ADC 偶发失效（通道切换、电池
    // 电压临界、引脚接触不良）设备就永远卡在「请站在光里」，只能
    // 靠反复烧录碰运气——这跟用户看到的症状完全对得上。
    // 处理方式与 CheckBattery() 一致：测不准就不动作，不要把设备锁死。
    if (n == 0) return 1;

    if (bg_sum == 0) bg_sum = 1;
    compensated = ((uint32_t)light_sum * 4095U) / bg_sum;

    // ---- 传感器异常保护 ----
    // 光敏电阻开路（虚焊、引脚断路、根本没装）时，P3.3 被 100K 直接
    // 拉到 VDD，读数永远贴近满量程 4095；P3.3 直接短路到地时贴近 0。
    // 两种情况下分压网络已经不在工作区间，任何阈值判断都不可信。
    // 这里直接放行，不要因为一个没装好的传感器把整台设备锁死在
    // 「请站在光里」页面上。
    {
        uint16_t lavg = (uint16_t)(light_sum / n);
        if (lavg >= 4050U || lavg <= 45U) return 1;
    }

#if LIGHT_LDR_ON_BOTTOM
    // LDR 在下臂：光越强，comp 越小，低于阈值即光照充足
    return (compensated < LIGHT_THRESHOLD_ON) ? 1 : 0;
#else
    // LDR 在上臂：光越强，comp 越大，高于阈值即光照充足
    return (compensated > LIGHT_THRESHOLD_ON) ? 1 : 0;
#endif
}

// Check battery voltage via bandgap reference.
// VDD = 1.19V * 4096 / ADC_BG. Lower VDD => higher ADC_BG.
//
// 这里必须取多次采样的**最小值**，不能取第一个有效值。
//
// 原因：刚切过光照通道（CH3）再切到带隙（CH15）时，采样保持电容
// 还带着上一个通道的电荷，首读常常偏高。旧实现遇到第一个有效值就
// break，于是只要有一次偶发偏高，bg 就被当成真的很高 → VDD 显得
// 很低 → 误判「电量耗尽」→ OLED_Off() → 用户看到的就是黑屏。
// 而重新烧录时板子由烧写器供电、VDD 被抬高，带隙读数回落，于是
// 「重烧一次有时候就好了」。
//
// 取最小值等于取「最乐观的 VDD」：只有所有采样都偏高（真正亏电）
// 才会判低电量，单次毛刺再也关不了机。真亏电时 5 次都偏高，min
// 依然偏高，保护照样生效。
#define BAT_LOW_ADC     1550U   // 约 VDD < 3.14V 判低电量

uint8_t CheckBattery(void)
{
    uint8_t i, n = 0;
    uint16_t v, bg = 0xFFFFU;

    (void)ReadBandgapADC();     // 丢弃通道切换后的首读

    for (i = 0; i < 5; i++) {
        v = ReadBandgapADC();
        if (v == 0 || v >= 4096U) continue;    // 超时哨兵 / 死通道
        if (v < bg) bg = v;
        n++;
    }

    // 一次都测不到：fail safe 为**不关机**，
    // 否则一次转换异常就能让设备永远黑屏。
    if (n == 0) return 0;

    return (bg > BAT_LOW_ADC) ? 1 : 0;
}
