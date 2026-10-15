# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont

font = ImageFont.truetype('C:/Windows/Fonts/simsun.ttc', 16)

chars = [0x8BF7, 0x7AD9, 0x5728, 0x91CC]

for code in chars:
    ch = chr(code)
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ch, font=font, fill=1)

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
                if y < 16 and pixels[y][col]:
                    byte_val |= (1 << row_in_page)
            data.append(byte_val)

    hex_str = ', '.join('0x%02X' % v for v in data)
    print('{ %s }, // U+%04X' % (hex_str, code))
