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
#include "oled.h"
#include "font_cn_16.h"

// I2C pins: P5.5 = SCL, P5.4 = SDA (swapped for test)
#define SCL     P55
#define SDA     P54

#define OLED_ADDR   0x78

// Compact font: only characters actually rendered by the firmware
// (' ', '+', '0'-'9'). '+' comes from OLED_ShowBigStringStep("+1").
// Unused letters/':' were dropped to save flash.
// 12 chars x 5 bytes = 60 bytes
static code const uint8_t Font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (space) index 0
    {0x08,0x08,0x3E,0x08,0x08}, // '+'         index 1
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'         index 2
    {0x00,0x42,0x7F,0x40,0x00}, // '1'         index 3
    {0x42,0x61,0x51,0x49,0x46}, // '2'         index 4
    {0x21,0x41,0x45,0x4B,0x31}, // '3'         index 5
    {0x18,0x14,0x12,0x7F,0x10}, // '4'         index 6
    {0x27,0x45,0x45,0x45,0x39}, // '5'         index 7
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'         index 8
    {0x01,0x71,0x09,0x05,0x03}, // '7'         index 9
    {0x36,0x49,0x49,0x49,0x36}, // '8'         index 10
    {0x06,0x49,0x49,0x29,0x1E}, // '9'         index 11
};

// Character to font index mapping (only glyphs present in Font5x7)
static uint8_t CharToIndex(char c)
{
    if (c == ' ') return 0;
    if (c == '+') return 1;
    if (c >= '0' && c <= '9') return 2 + (c - '0');
    return 0;   // unknown char -> space
}

// ---- 行间距 (row gap) ----
// 屏幕 32 行 = 两行 16px 汉字/数字正好占满，没有多余空间再挖空隙。
// 数字字模（5x7 放大 2 倍）上下各留约 2 行空白，本身就与上一行形成视觉间隔，
// 所以这里不再对像素做任何行屏蔽——屏蔽会吃掉汉字最下面一行，
// 表现就是「第一行底部少一块」。只把第二行数字整体下移 1px，使留白更均匀。
static uint8_t row_gap = 0;

void OLED_RowGap(uint8_t on)
{
    row_gap = on ? 1 : 0;
}

// ---- Software I2C ----

// Software I2C pacing. _nop_() is a single 1T instruction at 24 MHz, so
// without a delay the bus ran near 1 MHz - over the SSD1306 400 kHz
// limit - and only worked when voltage and pull-ups were ideal. Six
// iterations give tLOW ~1.5 us, i.e. SCL ~330 kHz: inside spec with
// margin, but far quicker than the first (over-cautious) attempt.
static void I2C_Dly(void)
{
    uint8_t d = 6;
    while (--d) { _nop_(); }
}
#define I2C_DLY()   I2C_Dly()

static void SWI2C_Start(void)
{
    SDA = 1; I2C_DLY();
    SCL = 1; I2C_DLY();
    SDA = 0; I2C_DLY();
    SCL = 0; I2C_DLY();
}

static void SWI2C_Stop(void)
{
    SDA = 0; I2C_DLY();
    SCL = 1; I2C_DLY();
    SDA = 1; I2C_DLY();
}

// Send one byte and read its ACK slot.
// Returns 0 when the slave pulled SDA low (acknowledged), 1 on NACK.
// A NACK means the slave did not latch the byte, so repeating just that
// byte is safe: the ACK clock resynchronises both ends.
static uint8_t I2C_WriteByte(uint8_t dat)
{
    uint8_t i, t;
    uint8_t ack = 1;

    for (t = 0; t < 2; t++) {
        uint8_t b = dat;
        for (i = 0; i < 8; i++) {
            SDA = (b & 0x80) ? 1 : 0;
            I2C_DLY();
            b <<= 1;
            SCL = 1; I2C_DLY();
            SCL = 0; I2C_DLY();
        }
        // ACK slot: release SDA, clock once, sample the line.
        SDA = 1; I2C_DLY();
        SCL = 1; I2C_DLY();
        ack = SDA;
        SCL = 0; I2C_DLY();
        if (ack == 0) break;    // acknowledged
    }
    return ack;
}

// Is the panel answering at its I2C address right now?
static uint8_t OLED_Probe(void)
{
    uint8_t ack;
    SWI2C_Start();
    ack = I2C_WriteByte(OLED_ADDR);
    SWI2C_Stop();
    return (ack == 0) ? 1 : 0;
}

// Position the cursor and open the pixel-data phase for a burst write.
// The control byte (0x00 = command, 0x40 = data) only applies to the
// bytes that follow it within the SAME transfer, so the cursor move and
// the data phase must be two separate transactions. That still costs one
// address byte per burst instead of one per pixel byte, which is what
// makes a refresh quick.
static void OLED_StreamBegin(uint8_t x, uint8_t page)
{
    SWI2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x00);
    I2C_WriteByte(0xB0 + page);
    I2C_WriteByte(0x00 + (x & 0x0F));
    I2C_WriteByte(0x10 + ((x >> 4) & 0x0F));
    SWI2C_Stop();

    SWI2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x40);
}

