#include "config.h"
#include "oled.h"
#include "font_cn_16.h"
#include "random.h"
#include "adc.h"
#include "display.h"
#include "eeprom.h"
#include "bitmap.h"
#include "keys.h"

static uint8_t code *GetBitmap(uint16_t prefix)
{
    switch (prefix) {
        case 600: return bitmap_600;
        case 601: return bitmap_601;
        case 603: return bitmap_603;
        case 605: return bitmap_605;
        case 688: return bitmap_688;
        case 0:   return bitmap_000;
        case 1:   return bitmap_001;
        case 2:   return bitmap_002;
        case 3:   return bitmap_003;
        case 300: return bitmap_300;
        case 301: return bitmap_301;
        case 920: return bitmap_920;
        default:  return NULL;
    }
}

static uint8_t BitmapCheck(uint8_t code *bmp, uint16_t suffix)
{
    uint8_t byte_idx = suffix >> 3;
    uint8_t bit_idx  = suffix & 0x07;
    return (bmp[byte_idx] >> bit_idx) & 0x01;
}

static uint16_t Rand16(void)
{
    return (uint16_t)FastRand() | ((uint16_t)FastRand() << 8);
}

static uint16_t RandPrefix(void)
{
    // Weighted random based on stock count per prefix
    // 600(875) 601(235) 603(627) 605(113) 688(621)
    // 000(506) 001(122) 002(977) 003(42) 300(979) 301(471) 920(346)
    // Total: 5914
    uint16_t r = Rand16() % 5914;
    if (r < 875)  return 600;
    r -= 875;
    if (r < 235)  return 601;
    r -= 235;
    if (r < 627)  return 603;
    r -= 627;
    if (r < 113)  return 605;
    r -= 113;
    if (r < 621)  return 688;
    r -= 621;
    if (r < 506)  return 0;
    r -= 506;
    if (r < 122)  return 1;
    r -= 122;
    if (r < 977)  return 2;
    r -= 977;
    if (r < 42)   return 3;
    r -= 42;
    if (r < 979)  return 300;
    r -= 979;
    if (r < 471)  return 301;
    return 920;
}

static void SelectStock(void)
{
    uint16_t prefix;
    uint8_t code *bitmap;
    uint8_t tries = 0;
    uint16_t suffix;
    uint32_t stock_val;

    prefix = RandPrefix();
    bitmap = GetBitmap(prefix);
    if (bitmap == NULL) {
        DisplayMsg("ERR");
        delay_ms(1500);
        return;
    }

    do {
        suffix = (uint16_t)FastRand() | ((uint16_t)FastRand() << 8);
        suffix = suffix % 1000;
        tries++;
        if (tries > 50) {
            // Fallback: just use whatever we got
            suffix = 1;
            break;
        }
    } while (!BitmapCheck(bitmap, suffix));

    stock_val = (uint32_t)prefix * 1000 + suffix;
    DisplayStock(stock_val);
    delay_or_key_ms(1500);
}

// Randomly select one of 9 trading actions
// Actions: ?? ?? ?? ?? ?? ?? ?? ?? ??
static void SelectTradeAction(void)
{
    uint8_t idx;
    idx = FastRand() % 9;

    DisplayTradeAction(idx);
    delay_or_key_ms(1500);
}

void ExecuteSelect(void)
{
    if (!IsLightSufficient()) {
        OLED_Clear();
        // "??????" 5 chars x 16px = 80px, centered: (128-80)/2 = 24
        OLED_ShowCN16x2(24, 0, F16_QING, F16_ZHAN);
        OLED_ShowCN16x2(56, 0, F16_ZAI, F16_GUANG);
        OLED_ShowCN16(88, 0, F16_LI);
        delay_ms(2000);
        return;
    }

    // Re-seed RNG with fresh light entropy before each selection
    ReSeedFromLight();

    // Execute based on current mode
    if (current_mode == MODE_TRADE) {
        SelectTradeAction();
    } else {
        SelectStock();
    }

    // Show "??+1"
    DisplayMeritAdd();
    delay_or_key_ms(500);

    light_merit++;
    SaveMeritDeferred();
}
