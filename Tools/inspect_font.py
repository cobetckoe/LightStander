"""Measure the actual ink extents of every glyph in font_cn_16.c.

Why: on the OLED the *visual* gap between glyphs depends on the ink width,
not on the 16px drawing cell. Many glyphs only use 15 (sometimes 14) of the
16 columns, so a fixed cell pitch produces uneven-looking gaps.

Output: per-glyph left/right inked column and ink width in pixels.
Keep this in sync with the enum order in font_cn_16.h.
"""
import os
import re

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Sources', 'font_cn_16.c')

# Must match the enum order in font_cn_16.h
NAMES = ['GU', 'QING', 'ZHAN', 'ZAI', 'GUANG', 'LI', 'BU', 'KONG', 'PING',
         'JIAN', 'BAN', 'JIAN2', 'JIA', 'CHI', 'MAN', 'CANG', 'SUI', 'JI',
         'XUAN', 'YI', 'WAI', 'CAO', 'PAN', 'GONG', 'DE', 'BAT1', 'BAT2']

ENTRY = re.compile(r'\{(.*?)\}\s*,\s*//', re.S)


def main():
    text = open(SRC, encoding='utf-8', errors='replace').read()
    body = text[text.index('{', text.index('F16Dat')) + 1:]
    entries = ENTRY.findall(body)

    print(f"{'idx':>3} {'name':<6}{'ink_l':>6}{'ink_r':>6}{'cols':>5}{'px':>5}"
          f"{'adv@G=3':>9}{'adv@G=5':>9}")
    print('-' * 60)
    for idx, raw in enumerate(entries):
        vals = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', raw)]
        name = NAMES[idx] if idx < len(NAMES) else f'?{idx}'
        if len(vals) != 32:
            print(f'{idx:>3} {name:<6} -- entry has {len(vals)} bytes (not 16x16) --')
            continue
        left, right = 99, -1
        for i in range(16):
            if vals[i] or vals[16 + i]:
                left = min(left, i)
                right = max(right, i)
        if right < 0:
            print(f'{idx:>3} {name:<6} (blank)')
            continue
        cols = right - left + 1
        # standard placement advances by (right+1)+GAP so trailing blank is the gap
        adv3 = (right + 1) + 3
        adv5 = (right + 1) + 5
        print(f'{idx:>3} {name:<6}{left:>6}{right:>6}{cols:>5}{cols * 2:>5}'
              f'{adv3:>9}{adv5:>9}')


if __name__ == '__main__':
    main()
