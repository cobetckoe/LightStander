#include "config.h"
#include "font_cn_16.h"

// I2C pins: P5.5 = SCL, P5.4 = SDA (swapped for test)
#define SCL     P55
#define SDA     P54

#define OLED_ADDR   0x78

// Compact font: only used characters (space, +, :, 0-9, A-Z)
// 39 chars × 5 bytes = 195 bytes (vs 475 bytes for full ASCII)
static code const uint8_t Font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (space) index 0
    {0x08,0x08,0x3E,0x08,0x08}, // '+'         index 1
    {0x00,0x36,0x36,0x00,0x00}, // ':'         index 2
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'         index 3
    {0x00,0x42,0x7F,0x40,0x00}, // '1'         index 4
    {0x42,0x61,0x51,0x49,0x46}, // '2'         index 5
    {0x21,0x41,0x45,0x4B,0x31}, // '3'         index 6
    {0x18,0x14,0x12,0x7F,0x10}, // '4'         index 7
    {0x27,0x45,0x45,0x45,0x39}, // '5'         index 8
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'         index 9
    {0x01,0x71,0x09,0x05,0x03}, // '7'         index 10
    {0x36,0x49,0x49,0x49,0x36}, // '8'         index 11
    {0x06,0x49,0x49,0x29,0x1E}, // '9'         index 12
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'         index 13
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'         index 14
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'         index 15
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'         index 16
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'         index 17
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'         index 18
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'         index 19
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'         index 20
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'         index 21
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'         index 22
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'         index 23
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'         index 24
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'         index 25
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'         index 26
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'         index 27
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'         index 28
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'         index 29
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'         index 30
    {0x46,0x49,0x49,0x49,0x31}, // 'S'         index 31
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'         index 32
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'         index 33
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'         index 34
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'         index 35
    {0x63,0x14,0x08,0x14,0x63}, // 'X'         index 36
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'         index 37
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'         index 38
};

// Character to font index mapping
static uint8_t CharToIndex(char c)
{
    if (c == ' ') return 0;
    if (c == '+') return 1;
    if (c == ':') return 2;
    if (c >= '0' && c <= '9') return 3 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 13 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 13 + (c - 'a');  // lowercase maps to uppercase
    return 0;
}

// ---- Software I2C ----

static void I2C_Start(void)
{
    SDA = 1; SCL = 1; _nop_(); _nop_();
    SDA = 0; _nop_(); _nop_();
    SCL = 0;
}

static void I2C_Stop(void)
{
    SDA = 0; _nop_(); _nop_();
    SCL = 1; _nop_(); _nop_();
    SDA = 1;
}

static void I2C_WriteByte(uint8_t dat)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;
        SCL = 1; _nop_(); _nop_();
        SCL = 0;
    }
    SDA = 1;
    SCL = 1; _nop_(); _nop_();
    SCL = 0;
}

static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x00);
    I2C_WriteByte(cmd);
    I2C_Stop();
}

static void OLED_WriteData(uint8_t dat)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x40);
    I2C_WriteByte(dat);
    I2C_Stop();
}

// ---- OLED API ----

void OLED_Init(void)
{
    delay_ms(100);
    OLED_WriteCmd(0xAE);    // Display OFF
    OLED_WriteCmd(0xD5);    // Set clock divider
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8);    // Set multiplex ratio
    OLED_WriteCmd(0x1F);    // 32-1=31 for 128x32
    OLED_WriteCmd(0xD3);    // Set display offset
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);    // Set start line = 0
    OLED_WriteCmd(0x8D);    // Charge pump
    OLED_WriteCmd(0x14);    // Enable charge pump
    OLED_WriteCmd(0x20);    // Memory addressing mode
    OLED_WriteCmd(0x02);    // Page addressing mode
    OLED_WriteCmd(0xA1);    // Segment remap: 180° rotation (COL0 = right)
    OLED_WriteCmd(0xC8);    // COM scan: 180° rotation (COM0 = bottom)
    OLED_WriteCmd(0xDA);    // COM pins config
    OLED_WriteCmd(0x02);    // 0x02 for 128x32
    OLED_WriteCmd(0x81);    // Contrast
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9);    // Pre-charge period
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB);    // VCOMH deselect level
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4);    // Display from RAM
    OLED_WriteCmd(0xA6);    // Normal display (not inverted)
    OLED_WriteCmd(0xAF);    // Display ON
}

