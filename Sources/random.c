#include "config.h"
#include "adc.h"

static uint16_t rand_state = 0x55AA;

void InitRandomSeed(void)
{
    uint16_t seed = 0;
    uint8_t i, j;
    uint16_t adc;
    for (i = 0; i < 32; i++) {
        adc = ReadLightADC();
        seed ^= adc;
        seed = (seed << 3) | (seed >> 13);  // rotate
        for (j = 0; j < 20; j++) {
            _nop_();
        }
    }
    seed ^= 0x5AA5;
    if (seed == 0) seed = 0x1234;
    rand_state = seed;
}

// Re-seed with fresh light ADC entropy on each button press
void ReSeedFromLight(void)
{
    uint8_t i;
    uint16_t adc;
    for (i = 0; i < 8; i++) {
        adc = ReadLightADC();
        rand_state ^= adc;
        rand_state = rand_state * 0x3D + 0x27;
        _nop_(); _nop_();
    }
}

uint8_t FastRand(void)
{
    rand_state = rand_state * 0x9D65 + 0x3B7A;
    return (uint8_t)(rand_state ^ (rand_state >> 8));
}

uint8_t RandRange(uint8_t min_val, uint8_t max_val)
{
    uint16_t range;
    if (min_val >= max_val) return min_val;
    range = max_val - min_val + 1;
    return min_val + (FastRand() % range);
}

