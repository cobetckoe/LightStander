# -*- coding: ascii -*-
"""Fetch latest A-share stock data and generate updated bitmap.h"""
import urllib.request
import json
import time
import re
import sys

def fetch_cninfo_szse():
    url = 'https://www.cninfo.com.cn/new/data/szse_stock.json'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    r = urllib.request.urlopen(req, timeout=15)
    d = json.loads(r.read())
    return [s['code'] for s in d['stockList'] if s['category'] == 'A\u80a1']

def fetch_sina(node, max_pages=50):
    codes = []
    for page in range(1, max_pages + 1):
        url = ('https://vip.stock.finance.sina.com.cn/quotes_service/api/'
               'json_v2.php/Market_Center.getHQNodeData?page=%d'
               '&num=80&sort=symbol&asc=1&node=%s' % (page, node))
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        try:
            r = urllib.request.urlopen(req, timeout=15)
            data = json.loads(r.read())
            if not data:
                break
            codes.extend([s['code'] for s in data])
            if len(data) < 80:
                break
            time.sleep(0.1)
        except Exception as e:
            sys.stderr.write("  sina %s page %d error: %s\n" % (node, page, str(e)))
            break
    return codes

def make_bitmap(valid_suffixes):
    bmp = [0] * 125
    for s in valid_suffixes:
        if 0 <= s < 1000:
            bmp[s >> 3] |= (1 << (s & 7))
    return bmp

print("Fetching SZSE from cninfo...")
szse_codes = fetch_cninfo_szse()
print("  SZSE: %d stocks" % len(szse_codes))

print("Fetching SSE (sh_a) from sina...")
sse_codes = fetch_sina('sh_a')
print("  SSE: %d stocks" % len(sse_codes))

print("Fetching BSE (bse_hq) from sina...")
bse_codes = fetch_sina('bse_hq')
print("  BSE: %d stocks" % len(bse_codes))

all_codes = set(szse_codes) | set(sse_codes) | set(bse_codes)
print("Total unique A-shares: %d" % len(all_codes))

prefixes = {}
for c in all_codes:
    p = c[:3]
    prefixes.setdefault(p, set()).add(c)

target = ['000','001','002','003','300','301','600','601','603','605','688','920']
old_counts = {'600':875,'601':233,'603':626,'605':113,'688':616,
              '000':506,'001':122,'002':977,'003':42,
              '300':979,'301':465,'920':335}

with open('Sources/bitmap.h', 'r', encoding='utf-8') as f:
    bmp_content = f.read()

new_bmp_arrays = {}
total_new = 0

print("\n=== Counts ===")
for pfx in target:
    stock_set = prefixes.get(pfx, set())
    suffixes = set()
    for c in stock_set:
        suffix = int(c[3:])
        suffixes.add(suffix)
    new_bmp = make_bitmap(suffixes)
    new_bmp_arrays[pfx] = new_bmp
    new_count = len(suffixes)
    total_new += new_count
    old_count = old_counts.get(pfx, 0)
    diff = new_count - old_count
    if diff > 0:
        marker = " CHANGED (+%d)" % diff
    elif diff < 0:
        marker = " CHANGED (%d)" % diff
    else:
        marker = " (unchanged)"
    print("  %sxxx: old=%d, new=%d%s" % (pfx, old_count, new_count, marker))

print("  Total: old=%d, new=%d" % (sum(old_counts.values()), total_new))

print("\n=== Stock changes ===")
for pfx in target:
    old_bmp_match = re.search(r'bitmap_%s\[\]\s*=\s*\{([^}]+)\}' % pfx, bmp_content)
    if old_bmp_match:
        old_vals = [int(x.strip(), 16) for x in old_bmp_match.group(1).split(',') if x.strip()]
    else:
        old_vals = [0]*125
    
    new_bmp = new_bmp_arrays[pfx]
    new_stocks = []
    removed_stocks = []
    for s in range(1000):
        old_bit = (old_vals[s>>3]>>(s&7))&1 if s>>3 < len(old_vals) else 0
        new_bit = (new_bmp[s>>3]>>(s&7))&1
        if new_bit and not old_bit:
            new_stocks.append("%s%03d" % (pfx, s))
        elif old_bit and not new_bit:
            removed_stocks.append("%s%03d" % (pfx, s))
    
    if new_stocks:
        print("  %sxxx new (%d): %s" % (pfx, len(new_stocks), ', '.join(new_stocks)))
    if removed_stocks:
        print("  %sxxx removed (%d): %s" % (pfx, len(removed_stocks), ', '.join(removed_stocks)))

# Save new bitmap data to file for later use
print("\n=== New bitmap C arrays ===")
for pfx in target:
    bmp = new_bmp_arrays[pfx]
    lines = []
    for i in range(0, 125, 16):
        chunk = bmp[i:i+16]
        lines.append('    ' + ','.join('0x%02X' % b for b in chunk) + ',')
    lines[-1] = lines[-1].rstrip(',')
    print("bitmap_%s:" % pfx)
    for line in lines:
        print(line)
    print()
