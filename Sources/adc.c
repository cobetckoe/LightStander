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

// Light threshold (compensated units)
//   comp = V_P3.3 / 1.19V * 4095，分压 V_P3.3 = VDD * LDR / (Rtop + LDR)
//   生产标定值 3072：采样值需接近/达到 3072 才允许执行选股或操盘。
//   对应 Rtop=2K 时 LDR≈0.54~0.84KΩ（随VDD略变）。
//   实机偏差时：在目标光照下读 comp，阈值设为略低于该值即可。
#define LIGHT_THRESHOLD_ON   3072

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

    // No usable conversion at all: report "not enough light" instead of
    // letting a garbage ratio pass the gate.
    if (n == 0) return 0;

    if (bg_sum == 0) bg_sum = 1;
    compensated = ((uint32_t)light_sum * 4095U) / bg_sum;

    return (compensated > LIGHT_THRESHOLD_ON) ? 1 : 0;
}

// Check battery voltage via bandgap reference.
// VDD = 1.19V * 4096 / ADC_BG. Lower VDD => higher ADC_BG.
// Use a slightly conservative trip point to avoid unstable operation near the edge.
uint8_t CheckBattery(void)
{
    uint8_t i;
    uint16_t bg = 0;

    // Only a conversion that actually completed is meaningful. The driver
    // returns 4096 on timeout, and 0 means a dead channel. Treating the
    // sentinel as a real reading made 4096 > 1500 look like an empty
    // battery, so the screen stayed dark and the device fell back to STOP.
    for (i = 0; i < 3; i++) {
        uint16_t v = ReadBandgapADC();
        if (v == 0 || v >= 4096U) {
            delay_ms(2);
            continue;
        }
        bg = v;
        break;
    }

    // Could not measure the battery: fail safe by NOT shutting down, so a
    // flaky conversion can never leave the user with a black screen.
    if (bg == 0) return 0;

    return (bg > 1500U) ? 1 : 0;
}
