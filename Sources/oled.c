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

// 画一根时钟线，用来把卡在半个字节里的移位寄存器冲干净。
// SDA 必须由主控保持高电平：面板被卡住时会把 SDA 拉着不放，
// 这时候主控不去驱动它，打再多时钟也放不开。
static void I2C_BusRecover(void)
{
    uint8_t i;

    SDA = 1;
    for (i = 0; i < 20; i++) {
        SCL = 0; I2C_DLY();
        SCL = 1; I2C_DLY();
    }
    SWI2C_Start();
    SWI2C_Stop();
}

// 显示配置表：每条一个字节的命令，加一个字节的参数。
// 单字节命令的参数位置填 CFG_NA，这样增删命令都不用担心数错参数个数。
//
// 冷启动和唤醒都走这张表。以前唤醒只补发几条命令，理由是「睡眠时面板
// 一直有电，状态不会丢」——这个假设只在短时间休眠时成立。电池电压偏低、
// 或者面板自己的复位电路被 I2C 线上的漏电流慢慢拖到 POR 门限以下时，
// 面板会在睡眠期间悄悄复位，寄存器全回出厂默认值，于是出现三种现象：
//   0xC8 丢失 → COM 扫描方向反了 → 画面上下颠倒
//   0x81 从 0xCF 回到 0x7F、0xD9 从 0xF1 回到 0x22 → 颜色很淡
//   0xA8 从 0x1F(32路) 回到 0x3F(64路) → 多扫出来的行 → 一行乱码
// 所以唤醒必须把整张表重发一遍。命令都是幂等的，重发只多花几十毫秒。
#define CFG_NA          0xFF

static code const uint8_t oled_cfg[][2] = {
    { 0xAE, CFG_NA },   // 关显示，配置期间不让人看见中间状态
    { 0xD5, 0x80   },   // 时钟分频
    { 0xA8, 0x1F   },   // 多路复用比，128x32 用 31
    { 0xD3, 0x00   },   // 显示偏移
    { 0x40, CFG_NA },   // 起始行 0
    { 0x8D, 0x14   },   // 电荷泵开
    { 0x20, 0x02   },   // 页寻址
    { 0xA1, CFG_NA },   // 段重映射（列 0 在最右，配合 180° 旋转）
    { 0xC8, CFG_NA },   // COM 扫描方向，与 0xA1 一起构成 180° 旋转
    { 0xDA, 0x02   },   // COM 引脚配置，128x32
    { 0x81, 0xCF   },   // 对比度
    { 0xD9, 0xF1   },   // 预充周期
    { 0xDB, 0x40   },   // VCOMH
    { 0xA4, CFG_NA },   // 按显存内容显示
    { 0xA6, CFG_NA },   // 正常显示（非反色）
};

#define CFG_CNT         (sizeof(oled_cfg) / sizeof(oled_cfg[0]))

// 把配置表整串发下去。SSD1306 的命令之间不需要间隔，所以一口气发完。
// 表尾不发 0xAF，开屏由调用方在确认显存已经干净之后自己做。
static void OLED_LoadConfig(void)
{
    uint8_t i;

    for (i = 0; i < CFG_CNT; i++) {
        OLED_WriteCmd(oled_cfg[i][0]);
        if (oled_cfg[i][1] != CFG_NA) {
            OLED_WriteCmd(oled_cfg[i][1]);
        }
    }
}

// 冷启动：面板可能处于任何状态，必须先泄放残压把它逼回 POR。
// 结束时保持关屏，调用方把第一屏画好再调 OLED_On()。
void OLED_Init(void)
{
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
    I2C_BusRecover();

    OLED_LoadConfig();

    // 配置里已经打开了电荷泵，等它把 VCC 抬起来再往显存里写，
    // 否则写进去的是打给一颗还没醒的芯片。
    delay_ms(20);

    // GDDRAM 上电是随机内容，必须清干净。清完仍不开屏——让调用方先把
    // 开机那一屏画进去，再点亮，用户看到的第一帧就是完整画面。
    OLED_Clear();
}

void OLED_Off(void)
{
    OLED_WriteCmd(0xAE);    // Display OFF
    OLED_WriteCmd(0x8D);    // Charge pump
    OLED_WriteCmd(0x10);    // Disable
}

// 从 STOP 唤醒时先调这个：把面板拉回一个已知的配置状态，继续保持关屏。
//
// 为什么不能只补发几条命令：睡眠期间面板一直挂着电池，看起来状态不会丢，
// 但电池电压偏低时漏电流会把面板自己的复位电路慢慢拖到 POR 门限以下，
// 面板就悄悄复位了，寄存器全回出厂默认值。三种现象都能对上：
//   0xC8 丢失 → COM 扫描方向反了 → 画面上下颠倒
//   0x81 从 0xCF 回到 0x7F、0xD9 从 0xF1 回到 0x22 → 颜色很淡
//   0xA8 从 0x1F(32路) 回到 0x3F(64路) → 多扫出 32 行 → 一行乱码
// 面板复位时 GDDRAM 也会变成随机值，所以睡前写好的那一页同样不可信，
// 调用方必须重新画一遍。整个过程都保持关屏，用户看不到中间状态。
void OLED_Wake(void)
{
    // 睡眠时引脚恢复成了准双向，这里先切回开漏再碰总线。
    P5M0 = 0x30; P5M1 = 0x30;
    SDA = 1; SCL = 1;
    delay_ms(2);

    // 唤醒瞬间面板可能正在从电荷泵关断中恢复，SDA 若被它拉着，
    // 打几个时钟把它放出来，避免第一笔事务被吞。
    I2C_BusRecover();

    // 整表重发。表头是 0xAE，配置全程保持关屏。
    OLED_LoadConfig();

    // 等电荷泵把 VCC 抬起来，否则接下来画进去的对比度会偏。
    delay_ms(20);
}

// 开屏。放在所有绘制之后再调，用户看到的第一帧就是完整的画面。
void OLED_On(void)
{
    OLED_WriteCmd(0xAF);
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
