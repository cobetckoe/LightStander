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
#include "STC8G_H_EEPROM.h"
#include "eeprom.h"

// Dual backup for power-loss protection
// Slot A: 0x0000, Slot B: 0x0200 (different sectors for brown-out safety)
#define EEPROM_SLOT_A   0x0000
#define EEPROM_SLOT_B   0x0200

// Slot layout, 7 bytes:
//   [0]     checksum = XOR of [1..6]   written FIRST, so a partial write
//                                       can never look like a valid record
//   [1..4]  light_merit, little endian
//   [5]     current_mode
//   [6]     sequence number, +1 on every save; the larger one is newer
//
// 旧格式只有 5 字节且没有任何完整性信息：扇区擦除到一半就掉电时，
// 残留字节只要仍落在 mode<=1 且 merit<=100000 的范围内就会被当成合法
// 数据接受，于是出现“这次开机 79、下次开机 101”。校验字节 + 序号
// 同时解决了「识别坏槽」和「判定哪个槽更新」两个问题。
#define SLOT_SIZE       7

uint32_t light_merit = 0;
uint8_t  current_mode = MODE_STOCK;

static uint8_t save_pending = 0;
static uint8_t slot_seq = 0;

static uint8_t SlotChk(const uint8_t *b)
{
    return (uint8_t)(b[1] ^ b[2] ^ b[3] ^ b[4] ^ b[5] ^ b[6]);
}

// 读取并校验一个槽位，1 = 数据完整可信
static uint8_t ReadSlot(uint16_t addr, uint32_t *merit, uint8_t *mode, uint8_t *seq)
{
    uint8_t buf[SLOT_SIZE];
    EEPROM_read_n(addr, buf, SLOT_SIZE);
    if (buf[0] != SlotChk(buf)) return 0;   // 半写 / 擦除残留 / 位翻转
    if (buf[5] > MODE_TRADE)    return 0;
    *seq   = buf[6];
    *mode  = buf[5];
    *merit = (uint32_t)buf[1] | ((uint32_t)buf[2] << 8) |
             ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 24);
    return 1;
}

// 写入一个槽位并回读确认，失败重试。1 = 写入成功
static uint8_t WriteSlot(uint16_t addr, uint8_t seq)
{
    uint8_t w[SLOT_SIZE];
    uint8_t r[SLOT_SIZE];
    uint8_t t;

    w[1] = (uint8_t)(light_merit);
    w[2] = (uint8_t)(light_merit >> 8);
    w[3] = (uint8_t)(light_merit >> 16);
    w[4] = (uint8_t)(light_merit >> 24);
    w[5] = current_mode;
    w[6] = seq;
    w[0] = SlotChk(w);

    for (t = 0; t < 3; t++) {
        EEPROM_SectorErase(addr);
        EEPROM_write_n(addr, w, SLOT_SIZE);
        EEPROM_read_n(addr, r, SLOT_SIZE);
        if (r[0] == SlotChk(r) && r[6] == seq) return 1;
    }
    return 0;
}

void LoadPersistent(void)
{
    uint32_t merit_a, merit_b;
    uint8_t mode_a, mode_b;
    uint8_t seq_a, seq_b;
    uint8_t valid_a, valid_b;

    valid_a = ReadSlot(EEPROM_SLOT_A, &merit_a, &mode_a, &seq_a);
    valid_b = ReadSlot(EEPROM_SLOT_B, &merit_b, &mode_b, &seq_b);

    // 两个槽都可信时取序号更新的那个。序号是 8 位会回绕，所以用无符号
    // 差值判断：差值 < 0x80 表示 B 更新。原实现无条件优先 A，而
    // SavePersistent 是先擦 A 再擦 B，掉电恰好发生在两次擦写之间时
    // 就会用上半写过的 A 盖掉完好的 B。
    if (valid_a && valid_b && (uint8_t)(seq_b - seq_a) < 0x80) valid_a = 0;

    if (valid_a) {
        light_merit = merit_a;
        current_mode = mode_a;
        slot_seq = seq_a;
    } else if (valid_b) {
        light_merit = merit_b;
        current_mode = mode_b;
        slot_seq = seq_b;
    } else {
        // 两个槽都不可信：用默认值，但**不在这里写 EEPROM**。
        // 冷启动路径上连擦两个扇区（还带重试）是开机后要黑屏一段
        // 时间的原因；改为挂一个待写标记，交给下一次 FlushSave。
        light_merit = 0;
        current_mode = MODE_STOCK;
        slot_seq = 0;
        save_pending = 1;
    }
}

void SavePersistent(void)
{
    uint8_t ea_save = EA;

    EA = 0;
    save_pending = 0;
    slot_seq++;                     // 序号递增
    // 交替写单个槽（偶数序号写 A，奇数写 B）。
    // 每次只擦一个扇区：既把擦写寿命翻倍，也保证掉电时另一个槽里
    // 一定还是上一次的完整记录——最坏只丢一次递增，绝不会读出垃圾。
    if (slot_seq & 0x01) {
        WriteSlot(EEPROM_SLOT_B, slot_seq);
    } else {
        WriteSlot(EEPROM_SLOT_A, slot_seq);
    }
    EA = ea_save;
}

void SaveMeritDeferred(void)
{
    // 每次按完立即落盘。
    //
    // 原实现攒够 10 次才写，但 save_pending 只存在 RAM 里，一旦在攒的
    // 过程中断电，这几次功德就彻底丢了；而攒够那一次又会连擦两个扇区，
    // 掉电窗口正好落在 A 已擦、B 未写的瞬间的概率并不低。改成每次
    // 立即写单槽（A/B 交替）后：断电最坏只丢这一次，且另一个槽里
    // 永远是上一笔完整记录。磨损上每个扇区每两笔才擦一次，
    // 按 10 万次擦写寿命折算可支撑约 20 万次按键。
    SavePersistent();
}

// Force flush any pending save to EEPROM immediately.
// 用于在进入低功耗或关机前确保最新数据已写入。
// 无待写数据时直接返回，避免每次睡眠都擦写EEPROM（功耗+寿命）。
void FlushSave(void)
{
    if (save_pending == 0) return;
    SavePersistent();
}
