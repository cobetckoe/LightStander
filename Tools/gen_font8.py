#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont
import os

chars = [0x80A1, 0x7968, 0x53CC, 0x8272, 0x7403, 0x79EF, 0x5206]

font_paths = [
    "C:/Windows/Fonts/simsun.ttc",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simhei.ttf",
]

font = None
for fp in font_paths:
    if os.path.exists(fp):
        font = ImageFont.truetype(fp, 8)
        print("Using font:", fp)
        break

if font is None:
    print("No Chinese font found")
    exit(1)

print("Total chars:", len(chars))
print()
print("// 8x8 Chinese font data")
print("#define F8_CNT", len(chars))
print("code const uint8_t F8Dat[F8_CNT][8] = {")

for code in chars:
    ch = chr(code)
    img = Image.new("1", (8, 8), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, -1), ch, font=font, fill=1)
    pixels = []
    for y in range(8):
        byte_val = 0
        for x in range(8):
            if y < img.height and x < img.width and img.getpixel((x, y)):
                byte_val |= (1 << (7 - x))
        pixels.append(byte_val)
    hex_str = ", ".join("0x%02X" % v for v in pixels)
    print("    { %s }, // %s" % (hex_str, ch))

print("};")