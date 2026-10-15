import os, re

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Sources')

CN_NAMES = ['GU', 'QING', 'ZHAN', 'ZAI', 'GUANG', 'LI', 'BU', 'KONG', 'PING',
            'JIAN', 'BAN', 'JIAN2', 'JIA', 'CHI', 'MAN', 'CANG', 'SUI', 'JI',
            'XUAN', 'YI', 'WAI', 'CAO', 'PAN', 'GONG', 'DE', 'BAT1', 'BAT2']


def load_cn(path):
    t = open(path, encoding='utf-8', errors='replace').read()
    b = t[t.index('{', t.index('F16Dat')) + 1:]
    o = {}
    for i, raw in enumerate(re.findall(r'\{(.*?)\}\s*,\s*//', b, re.S)):
        v = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', raw)]
        if len(v) != 32:
            continue
        key = CN_NAMES[i] if i < len(CN_NAMES) else 'IDX' + str(i)
        o[key] = v
    return o


def load_big(path):
    t = open(path, encoding='utf-8', errors='replace').read()
    s = t.index('Font5x7')
    b = t[s:t.index('};', s)]
    o = {}
    for m in re.finditer(r'\{\s*((?:0x[0-9A-Fa-f]{2}\s*,?\s*){5})\}\s*,\s*//\s*(.*)', b):
        v = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(1))]
        c = re.search(r"'(.)'", m.group(2))
        if c:
            o[c.group(1)] = v
    return o


def cn_span(v):
    cols = [c for c in range(16) if v[c] or v[16 + c]]
    return (min(cols), max(cols)) if cols else None


def big_span(v):
    cols = [c for c in range(5) if v[c]]
    return (min(cols) * 2, max(cols) * 2 + 1) if cols else None


cn = load_cn(os.path.join(SRC, 'font_cn_16.c'))
big = load_big(os.path.join(SRC, 'oled.c'))

print('--- CN16 ink column spans (relative to glyph cell) ---')
for n in CN_NAMES:
    if n in cn:
        s = cn_span(cn[n])
        if s:
            print('  %-6s cols %2d..%-2d  width=%2d' % (n, s[0], s[1], s[1] - s[0] + 1))
        else:
            print('  %-6s (graphic/ragged)' % n)

print('')
print('--- Font5x7 big glyphs (x2 expanded) ---')
for ch in ['+', '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ' ']:
    if ch in big:
        s = big_span(big[ch])
        if s:
            print("  '%s' src=%s ink %2d..%-2d width=%2d" % (ch, big[ch], s[0], s[1], s[1] - s[0] + 1))
        else:
            print("  '%s' src=%s (blank)" % (ch, big[ch]))