// Commands are sent once, never retried individually: SSD1306 has
// two-byte commands (0xD5 <arg>), and re-sending only the first byte
// would make the panel swallow the next command as its argument.
// OLED_Init() instead retries the entire sequence.
static void OLED_WriteCmd(uint8_t cmd)
{
    SWI2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x00);
    I2C_WriteByte(cmd);
    SWI2C_Stop();
}

// ---- OLED API ----

void OLED_Init(void)
{
    uint8_t i, t;

    // Bus recovery: a glitch can leave SDA stuck low, in which case
    // the panel ignores every byte. Nine clocks plus a STOP free it.
    SDA = 1;
    for (i = 0; i < 9; i++) { SCL = 0; I2C_DLY(); SCL = 1; I2C_DLY(); }
    SWI2C_Start();
    SWI2C_Stop();
    delay_ms(150);              // let the panel charge pump settle

    // The whole sequence is retried, because a two-byte command cannot
    // be retried on its own without desyncing the panel's argument
    // state. A final address probe confirms the panel came up.
    for (t = 0; t < 3; t++) {
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
        if (OLED_Probe()) break;    // panel answered: we are done
        delay_ms(50);               // maybe the rail is still ramping
    }
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
    EA = 0;
    for (page = 0; page < 4; page++) {
        // Clear a whole page in a single transaction: 4 bytes of
        // overhead instead of 128.
        OLED_StreamBegin(0, page);
        for (col = 0; col < 128; col++) {
            I2C_WriteByte(0x00);
        }
        SWI2C_Stop();
    }
    EA = 1;
}

// 字框步长（不是视觉间距；视觉间距由 display.c 按实测墨迹宽度排布）
//   大字/数字：字框 10px（5 列字模横向 ×2）
//   16x16 汉字：字框 16px，需要紧贴拼合时（如电池图标两半）用 CN_CELL
#define CN_CELL         16

// Big font: 2x size (10x16 pixels per digit)
// Expand4[n]: 把 4 位宽的字模横向放大 2 倍（每位变 2 位）
static code const uint8_t Expand4[16] = {
    0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,
    0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF
};

static void OLED_ShowBigChar(uint8_t x, uint8_t page, char c)
{
    uint8_t i;
    uint8_t idx = CharToIndex(c);
    const uint8_t *src = Font5x7[idx];
    // 仅在第二行做 1px 下移，使上下留白均匀（不裁掉任何像素）。
    uint8_t shift = (row_gap && page >= 2) ? 1 : 0;

    EA = 0;
    // Top half
    OLED_StreamBegin(x, page);
    for (i = 0; i < 5; i++) {
        uint8_t out = Expand4[src[i] & 0x0F];
        if (shift) out <<= 1;
        I2C_WriteByte(out);
        I2C_WriteByte(out);
    }
    SWI2C_Stop();

    // Bottom half
    OLED_StreamBegin(x, page + 1);
    for (i = 0; i < 5; i++) {
        uint8_t out = Expand4[src[i] >> 4];
        if (shift) out = (out << 1) | (Expand4[src[i] & 0x0F] >> 7);
        I2C_WriteByte(out);
        I2C_WriteByte(out);
    }
    SWI2C_Stop();
    EA = 1;
}

// 6 位大数字，可指定字间距 step，用于与汉字行两端对齐
void OLED_ShowBigNum6Step(uint8_t x, uint8_t page, uint32_t num, uint8_t step)
{
    OLED_ShowBigChar(x,             page, '0' + (num / 100000));
    OLED_ShowBigChar(x + step,      page, '0' + ((num / 10000) % 10));
    OLED_ShowBigChar(x + step * 2,  page, '0' + ((num / 1000) % 10));
    OLED_ShowBigChar(x + step * 3,  page, '0' + ((num / 100) % 10));
    OLED_ShowBigChar(x + step * 4,  page, '0' + ((num / 10) % 10));
    OLED_ShowBigChar(x + step * 5,  page, '0' + (num % 10));
}

void OLED_ShowBigStringStep(uint8_t x, uint8_t page, const char *str, uint8_t step)
{
    while (*str) {
        OLED_ShowBigChar(x, page, *str);
        x += step;
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

    EA = 0;
    OLED_StreamBegin(x, page);
    for (i = 0; i < 16; i++) {
        I2C_WriteByte(F16Dat[idx][i]);
    }
    SWI2C_Stop();

    if (page < 3) {
        OLED_StreamBegin(x, page + 1);
        for (i = 0; i < 16; i++) {
            I2C_WriteByte(F16Dat[idx][16 + i]);
        }
        SWI2C_Stop();
    }
    EA = 1;
}

// Show 2 Chinese characters side by side
// 注意：步长用字框宽度 CN_CELL，两个字紧贴，便于拼合宽图形（如 32x16 电池图标）。
// 需要“留间隙”的排版请改用 display.c 里的逐字定位。
void OLED_ShowCN16x2(uint8_t x, uint8_t page, uint8_t idx1, uint8_t idx2)
{
    OLED_ShowCN16(x, page, idx1);
    OLED_ShowCN16(x + CN_CELL, page, idx2);
}
