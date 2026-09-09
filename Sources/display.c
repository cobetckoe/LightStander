#include "config.h"
#include "oled.h"
#include "eeprom.h"
#include "font_cn_16.h"

// Screen: 128x32 pixels, 4 pages (page 0-3)
// Big font: 10x16 pixels per char, 12px width with gap
// CN16 font: 16x16 pixels per char, takes 2 pages

void DisplayStandby(void)
{
    OLED_Clear();
    if (current_mode == MODE_TRADE) {
        // "意外操盘" 4 CN chars x 16px = 64px, centered: (128-64)/2 = 32
        OLED_ShowCN16x2(32, 0, F16_YI, F16_WAI);
        OLED_ShowCN16x2(64, 0, F16_CAO, F16_PAN);
    } else {
        // "随机选股" 4 CN chars x 16px = 64px, centered: (128-64)/2 = 32
        OLED_ShowCN16x2(32, 0, F16_SUI, F16_JI2);
        OLED_ShowCN16x2(64, 0, F16_XUAN, F16_GU);
    }
}

void DisplayMerit(uint32_t merit)
{
    OLED_Clear();
    // "光功德" 3 CN chars x 16px = 48px, centered: (128-48)/2 = 40
    OLED_ShowCN16x2(40, 0, F16_GUANG, F16_GONG);
    OLED_ShowCN16(72, 0, F16_DE);
    // 6 digits 72px centered: x=28
    OLED_ShowBigNum6(28, 2, merit);
}

void DisplayStock(uint32_t stock_code)
{
    OLED_Clear();
    // "随机选股" 4 CN chars x 16px = 64px, centered: x=32
    OLED_ShowCN16x2(32, 0, F16_SUI, F16_JI2);
    OLED_ShowCN16x2(64, 0, F16_XUAN, F16_GU);
    // Stock code on bottom: 6 digits x 12px = 72px, centered: (128-72)/2 = 28
    OLED_ShowBigNum6(28, 2, stock_code);
}

void DisplayMeritAdd(void)
{
    OLED_Clear();
    // "光功德" 48px + "+1" 24px = 72px, centered: (128-72)/2 = 28
    OLED_ShowCN16x2(28, 0, F16_GUANG, F16_GONG);
    OLED_ShowCN16(60, 0, F16_DE);
    OLED_ShowBigString(76, 0, "+1");
}

void DisplayMsg(const char *msg)
{
    OLED_Clear();
    // Generic centered message (up to 8 chars)
    OLED_ShowBigString(34, 1, msg);
}

// 9 trading actions: 补仓 空仓 平仓 减仓 半仓 建仓 加仓 持仓 满仓
// Each action is 2 Chinese chars = 32px, centered: (128-32)/2 = 48
static const uint8_t code tradePairs[9][2] = {
    { F16_BU2,  F16_CANG },  // 0: 补仓
    { F16_KONG, F16_CANG },  // 1: 空仓
    { F16_PING, F16_CANG },  // 2: 平仓
    { F16_JIAN, F16_CANG },  // 3: 减仓
    { F16_BAN,  F16_CANG },  // 4: 半仓
    { F16_JIAN2,F16_CANG },  // 5: 建仓
    { F16_JIA,  F16_CANG },  // 6: 加仓
    { F16_CHI,  F16_CANG },  // 7: 持仓
    { F16_MAN,  F16_CANG },  // 8: 满仓
};

void DisplayTradeAction(uint8_t idx)
{
    if (idx >= 9) idx = 0;
    OLED_Clear();
    // Action name centered on top: (128-32)/2 = 48
    OLED_ShowCN16x2(48, 0, tradePairs[idx][0], tradePairs[idx][1]);
}
