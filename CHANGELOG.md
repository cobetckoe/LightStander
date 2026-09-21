# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [1.0.0] - 2026-09-20

### Features
- Random A-share stock code generator covering 5914 stocks across 12 exchange prefixes
- Random trade action selector (9 actions)
- Light-based hardware entropy for true randomness (bandgap-compensated ADC)
- Ultra-low power STOP mode (~5?A standby)
- Single-button interface with short press (select) and long press (mode switch)
- Dual-slot EEPROM persistence with power-loss protection (Slot A/B in different sectors)
- SSD1306 OLED 128×32 display with 16×16 Chinese font rendering
- Software I2C with EA interrupt protection at lowest level + I2C bus recovery
- Low-battery protection: auto power-off when VDD < 3.3V (bandgap-referenced)
- Light threshold 3072 (bandgap-compensated units)
- Boot self-test displaying light entropy accumulation value
- Dynamic stock data from cninfo + sina APIs (auto-update scripts)

### Display
- Standby: mode name display (???? / ????)
- Selection: 6-digit stock code / 2-char trade action (no title)
- Light warning: ????? (with key-press immediate return + 2s auto-timeout)
- Merit: ??? + N (cumulative count, immediate key return after display)

### Responsiveness
- Result display delay reduced from 1.5s to 0.8s with key-press interruptible (EX0 re-enabled during display)
- ??+1 display returns immediately to standby (removed redundant 500ms delay)
- Boot splash reduced from 500ms to 200ms
- OLED power-up delay optimized to 150ms with I2C bus recovery (9 clock pulses)

### Reliability
- EEPROM dual-slot moved to different sectors (0x0000 / 0x0200) — brown-out safe
- OLED_Init restructured: battery check first, then I2C recovery + delayed init
- ReadSlot validates merit ? 100000 as sanity check against corrupted data
- EnterStopMode simplified: no redundant ADC/OLED reinit, main loop manages lifecycle

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
- Power: 3.7V LiPo battery (100mAh) + TP4057 charging + USB-C
- BOM cost: ¥9.90 per unit

### Code Size
- Code: 8183 bytes / 8192 bytes (99.9% utilization)
- Font data: 27 × 32 = 864 bytes
- Stock bitmaps: ~5000 bytes (12 prefix groups)
- All 8 MCU pins utilized
