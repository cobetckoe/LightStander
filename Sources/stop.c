#include "config.h"
#include "eeprom.h"
#include "adc.h"
#include "oled.h"
#include "display.h"

void EnterStopMode(void)
{
    // Close peripherals to save power
    // Flush any pending EEPROM writes before powering down
    FlushSave();

    ADC_CONTR = 0;
    OLED_Off();

    // All pins quasi-bidirectional for low power
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00; P3M1 = 0x00;
    P5M0 = 0x00; P5M1 = 0x00;

    // Enable INT0 wake-up
    IT0 = 1;
    EX0 = 1;
    EA  = 1;

    // Enter STOP mode
    PCON |= 0x02;
    _nop_();
    _nop_();

    // === MCU wakes up here ===

    // Re-init pins
    P1M0 = 0x00; P1M1 = 0x00;
    P3M0 = 0x00; P3M1 = 0x08;  // P3.3 high-Z for ADC
    P5M0 = 0x30; P5M1 = 0x30;  // P5.4/P5.5 open-drain for I2C

    // Wait for button release before heavy init
    // P3.2 is also ISP pin - must be HIGH before any reset
    // Prevents power collapse during OLED init while button held
    delay_ms(200);
    while (!P32);       // wait until button released
    delay_ms(50);       // debounce

    ADC_Init();
    OLED_Init();
    DisplayStandby();
}

