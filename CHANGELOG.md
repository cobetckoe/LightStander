# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [1.0.1] - 2026-09-02

### Fixed
- 修复待机恢复/运行中字体变小、乱码的显示问题
- I2C 时序加固：SCL 高电平 NOP 延迟从 2 个增至 4 个（~333ns），确保 SSD1306 满足数据建立时间要求
- 关键 OLED 显示函数添加 `EA=0/EA=1` 中断保护，防止 INT0 中断打断 I2C 位操作导致 SSD1306 误收命令字节（如 0xB0 误置页地址）

## [1.0.0] - 2026-09-02

### Features
- Random A-share stock code generator covering 5914 stocks across 12 exchange prefixes
- Random trade action selector (9 actions: 补仓/空仓/平仓/减仓/半仓/建仓/加仓/持仓/满仓)
- Light-based hardware entropy for true randomness (bandgap-compensated ADC)
- Ultra-low power STOP mode (~5μA standby)
- Single-button interface with short press (select) and long press (mode switch)
- Dual-slot EEPROM persistence with power-loss protection
- SSD1306 OLED 128×32 display with 16×16 Chinese font rendering
- Boot self-test displaying light entropy accumulation value
- Dynamic stock data from cninfo + sina APIs (auto-update scripts)

### Display Messages
- Standby: 随机选股 / 意外操盘 (per mode)
- Selection: 6-digit stock code / 2-char trade action
- Light warning: 请站在光里 (when ambient light insufficient)
- Merit: 光功德 + N (cumulative count)

### Supported Stock Codes
- Shanghai Main Board: 600xxx, 601xxx, 603xxx, 605xxx
- Shanghai STAR Market: 688xxx
- Shenzhen Main Board: 000xxx, 001xxx, 002xxx, 003xxx
- ChiNext: 300xxx, 301xxx
- Beijing Stock Exchange: 920xxx

### Hardware
- MCU: STC8G1K08A (8KB Flash, 256B iRAM, 1K xRAM, 24MHz)
- Display: SSD1306 128×32 OLED, software I2C (SCL=P5.5, SDA=P5.4)
- Light sensor: ADC Channel 3 (P3.3) with 2K voltage divider
- Button: P3.2 / INT0 (falling edge)
- Bandgap reference for ratiometric ADC compensation
- Power: 3.7V LiPo battery (100mAh) + TP4057 charging + USB-C (battery directly supplies MCU and OLED)
- BOM cost: ?9.90 per unit
- Case: 1:1 optical module replica design

### Power Management
- MCU enters STOP mode between operations
- OLED charge pump disabled during sleep
- All GPIO reset to quasi-bidirectional for minimal leakage
- Wake-on-button via INT0 interrupt

### Code Size
- Total: ~8170 bytes / 8192 bytes (99.7% utilization)
- Font data: 28 × 32 = 896 bytes
- Stock bitmaps: ~5000 bytes (12 prefix groups)
- All 8 MCU pins utilized, zero waste
- BOM cost: ?9.90 — maximum value from minimum silicon
