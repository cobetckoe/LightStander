#include "config.h"
#include "adc.h"
#include "oled.h"
#include "random.h"
#include "eeprom.h"
#include "modes.h"
#include "keys.h"
#include "display.h"
#include "stop.h"

void main(void)
{
    SYS_Init();
    OLED_Init();
    LoadPersistent();
    InitRandomSeed();
    EnableKeyInterrupt();

    // Boot splash: show PTS value
    DisplayMerit(light_merit);
    delay_ms(500);
    DisplayStandby();

    while (1) {
        EnterStopMode();
        // Woke up from button
        DisplayMerit(light_merit);
        delay_ms(500);
        DisplayStandby();
        // Active loop: wait for user action
        while (1) {
            if (ProcessKey()) break;  // executed selection, back to sleep
        }
    }
}

void SYS_Init(void)
{
    // GPIO config
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x08;                 // P3.3 high-Z for ADC
    P5M0 = 0x30; P5M1 = 0x30;   // P5.4/P5.5 open-drain for I2C (bit4=4, bit5=5)

    ADC_Init();
}

void delay_ms(uint16_t ms)
{
    uint16_t i;
    do {
        i = MAIN_Fosc / 10000;
        while (--i);
    } while (--ms);
}
