# Contributing to LightStander

Thank you for your interest in contributing!

## How to Contribute

1. **Fork** this repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes: `git commit -m "Add your feature"`
4. Push to your branch: `git push origin feature/your-feature`
5. Open a **Pull Request**

## Development Setup

### Prerequisites
- Keil uVision 5 (MDK-51)
- STC8G1K08A development board
- SSD1306 128x32 OLED module
- Light sensor (LDR or photodiode) + 2K resistor
- 3.7V LiPo battery + TP4057 charging circuit + USB-C connector

### Build
1. Open `LightStander.uvproj` in Keil uVision
2. Build (F7) or Rebuild (Ctrl+F7)
3. Flash the hex file to MCU via STC-ISP

### Code Size Constraint
The STC8G1K08A has 8KB Flash. Current usage is ~8158 bytes (8192 limit, 99.6% utilization). **Any new feature must not exceed the 8KB limit.** If you need to add functionality, consider removing unused features first. Remember: this project is about making the impossible fit in 8KB.

## Guidelines

- Keep code compact and efficient (8KB Flash limit)
- Use `code` keyword for constant data in Flash
- Prefer `static` functions to reduce code size
- Test on real hardware before submitting
- Update `bitmap.h` and `RandPrefix()` counts if adding/removing stock codes

## Reporting Issues

- Describe the hardware setup
- Include the Keil build output
- Note the MCU and flash size
