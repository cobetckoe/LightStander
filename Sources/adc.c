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
// Uses internal bandgap ADC to compensate for battery voltage variation.
uint8_t IsLightSufficient(void)
{
    uint32_t light_sum = 0;
    uint32_t bg_sum = 0;
    uint8_t i;
    uint32_t compensated;

    for (i = 0; i < 4; i++) {
        light_sum += ReadLightADC();
        bg_sum += ReadBandgapADC();
    }

    if (bg_sum == 0) bg_sum = 1;
    compensated = ((uint32_t)light_sum * 4095U) / bg_sum;

    return (compensated > LIGHT_THRESHOLD_ON) ? 1 : 0;
}

// Check battery voltage via bandgap reference.
// VDD = 1.19V * 4096 / ADC_BG. Lower VDD => higher ADC_BG.
// Returns: 0=OK, 1=low battery (VDD < 3.3V)
uint8_t CheckBattery(void)
{
    uint16_t bg = ReadBandgapADC();
    // VDD = 1.19*4096/bg. Low at VDD<3.3V => bg>1477
    return (bg > 1477U) ? 1 : 0;
}
