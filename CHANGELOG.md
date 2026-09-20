# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [1.0.0] - 2026-09-20

### Features
- Random A-share stock code generator covering 5914 stocks across 12 exchange prefixes
- Random trade action selector (9 actions: ²¹²Ö/¿Õ²Ö/Æ½²Ö/¼õ²Ö/°ë²Ö/½¨²Ö/¼Ó²Ö/³Ö²Ö/Âú²Ö)
- Trade mode display: shows "????" title + action result (consistent with stock mode)
- Light-based hardware entropy for true randomness (bandgap-compensated ADC)
- Ultra-low power STOP mode (~5?A standby)
- Single-button interface with short press (select) and long press (mode switch)
- Dual-slot EEPROM persistence with power-loss protection
- SSD1306 OLED 128×32 display with 16×16 Chinese font rendering
- Software I2C: EA interrupt protection at lowest level (per WriteCmd/WriteData)
- Software I2C: 4-NOP SCL high period (~250ns) for SSD1306 timing compliance
- Low-battery protection: auto power-off when VDD < 3.3V (bandgap-referenced)
- Light threshold optimized to 3072 (bandgap-compensated units)
- Boot self-test displaying light entropy accumulation value
- Dynamic stock data from cninfo + sina APIs (auto-update scripts)

### Display Messages
- Standby: Ëæ»úÑ¡¹É / ÒâÍâ²ÙÅÌ (per mode)
- Selection: 6-digit stock code / 2-char trade action
- Light warning: ÇëÕ¾ÔÚ¹âÀï (when ambient light insufficient)
- Merit: ¹â¹¦µÂ + N (cumulative count)

### Supported Stock Codes
- Shanghai Main Board: 600xxx, 601xxx, 603xxx, 605xxx
- Shanghai STAR Market: 688xxx
- Shenzhen Main Board: 000xxx, 001xxx, 002xxx, 003xxx
- ChiNext: 300xxx, 301xxx
- Beijing Stock Exchange: 920xxx

### Hardware
- MCU: STC8G1K08A (8KB Flash, 256B iRAM, 1K xRAM, 24MHz)
- Display: SSD1306 128¡Á32 OLED, software I2C (SCL=P5.5, SDA=P5.4)
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
- Total: ~8187 bytes / 8192 bytes (99.9% utilization)
- Font data: 27 ¡Á 32 = 864 bytes
- Stock bitmaps: ~5000 bytes (12 prefix groups)
- All 8 MCU pins utilized, zero waste
- BOM cost: ?9.90 ¡ª maximum value from minimum silicon
