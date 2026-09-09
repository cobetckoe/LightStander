#include "config.h"
#include "STC8G_H_ADC.h"

// ADC channels
#define ADC_CH_LIGHT    3       // P3.3 = ADC3 on STC8G1K08A
#define ADC_CH_BANDGAP  15      // Internal 1.19V reference

// Light threshold (compensated units, 0..4095)
// Sunlight ? 2000+, fluorescent ? 500-800
#define LIGHT_THRESHOLD_ON   3096

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
// Uses internal bandgap ADC to compensate for battery voltage variation
// and applies simple hysteresis to avoid flicker near the threshold.
uint8_t IsLightSufficient(void)
{
    uint32_t light_sum = 0;
    uint32_t bg_sum = 0;
    uint8_t i;
    uint16_t light_avg, bg_avg;
    uint32_t compensated;

    // Average multiple samples for stability
    for (i = 0; i < 8; i++) {
        light_sum += ReadLightADC();
    }
    for (i = 0; i < 8; i++) {
        bg_sum += ReadBandgapADC();
    }

    light_avg = (uint16_t)(light_sum / 8);
    bg_avg = (uint16_t)(bg_sum / 8);
    if (bg_avg == 0) bg_avg = 1; // avoid div0

    // Compensate light reading by bandgap: normalized to 0..4095
    compensated = ((uint32_t)light_avg * 4095U) / (uint32_t)bg_avg;

    return (compensated > LIGHT_THRESHOLD_ON) ? 1 : 0;
}
