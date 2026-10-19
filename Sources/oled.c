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

// I2C 引脚：P5.5 = SCL，P5.4 = SDA
#define SCL     P55
#define SDA     P54

#define OLED_ADDR   0x78

// 只存实际会用到的字符（' '、'+'、'0'~'9'），用不上的字母和冒号删掉省 Flash。
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

// 字符到字模索引
static uint8_t CharToIndex(char c)
{
    if (c == ' ') return 0;
    if (c == '+') return 1;
    if (c >= '0' && c <= '9') return 2 + (c - '0');
    return 0;   // 不认识的当空格
}

// 行间距。32 行刚好被两行 16px 占满，没有空隙可挖。
// 数字字模上下本来就有空白，与上一行已经拉开了距离，这里不再做像素
// 屏蔽（屏蔽会吃掉汉字最下面一行）。只把第二行整体下移 1px 让留白均匀。
static uint8_t row_gap = 0;

void OLED_RowGap(uint8_t on)
{
    row_gap = on ? 1 : 0;
}

// ---- Software I2C ----

// 软件 I2C 速率。_nop_() 在 24MHz 下是单周期指令，不加延时总线会跑到
// 1MHz 左右，超出 SSD1306 的 400kHz 上限，只在电压和上拉都恰到好处时
// 才能工作。六次循环得到 tLOW≈1.5us，即 SCL≈330kHz，留有余量。
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

// 送一个字节并读 ACK 位，0 = 应答，1 = 无应答。
//
// 这里不能在 NACK 时重发。SSD1306 在页寻址模式下写数据是纯流式的，
// 多收一个字节就多占一列，整页右移一列，页尾溢出的那个字节回绕到列 0。
// 初始化用了 0xA1 段重映射，列 0 正好在最右边，于是屏幕上会出现最右侧
// 一条竖状乱码。ACK 只用来判断器件在不在线，写数据不重试。
static uint8_t I2C_WriteByte(uint8_t dat)
{
    uint8_t i;
    uint8_t ack = 1;

    for (i = 0; i < 8; i++) {
        SDA = (dat & 0x80) ? 1 : 0;
        I2C_DLY();
        dat <<= 1;
        SCL = 1; I2C_DLY();
        SCL = 0; I2C_DLY();
    }
    // ACK slot: release SDA, clock once, sample the line.
    SDA = 1; I2C_DLY();
    SCL = 1; I2C_DLY();
    ack = SDA;
    SCL = 0; I2C_DLY();
    return ack;
}

// 光标定位，并打开像素数据相位。控制字节（0x00 命令 / 0x40 数据）
// 只对它后面的字节有效，所以移光标和写数据必須分成两次事务。
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

