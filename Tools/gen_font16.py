#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont

# 股 票 积 分 光 照 不 足 补 空 平 减 半 建 加 持 满 仓 随 机 选 意 外 操 盘 功 德
chars = [0x80A1, 0x7968, 0x79EF, 0x5206, 0x5149, 0x7167, 0x4E0D, 0x8DB3,
         0x8865, 0x7A7A, 0x5E73, 0x51CF, 0x534A, 0x5EFA, 0x52A0, 0x6301, 0x6EE1, 0x4ED3,
         0x968F, 0x673A, 0x9009, 0x610F, 0x5916, 0x64CD, 0x76D8,
         0x529F, 0x5FB7]

font = ImageFont.truetype('C:/Windows/Fonts/simsun.ttc', 16)

for code in chars:
    ch = chr(code)
    # Render on larger canvas to avoid cutoff
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ch, font=font, fill=1)

    # OLED uses 0xA0 (segment remap normal) - no horizontal flip needed
    # OLED_ShowCN16 uses SetCursor with 3-page mapping for 0+C8
    # No flip needed, data is stored directly

    pixels = [[0]*16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            if y < img.height and x < img.width and img.getpixel((x, y)):
                pixels[y][x] = 1

    data = []
    for page in range(2):
        for col in range(16):
            byte_val = 0
            for row_in_page in range(8):
                y = page * 8 + row_in_page
                if y < img.height and pixels[y][col]:
                    byte_val |= (1 << row_in_page)  # bit 0 = top of page
            data.append(byte_val)

    hex_str = ', '.join('0x%02X' % v for v in data)
    print('{ %s }, // %s' % (hex_str, ch))