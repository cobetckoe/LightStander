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

// ADC 通道
#define ADC_CH_LIGHT    3       // P3.3 = ADC3
#define ADC_CH_BANDGAP  15      // 内部 1.19V 基准

// 本机 ADC 是 10 位，满量程 1023。带隙读数 369 可以印证：
//   10 位：VDD = 1.19 * 1023 / 369 ≈ 3.30V  合理
//   12 位：VDD = 1.19 * 4095 / 369 ≈ 13.2V  不可能
// 强光读数又正好停在 1023（2^10-1），所以按 4095 标定的常量一律除以 4。
#define ADC_INVALID      1024U  // 无效下界，超时哨兵 4096 也落在这里

// 光照门禁，判 raw 值不判 comp：
//   强光下分压顶到 1023 饱和，comp = 1023*1023/bg 就只由带隙决定，
//   bg 抖几个 LSB 结果就越阈值，这也是以前阈值怎么调都卡不住的原因。
//   饱和点本身是相对 VDD 的，电池从 3.3V 掉到 2.9V 强光照样读 1023，
//   所以 raw 判据天然与电池电压无关。
//
// 本机实测：遮挡 867，微弱光约 950，强光 1023。
// 1015 只比满量程低 8 个 LSB，室内、阴天、树荫都达不到，
// 只有直射阳光才行。反推 raw=1015 → V_P3.3≈3.27V → LDR 约 1.2K。
// 要放宽收紧只改这一个数：raw = 1023 * 100K / (100K + LDR)。
#define LIGHT_SUN_ADC    1015U

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

// 只认「晒到太阳」这一档：raw 必须接近满量程。
uint8_t IsLightSufficient(void)
{
    uint16_t sum = 0;
    uint16_t lavg;
    uint8_t i, n = 0;   // n = 实际保留的有效采样数

    // 丢掉切换通道后的首读：采样保持电容还带着上一通道的电荷
    (void)ReadLightADC();

    // 10 位有效范围 1~1023；0 是死通道，>=1024 是超时哨兵，都剔掉
    for (i = 0; i < 4; i++) {
        uint16_t lv = ReadLightADC();
        if (lv == 0 || lv >= ADC_INVALID) continue;
        sum += lv;
        n++;
    }

    // 一次有效转换都没拿到就放行。测不准就不拦人，总比把设备
    // 锁死在「请站在光里」好。
    if (n == 0) return 1;

    lavg = (uint16_t)(sum / n);

    // 读数被拉到地，说明光敏电阻短路或引脚接地，数据不可信，放行
    if (lavg <= 45U) return 1;

    return (lavg >= LIGHT_SUN_ADC) ? 1 : 0;
}

// 用带隙基准反推电池电压：VDD = 1.19 * 1023 / bg，VDD 越低 bg 越高。
//
// 取多次采样的最小值，不能遇到第一个有效值就 break。刚切过 CH3 再切
// 到 CH15 时首读常常偏高，只要有一次毛刺就会被当成真亏电，然后关屏。
// 取最小值等于取最乐观的 VDD，真亏电时 5 次都高，保护照样生效。
//
// 阈值：bg = 1217 / VDD。实测 bg=369 → VDD≈3.30V。
// 取 VDD < 2.90V 报警，即 bg ≈ 420。定这么低是因为误报的直接后果
// 就是黑屏，宁可漏报不可误报。
#define BAT_LOW_ADC     420U    // 对应 VDD < 约 2.90V

uint8_t CheckBattery(void)
{
    uint8_t i, n = 0;
    uint16_t v, bg = 0xFFFFU;

    (void)ReadBandgapADC();     // 丢弃通道切换后的首读

    for (i = 0; i < 5; i++) {
        v = ReadBandgapADC();
        if (v == 0 || v >= ADC_INVALID) continue;  // 超时哨兵 / 死通道
        if (v < bg) bg = v;
        n++;
    }

    // 一次都测不到时不关机，否则一次转换异常就能让设备永远黑屏
    if (n == 0) return 0;

    return (bg > BAT_LOW_ADC) ? 1 : 0;
}