// 命令不单独重试：SSD1306 有双字节命令（0xD5 <arg>），只重发首字节
// 会让面板把下一条命令当成参数吞掉。要重试就重试整串。
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

    // 先把两根线拉低放一会儿，泄掉面板上的残压。
    //
    // 只断正极再上电时，SSD1306 的 VDD 会经 I2C 上拉电阻和各引脚 ESD
    // 二极管反向取得微弱电流，板上滤波电容也让它降得很慢。面板因此
    // 没有真正跌破内部 POR 门限，不会复位：它还照样回 ACK，却可能停在
    // 上次的半个字节中间，把后面的地址字节当数据吞掉，电荷泵从未打开，
    // 0xAF 之后屏幕依然全黑。
    //
    // 拉低既断开了这条供电路径，也把引脚和电容上的残压泄到地。
    // 必须先把引脚配成开漏，否则推挽拉低会灌电流。
    P5M0 = 0x30; P5M1 = 0x30;
    SCL = 0; SDA = 0;
    delay_ms(250);
    SDA = 1; SCL = 1;

    delay_ms(300);              // 留够时间让 VDD 爬完、内部基准起振

    // 总线恢复：SDA 被卡低时面板会忽略所有字节，打几个时钟再补个 STOP 放开。
    // 这里用 20 个而不是规范的 9 个，多打没有副作用，能把移位寄存器冲干净。
    SDA = 1;
    for (i = 0; i < 20; i++) { SCL = 0; I2C_DLY(); SCL = 1; I2C_DLY(); }
    SWI2C_Start();
    SWI2C_Stop();
    delay_ms(120);              // 等电荷泵稳定

    // 整串命令连发三遍，无条件，不做「成功就 break」。
    //
    // 曾经用 OLED_Probe() 做跳出条件，但那个探测只有器件地址 ACK，而
    // SSD1306 只要通了电就一定 ACK，不管有没有真收下命令，于是 break
    // 第一轮必定命中，重试形同虚设。面板也无法回读 GDDRAM，软件上不
    // 存在能证伪的初始化判据，那就别假装能测。命令都是幂等的，多发
    // 三遍只多花几十毫秒。
    //
    // 每轮之间那次 80ms 很关键：第一遍已经把电荷泵打开，等电源真正
    // 建起来，第二遍的 0xAF 才落在稳定电压上。
    for (t = 0; t < 3; t++) {
        if (t) {
            SDA = 1;
            for (i = 0; i < 20; i++) { SCL = 0; I2C_DLY(); SCL = 1; I2C_DLY(); }
            SWI2C_Start();
            SWI2C_Stop();
            delay_ms(80);
        }
        OLED_WriteCmd(0xAE);    // 关显示
        OLED_WriteCmd(0xD5);    // 时钟分频
        OLED_WriteCmd(0x80);
        OLED_WriteCmd(0xA8);    // 多路复用比
        OLED_WriteCmd(0x1F);    // 128x32 用 31
        OLED_WriteCmd(0xD3);    // 显示偏移
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x40);    // 起始行 0
        OLED_WriteCmd(0x8D);    // 电荷泵
        OLED_WriteCmd(0x14);    // 打开
        // 必须等电荷泵把 VCC 抬上来再开显示，否则会出现
        // 「显示已开、屏幕却全黑」。
        delay_ms(20);
        OLED_WriteCmd(0x20);    // 寻址模式
        OLED_WriteCmd(0x02);    // 页寻址
        OLED_WriteCmd(0xA1);    // 段重映射，180° 旋转（列 0 在最右）
        OLED_WriteCmd(0xC8);    // COM 扫描方向，180° 旋转
        OLED_WriteCmd(0xDA);    // COM 引脚配置
        OLED_WriteCmd(0x02);    // 128x32
        OLED_WriteCmd(0x81);    // 对比度
        OLED_WriteCmd(0xCF);
        OLED_WriteCmd(0xD9);    // 预充周期
        OLED_WriteCmd(0xF1);
        OLED_WriteCmd(0xDB);    // VCOMH
        OLED_WriteCmd(0x40);
        OLED_WriteCmd(0xA4);    // 按显存内容显示
        OLED_WriteCmd(0xA6);    // 正常显示（非反色）
        OLED_WriteCmd(0xAF);    // 开显示
    }

    // 再确认一次电荷泵和显示开关，救一下前面恰好落在
    // 面板未就绪窗口里的那一次。
    OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x14);
    delay_ms(120);
    OLED_WriteCmd(0xAF);

    OLED_Clear();               // 上电显存是随机的，不清会显出一片雪花
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
        // 整页一次性清掉，开一次事务就够了，不用每列都开
        OLED_StreamBegin(0, page);
        for (col = 0; col < 128; col++) {
            I2C_WriteByte(0x00);
        }
        SWI2C_Stop();
    }
    EA = 1;
}

// （字框步长，不是视觉间距；视觉间距由 display.c 按实测墨迹宽度排布）
//   大字/数字：字框 10px（5 列字模横向 ×2）
//   16x16 汉字：字框 16px，需要紧贴拼合时（如电池图标两半）用 CN_CELL
#define CN_CELL         16

// 数字按 2 倍显示，每个 10x16
// Expand4[n]: 把 4 位宽的字模横向放大 2 倍
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

// 16x16 汉字，字模按列顺序存，配合 0xA1 + 0xC8 的 180° 旋转
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

// 两个字拼在一起，步长用字框宽度 CN_CELL，用于电池图标这种宽图形。
void OLED_ShowCN16x2(uint8_t x, uint8_t page, uint8_t idx1, uint8_t idx2)
{
    OLED_ShowCN16(x, page, idx1);
    OLED_ShowCN16(x + CN_CELL, page, idx2);
}