void OLED_Off(void)
{
    OLED_WriteCmd(0xAE);    // Display OFF
    OLED_WriteCmd(0x8D);    // Charge pump
    OLED_WriteCmd(0x10);    // Disable
}

void OLED_Clear(void)
{
    uint8_t page, col;
    for (page = 0; page < 4; page++) {
        OLED_WriteCmd(0xB0 + page);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for (col = 0; col < 128; col++) {
            OLED_WriteData(0x00);
        }
    }
}

static void OLED_SetCursor(uint8_t x, uint8_t page)
{
    OLED_WriteCmd(0xB0 + page);  // Page address (0-3)
    OLED_WriteCmd(0x00 + (x & 0x0F));      // Lower column address
    OLED_WriteCmd(0x10 + ((x >> 4) & 0x0F)); // Upper column address
}

// Big font: 2x size (10x16 pixels per digit)
static void OLED_ShowBigChar(uint8_t x, uint8_t page, char c)
{
    uint8_t i;
    uint8_t idx = CharToIndex(c);
    const uint8_t *src = Font5x7[idx];

    // Top half
    OLED_SetCursor(x, page);
    for (i = 0; i < 5; i++) {
        uint8_t b = src[i];
        // Double each bit horizontally
        uint8_t out = 0;
        if (b & 0x01) out |= 0x03;
        if (b & 0x02) out |= 0x0C;
        if (b & 0x04) out |= 0x30;
        if (b & 0x08) out |= 0xC0;
        OLED_WriteData(out);
        OLED_WriteData(out);
    }

    // Bottom half
    OLED_SetCursor(x, page + 1);
    for (i = 0; i < 5; i++) {
        uint8_t b = src[i];
        uint8_t out = 0;
        if (b & 0x10) out |= 0x03;
        if (b & 0x20) out |= 0x0C;
        if (b & 0x40) out |= 0x30;
        if (b & 0x80) out |= 0xC0;
        OLED_WriteData(out);
        OLED_WriteData(out);
    }
}

void OLED_ShowBigNum6(uint8_t x, uint8_t page, uint32_t num)
{
    OLED_ShowBigChar(x,      page, '0' + (num / 100000));
    OLED_ShowBigChar(x + 12, page, '0' + ((num / 10000) % 10));
    OLED_ShowBigChar(x + 24, page, '0' + ((num / 1000) % 10));
    OLED_ShowBigChar(x + 36, page, '0' + ((num / 100) % 10));
    OLED_ShowBigChar(x + 48, page, '0' + ((num / 10) % 10));
    OLED_ShowBigChar(x + 60, page, '0' + (num % 10));
}

void OLED_ShowBigString(uint8_t x, uint8_t page, const char *str)
{
    while (*str) {
        OLED_ShowBigChar(x, page, *str);
        x += 12;
        str++;
    }
}

// 16x16 Chinese character display
// Font data: standard column order
// OLED: 0xA1 + 0xC8 (180° rotation), SetCursor is direct page mapping

// 显示16x16汉字
void OLED_ShowCN16(uint8_t x, uint8_t page, uint8_t idx)
{
    uint8_t i;
    if (idx >= F16_CNT) return;

    OLED_SetCursor(x, page);
    for (i = 0; i < 16; i++) {
        OLED_WriteData(F16Dat[idx][i]);
    }

    if (page < 3) {
        OLED_SetCursor(x, page + 1);
        for (i = 0; i < 16; i++) {
            OLED_WriteData(F16Dat[idx][16 + i]);
        }
    }
}

// Show 2 Chinese characters side by side
void OLED_ShowCN16x2(uint8_t x, uint8_t page, uint8_t idx1, uint8_t idx2)
{
    OLED_ShowCN16(x, page, idx1);
    OLED_ShowCN16(x + 16, page, idx2);
}
