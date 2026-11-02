#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 README 里那张「一页看懂站光侠」的说明图。

用 3 倍超采样绘制再降采样，边缘和文字才干净；直接按目标尺寸画会到处是锯齿。

    python Tools/make_how_it_works.py

输出覆盖 docs/images/how-it-works.png。
"""

import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

# ---------------------------------------------------------------- 基础

S = 3                      # 超采样倍数
W, H = 1240, 1750          # 目标尺寸（1x）

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "images", "how-it-works.png")

FONT_DIR = r"C:\Windows\Fonts"
F_BOLD = os.path.join(FONT_DIR, "msyhbd.ttc")
F_REG = os.path.join(FONT_DIR, "msyh.ttc")

# 配色：暖黄底 + 深海军蓝 + 奶油卡片，保持原来的品牌感觉
BG_TOP = (248, 203, 74)
BG_BOT = (240, 180, 42)
NAVY = (18, 58, 99)
NAVY_D = (11, 34, 60)
NAVY_L = (30, 80, 130)
CREAM = (252, 241, 213)
CREAM_D = (240, 224, 184)
INK = (20, 38, 60)
GOLD = (222, 165, 55)
GOLD_L = (242, 208, 124)
GOLD_D = (170, 124, 30)
RED = (198, 50, 42)
GREEN = (44, 140, 76)
MUTED = (124, 108, 80)
WHITE = (255, 251, 238)

_fonts = {}


def font(path, size, index=0):
    key = (path, size, index)
    if key not in _fonts:
        _fonts[key] = ImageFont.truetype(path, int(size * S), index=index)
    return _fonts[key]


def bold(size):
    return font(F_BOLD, size)


def reg(size):
    return font(F_REG, size)


class Pen:
    """把 1x 坐标换算成画布坐标的薄封装。"""

    def __init__(self, d):
        self.d = d

    def _p(self, v):
        return int(round(v * S))

    def _b(self, box):
        return tuple(self._p(v) for v in box)

    def rr(self, box, r, fill=None, outline=None, width=1):
        self.d.rounded_rectangle(self._b(box), self._p(r), fill=fill,
                                 outline=outline, width=max(1, self._p(width)))

    def rect(self, box, fill=None, outline=None, width=1):
        self.d.rectangle(self._b(box), fill=fill, outline=outline,
                         width=max(1, self._p(width)))

    def ell(self, box, fill=None, outline=None, width=1):
        self.d.ellipse(self._b(box), fill=fill, outline=outline,
                       width=max(1, self._p(width)))

    def circle(self, cx, cy, r, fill=None, outline=None, width=1):
        self.ell((cx - r, cy - r, cx + r, cy + r), fill, outline, width)

    def poly(self, pts, fill=None, outline=None):
        self.d.polygon([(self._p(x), self._p(y)) for x, y in pts],
                       fill=fill, outline=outline)

    def line(self, pts, fill, width=1):
        self.d.line([(self._p(x), self._p(y)) for x, y in pts],
                    fill=fill, width=max(1, self._p(width)))

    def text(self, xy, txt, f, fill, anchor="mm"):
        self.d.text((self._p(xy[0]), self._p(xy[1])), txt,
                    font=f, fill=fill, anchor=anchor)


def vgrad(w, h, c1, c2):
    """竖直渐变，先做 1 像素宽再拉伸，省内存。"""
    strip = Image.new("RGB", (1, h))
    px = strip.load()
    for y in range(h):
        t = y / (h - 1) if h > 1 else 0
        px[0, y] = tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))
    return strip.resize((w, h), Image.BILINEAR)


def paste_rr_grad(img, box, r, c1, c2):
    x0, y0, x1, y1 = [int(round(v * S)) for v in box]
    w, h = max(1, x1 - x0), max(1, y1 - y0)
    g = vgrad(w, h, c1, c2)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, w - 1, h - 1), int(round(r * S)), fill=255)
    img.paste(g, (x0, y0), mask)


def shadow(img, box, r, blur, dy, alpha):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    x0, y0, x1, y1 = [int(round(v * S)) for v in box]
    d.rounded_rectangle((x0, y0 + int(dy * S), x1, y1 + int(dy * S)),
                        int(round(r * S)), fill=(0, 0, 0, alpha))
    layer = layer.filter(ImageFilter.GaussianBlur(blur * S))
    img.alpha_composite(layer)


def glow(img, cx, cy, radius, color, alpha):
    """柔和光晕，用来让太阳周围亮一点。"""
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    r = int(radius * S)
    d.ellipse((int(cx * S) - r, int(cy * S) - r,
               int(cx * S) + r, int(cy * S) + r), fill=color + (alpha,))
    layer = layer.filter(ImageFilter.GaussianBlur(radius * S / 2.2))
    img.alpha_composite(layer)


# ---------------------------------------------------------------- 元件


def draw_module(img, D, x, y, w, h, screen=None, screen_color=GREEN,
                off=False, led=True):
    """画一台设备：深色金属机身 + 左端镀金触点 + 右端双 LC 口 + 中间屏幕。"""
    r = h * 0.16

    shadow(img, (x + 2, y + 6, x + w + 2, y + h + 6), r, 9, 7, 95)

    paste_rr_grad(img, (x, y, x + w, y + h), r, (42, 51, 62), (15, 19, 25))
    D.rr((x, y, x + w, y + h), r, outline=(9, 12, 16), width=1.6)

    # 顶部一条细高光，金属感就靠它
    D.rr((x + w * 0.03, y + h * 0.07, x + w * 0.80, y + h * 0.15),
         h * 0.05, fill=(70, 83, 99))

    # 左端镀金触点
    gw = w * 0.095
    paste_rr_grad(img, (x + 1, y + h * 0.13, x + gw, y + h * 0.87),
                  r * 0.5, GOLD_L, GOLD_D)
    n = 5
    for i in range(1, n):
        gx = x + 1 + gw * i / n
        D.line([(gx, y + h * 0.17), (gx, y + h * 0.83)], (150, 108, 24), 1.1)

    # 右端双 LC 口。半径和位置都要收在机身内，否则会凸出右边缘
    pr = h * 0.155
    for px in (x + w * 0.835, x + w * 0.930):
        D.circle(px, y + h * 0.5, pr, fill=(7, 9, 13), outline=GOLD, width=1.3)
        D.circle(px, y + h * 0.5, pr * 0.52, fill=(3, 4, 6))

    # 屏幕。右边界让到 0.76，给 LC 口腾地方
    sx0, sx1 = x + w * 0.15, x + w * 0.76
    sy0, sy1 = y + h * 0.20, y + h * 0.80
    if off:
        D.rr((sx0, sy0, sx1, sy1), 3, fill=(6, 9, 14), outline=(3, 4, 6), width=1)
        # 熄屏时两条极暗的横线，像余辉
        cy = (sy0 + sy1) / 2
        D.line([(sx0 + w * 0.10, cy), (sx0 + w * 0.22, cy)], (26, 34, 44), 2)
        D.line([(sx1 - w * 0.22, cy), (sx1 - w * 0.10, cy)], (26, 34, 44), 2)
    else:
        D.rr((sx0, sy0, sx1, sy1), 3, fill=(3, 16, 9), outline=(2, 8, 5), width=1)
        if screen:
            # 字号按可用宽度收，长字符串不能画出屏幕。
            # textlength 返回的是画布像素（3x），比较前要一起放大。
            size = h * 0.34
            while size > 8:
                f = bold(size)
                if D.d.textlength(screen, font=f) <= (sx1 - sx0) * 0.92 * S:
                    break
                size *= 0.94
            D.text(((sx0 + sx1) / 2, (sy0 + sy1) / 2), screen, f, screen_color)

    if led:
        D.circle(x + w * 0.055, y + h * 0.055, h * 0.035, fill=(214, 66, 54))


def draw_sun(img, D, cx, cy, r):
    """太阳：光晕 + 渐变色球 + 锥形光芒 + 笑脸。"""
    glow(img, cx, cy, r * 3.4, (255, 226, 140), 70)

    # 锥形光芒，外端收尖。只画右半圈和上下，左边留给标题，右边留给射向人物的光束
    import math
    for i in range(8):
        a = math.pi * 2 * i / 8 + math.pi / 8
        # 跳过射向人物的那三个方向（约 -15°~13°），避免光芒和光束重叠
        deg = (math.degrees(a) + 360) % 360
        if 340 <= deg <= 360 or deg <= 20:
            continue
        r0, r1 = r * 1.30, r * 1.68
        half = 0.052 if i % 2 == 0 else 0.034
        p1 = (cx + r0 * math.cos(a - half), cy + r0 * math.sin(a - half))
        p2 = (cx + r0 * math.cos(a + half), cy + r0 * math.sin(a + half))
        p3 = (cx + r1 * math.cos(a), cy + r1 * math.sin(a))
        D.poly([p1, p2, p3], fill=GOLD)

    # 球体：上下渐变做出体积
    paste_rr_grad(img, (cx - r, cy - r, cx + r, cy + r), r, (255, 214, 92), (233, 160, 36))
    D.circle(cx, cy, r, outline=(198, 130, 22), width=1.6)

    # 高光
    D.ell((cx - r * 0.62, cy - r * 0.72, cx - r * 0.10, cy - r * 0.28),
          fill=(255, 240, 196))

    # 笑脸
    D.circle(cx - r * 0.34, cy - r * 0.10, r * 0.075, fill=(120, 74, 16))
    D.circle(cx + r * 0.34, cy - r * 0.10, r * 0.075, fill=(120, 74, 16))
    D.d.arc([(cx - r * 0.34) * S, (cy + r * 0.02) * S,
             (cx + r * 0.34) * S, (cy + r * 0.52) * S],
            20, 160, fill=(120, 74, 16), width=max(1, int(1.6 * S)))


def _limb(D, pts, w0, w1, fill):
    """带粗细变化的肢体：沿折线逐段铺圆，末端收细，比一根直线像手臂。"""
    import math
    segs = len(pts) - 1
    for i in range(segs):
        (x1, y1), (x2, y2) = pts[i], pts[i + 1]
        t0, t1 = i / segs, (i + 1) / segs
        n = 6
        for k in range(n + 1):
            t = t0 + (t1 - t0) * k / n
            w = w0 + (w1 - w0) * t
            px, py = x1 + (x2 - x1) * k / n, y1 + (y2 - y1) * k / n
            D.circle(px, py, w, fill=fill)


def draw_hero(img, D, cx, feet, hgt, body=NAVY, accent=GOLD):
    """站立的侠客剪影：双手上举迎接光。实心块面，不是火柴人。"""
    import math
    head_r = hgt * 0.108
    head_cy = feet - hgt * 0.845
    sh_y = head_cy + head_r * 0.78          # 肩线
    waist = feet - hgt * 0.395

    # 影子
    D.ell((cx - hgt * 0.215, feet - hgt * 0.016, cx + hgt * 0.215, feet + hgt * 0.030),
          fill=(205, 156, 58))

    # 腿：上宽下窄，两腿间留一道缝
    for sgn in (-1, 1):
        D.poly([(cx + sgn * hgt * 0.018, waist),
                (cx + sgn * hgt * 0.098, waist),
                (cx + sgn * hgt * 0.088, feet - hgt * 0.075),
                (cx + sgn * hgt * 0.104, feet),
                (cx + sgn * hgt * 0.020, feet),
                (cx + sgn * hgt * 0.014, feet - hgt * 0.075)], fill=body)
    # 鞋
    for sgn in (-1, 1):
        D.rr((cx + sgn * hgt * 0.012 - (hgt * 0.062 if sgn < 0 else 0),
              feet - hgt * 0.035,
              cx + sgn * hgt * 0.012 + (0 if sgn < 0 else hgt * 0.062),
              feet + hgt * 0.012), hgt * 0.018, fill=(12, 30, 52))

    # 躯干：肩宽腰窄的梯形，比矩形有型
    D.poly([(cx - hgt * 0.118, sh_y), (cx + hgt * 0.118, sh_y),
            (cx + hgt * 0.100, waist + hgt * 0.01),
            (cx - hgt * 0.100, waist + hgt * 0.01)], fill=body)

    # 领口
    D.poly([(cx - hgt * 0.048, sh_y), (cx + hgt * 0.048, sh_y),
            (cx, sh_y + hgt * 0.052)], fill=(238, 246, 252))

    # 腰带 + 扣
    D.rr((cx - hgt * 0.102, waist - hgt * 0.030,
          cx + hgt * 0.102, waist + hgt * 0.012), hgt * 0.012, fill=accent)
    D.rr((cx - hgt * 0.020, waist - hgt * 0.028,
          cx + hgt * 0.020, waist + hgt * 0.010), hgt * 0.008, fill=(252, 246, 230))

    # 双臂上举：肩 → 肘 → 手，由粗到细
    for sgn in (-1, 1):
        _limb(D, [(cx + sgn * hgt * 0.108, sh_y + hgt * 0.012),
                  (cx + sgn * hgt * 0.175, sh_y - hgt * 0.085),
                  (cx + sgn * hgt * 0.232, head_cy - head_r * 1.15)],
              hgt * 0.040, hgt * 0.024, body)
        # 手
        D.circle(cx + sgn * hgt * 0.240, head_cy - head_r * 1.28,
                 hgt * 0.031, fill=(255, 236, 200))

    # 脖子
    D.rr((cx - hgt * 0.026, head_cy + head_r * 0.55,
          cx + hgt * 0.026, sh_y + hgt * 0.004), hgt * 0.010, fill=(240, 216, 176))

    # 头
    D.circle(cx, head_cy, head_r, fill=(255, 240, 208), outline=(18, 58, 99), width=1.4)

    # 头带
    D.rr((cx - head_r * 1.02, head_cy - head_r * 0.92,
          cx + head_r * 1.02, head_cy - head_r * 0.40), head_r * 0.26, fill=body)
    # 飘尾：向左侧飘两段，末端收窄
    D.poly([(cx - head_r * 0.98, head_cy - head_r * 0.80),
            (cx - head_r * 2.10, head_cy - head_r * 0.52),
            (cx - head_r * 1.86, head_cy - head_r * 0.10),
            (cx - head_r * 0.96, head_cy - head_r * 0.34)], fill=accent)

    # 眼睛
    D.circle(cx - head_r * 0.38, head_cy + head_r * 0.14, head_r * 0.10, fill=body)
    D.circle(cx + head_r * 0.38, head_cy + head_r * 0.14, head_r * 0.10, fill=body)
    # 嘴角
    D.d.arc([(cx - head_r * 0.26) * S, (head_cy + head_r * 0.26) * S,
             (cx + head_r * 0.26) * S, (head_cy + head_r * 0.62) * S],
            20, 160, fill=(18, 58, 99), width=max(1, int(1.3 * S)))


def draw_badge(D, x, y, num, color, r=17):
    D.circle(x, y, r, fill=color)
    D.text((x, y - 1), str(num), bold(r * 1.15), WHITE)


def draw_swap(D, cx, cy, r):
    """一上一下两支箭头，表示两种模式互换。两支各自独立，别挤在一起。"""
    D.circle(cx, cy, r, fill=(240, 228, 194), outline=GOLD, width=1.6)

    ax = r * 0.34            # 两支箭头到圆心的横向间距
    sh = r * 0.115           # 箭杆半宽
    hw = r * 0.30            # 箭头半宽
    hl = r * 0.32            # 箭头高
    top, bot = cy - r * 0.60, cy + r * 0.60

    # 左：向上
    D.poly([(cx - ax - hw, top + hl), (cx - ax + hw, top + hl), (cx - ax, top)], fill=NAVY)
    D.rect((cx - ax - sh, top + hl, cx - ax + sh, bot), fill=NAVY)

    # 右：向下
    D.poly([(cx + ax - hw, bot - hl), (cx + ax + hw, bot - hl), (cx + ax, bot)], fill=RED)
    D.rect((cx + ax - sh, top, cx + ax + sh, bot - hl), fill=RED)


def bubble(D, box, txt, tail_x, tail_to_left=True, f=None, fill=WHITE):
    D.rr(box, 9, fill=fill, outline=NAVY, width=1.4)
    x0, y0, x1, y1 = box
    if tail_to_left:
        D.poly([(x0 + 16, y1 - 2), (x0 + 4, y1 + 16), (x0 + 34, y1 - 2)], fill=fill)
    else:
        D.poly([(x1 - 34, y1 - 2), (x1 - 4, y1 + 16), (x1 - 16, y1 - 2)], fill=fill)
    D.text(((x0 + x1) / 2, (y0 + y1) / 2), txt, f, INK)


def draw_emblem(D, cx, cy, r):
    """右上角的圆形徽标：举手的侠客剪影。"""
    D.circle(cx, cy, r, fill=(24, 68, 112), outline=GOLD, width=2)
    D.circle(cx, cy, r * 0.86, outline=(48, 100, 152), width=1)

    head_r = r * 0.155
    head_cy = cy - r * 0.30
    body = GOLD_L

    # 躯干
    D.rr((cx - r * 0.15, head_cy + head_r * 0.5, cx + r * 0.15, cy + r * 0.42),
         r * 0.07, fill=body)
    # 双臂上举
    for sgn in (-1, 1):
        D.line([(cx + sgn * r * 0.13, head_cy + head_r * 0.7),
                (cx + sgn * r * 0.34, head_cy - head_r * 0.2),
                (cx + sgn * r * 0.46, head_cy - head_r * 1.1)], body, r * 0.075)
    # 头
    D.circle(cx, head_cy, head_r, fill=body)
    # 腿
    for sgn in (-1, 1):
        D.line([(cx + sgn * r * 0.07, cy + r * 0.38),
                (cx + sgn * r * 0.14, cy + r * 0.70)], body, r * 0.065)


# ---------------------------------------------------------------- 主体


def build():
    img = Image.new("RGBA", (W * S, H * S), BG_TOP + (255,))
    D = Pen(ImageDraw.Draw(img))

    # 背景渐变
    img.paste(vgrad(W * S, H * S, BG_TOP, BG_BOT), (0, 0))
    glow(img, W * 0.5, 0, W * 0.75, (255, 238, 176), 60)

    # ---------------- 页眉
    hx0, hy0, hx1, hy1 = 28, 24, 1212, 180
    shadow(img, (hx0, hy0, hx1, hy1), 20, 10, 8, 110)
    paste_rr_grad(img, (hx0, hy0, hx1, hy1), 20, NAVY_L, NAVY_D)
    D.rr((hx0, hy0, hx1, hy1), 20, outline=(8, 26, 48), width=1.8)

    # 左侧竖条
    D.rr((hx0 + 34, hy0 + 34, hx0 + 44, hy1 - 34), 5, fill=GOLD)

    D.text((hx0 + 62, hy0 + 68), "站光侠", bold(56), WHITE, "lm")
    D.text((hx0 + 300, hy0 + 56), "让太阳做你的操盘手", bold(31), (232, 240, 250), "lm")
    D.text((hx0 + 300, hy0 + 104), "赌王不赌 · 股神不炒 · 阳光推荐",
           reg(19), GOLD_L, "lm")

    draw_emblem(D, hx1 - 88, (hy0 + hy1) / 2, 58)

    # ---------------- 四张卡片
    CX1, CX2 = 28, 630
    CW = 582
    R1, R2 = 200, 710
    CH = 490

    def card(x, y, num, title, color):
        shadow(img, (x, y, x + CW, y + CH), 16, 9, 8, 80)
        D.rr((x, y, x + CW, y + CH), 16, fill=CREAM, outline=NAVY, width=2)
        draw_badge(D, x + 42, y + 42, num, color)
        D.text((x + 72, y + 42), title, bold(26), INK, "lm")

    # --- 卡 1：站到光里
    card(CX1, R1, 1, "站到光里", NAVY)
    x, y = CX1, R1
    draw_sun(img, D, x + 128, y + 164, 44)

    # 光束：从太阳边缘射向人物，做成渐宽的实心梯形，比细线像光
    import math
    scx, scy = x + 128, y + 164
    for a, ln in ((-15, 116), (-1, 130), (13, 122)):
        rad = math.radians(a)
        ca, sa = math.cos(rad), math.sin(rad)
        # 光束起点在球体外（r+6），终点 ln 之外
        sx, sy = scx + (44 + 6) * ca, scy + (44 + 6) * sa
        ex, ey = scx + (44 + ln) * ca, scy + (44 + ln) * sa
        w0, w1 = 2.4, 7.5
        D.poly([(sx - sa * w0, sy + ca * w0), (sx + sa * w0, sy - ca * w0),
                (ex + sa * w1, ey - ca * w1), (ex - sa * w1, ey + ca * w1)],
               fill=(248, 214, 132))

    draw_hero(img, D, x + 350, y + 268, 150)
    D.text((x + 128, y + 258), "阳光即随机源", reg(19), MUTED, "mm")

    draw_module(img, D, x + 96, y + 300, 390, 116, "请站在光里", GOLD, led=False)
    D.text((x + CW / 2, y + 452), "没有光，就没有随机数", reg(18), MUTED)

    # --- 卡 2：按一下
    card(CX2, R1, 2, "按一下", GREEN)
    x, y = CX2, R1
    draw_module(img, D, x + 96, y + 118, 390, 116, "600188", GREEN)
    D.text((x + CW / 2, y + 268), "读取光的硬件随机性", reg(18), MUTED)

    # 按钮 + 向上箭头
    D.poly([(x + CW / 2 - 13, y + 312), (x + CW / 2 + 13, y + 312),
            (x + CW / 2, y + 288)], fill=NAVY)
    D.rr((x + CW / 2 - 104, y + 322, x + CW / 2 + 104, y + 386), 30,
         fill=GREEN, outline=(28, 100, 54), width=1.6)
    D.text((x + CW / 2, y + 354), "按一下", bold(24), WHITE)

    D.text((x + CW / 2, y + 440), "随机 → 庄家也操纵不了", reg(18), MUTED)

    # --- 卡 3：长按切换
    card(CX1, R2, 3, "长按切换", RED)
    x, y = CX1, R2
    D.rr((x + 92, y + 104, x + CW - 92, y + 174), 35, fill=NAVY, outline=NAVY_D, width=1.6)
    D.text((x + CW / 2, y + 139), "随机选股", bold(23), WHITE)

    draw_swap(D, x + CW / 2, y + 222, 38)

    D.rr((x + 92, y + 268, x + CW - 92, y + 338), 35, fill=RED, outline=(150, 34, 28), width=1.6)
    D.text((x + CW / 2, y + 303), "意外操盘", bold(23), WHITE)

    D.text((x + CW / 2, y + 400), "长按 0.9 秒立即切换", reg(18), MUTED)
    D.text((x + CW / 2, y + 434), "不用等松手", reg(17), (150, 132, 100))

    # --- 卡 4：自动休眠
    card(CX2, R2, 4, "自动休眠", NAVY)
    x, y = CX2, R2
    draw_module(img, D, x + 96, y + 132, 390, 116, off=True)

    # Zzz：贴着屏幕右上角往外飘，全部留在卡片内
    for zx, zy, fs in ((432, 116, 19), (456, 84, 25), (486, 48, 32)):
        D.text((x + zx, y + zy), "Z", bold(fs), NAVY_L, "mm")

    D.text((x + CW / 2, y + 350), "空闲 4.5 秒自动熄屏", reg(18), MUTED)

    # 低电量：画一个只剩一格的电池，配一行字
    bx, by, bw, bh = x + 178, y + 382, 52, 26
    D.rr((bx, by, bx + bw, by + bh), 4, outline=(150, 132, 100), width=1.6)
    D.rr((bx + bw + 2, by + bh * 0.32, bx + bw + 8, by + bh * 0.68), 2,
         fill=(150, 132, 100))
    D.rr((bx + 4, by + 4, bx + bw * 0.32, by + bh - 4), 2, fill=RED)
    D.text((bx + bw + 22, by + bh / 2), "低电量直接回休眠", reg(18), MUTED, "lm")

    # ---------------- 借光方案
    bx0, by0, bx1, by1 = 28, 1220, 1212, 1570
    shadow(img, (bx0, by0, bx1, by1), 16, 9, 8, 80)
    D.rr((bx0, by0, bx1, by1), 16, fill=CREAM, outline=NAVY, width=2)

    D.circle(bx0 + 42, by0 + 40, 17, fill=GOLD)
    D.text((bx0 + 42, by0 + 39), "★", bold(20), WHITE)
    D.text((bx0 + 72, by0 + 40), "借光方案", bold(26), INK, "lm")

    # 两台设备：机身加宽，长名字才放得进屏幕
    draw_module(img, D, 106, 1366, 366, 100, "LightStander", (150, 200, 240), led=True)
    draw_module(img, D, 768, 1366, 366, 100, "LightStander", (150, 200, 240), led=True)

    # 中间的光：一条横向光带，中间亮两边淡，两端的箭头指向对方
    mid = 1416
    for dy, wgt in ((0, 12), (-16, 8), (16, 8)):
        D.rr((492, mid + dy - wgt / 2, 748, mid + dy + wgt / 2), wgt / 2,
             fill=(249, 219, 146) if dy else (250, 224, 158))
    for k in range(2):
        # 左端朝右，右端朝左
        D.poly([(500 + k * 18, mid - 7), (516 + k * 18, mid), (500 + k * 18, mid + 7)],
               fill=GOLD)
        D.poly([(740 - k * 18, mid - 7), (724 - k * 18, mid), (740 - k * 18, mid + 7)],
               fill=GOLD)

    bubble(D, (116, 1286, 398, 1340), "兄弟，借点光？", 0, True, reg(19))
    bubble(D, (842, 1286, 1124, 1340), "照你可以，你也得照我", 0, False, reg(19))

    D.text(((bx0 + bx1) / 2, 1500), "两台互相照亮  →  达文西的智慧跨越时空",
           bold(20), INK)
    D.text(((bx0 + bx1) / 2, 1534), "（玩笑归玩笑，逻辑确实自洽）",
           reg(17), (150, 132, 100))

    # ---------------- 流程条
    steps = [("站到光里", NAVY, WHITE), ("按一下", GREEN, WHITE),
             ("拿代码", (146, 100, 22), WHITE), ("它睡了", (32, 42, 54), WHITE),
             ("你也睡", RED, WHITE)]
    fw = (1184 - 4 * 23) / 5
    fy0, fy1 = 1586, 1648
    for i, (txt, bg, fg) in enumerate(steps):
        sx = 28 + i * (fw + 23)
        D.rr((sx, fy0, sx + fw, fy1), 14, fill=bg)
        D.text((sx + fw / 2, (fy0 + fy1) / 2), txt, bold(22), fg)
        if i < 4:
            ax = sx + fw + 4
            D.poly([(ax, (fy0 + fy1) / 2 - 7), (ax + 14, (fy0 + fy1) / 2),
                    (ax, (fy0 + fy1) / 2 + 7)], fill=(150, 120, 40))

    # ---------------- 页脚
    fx0, fy0f, fx1, fy1f = 28, 1662, 1212, 1726
    paste_rr_grad(img, (fx0, fy0f, fx1, fy1f), 14, NAVY, NAVY_D)
    D.rr((fx0, fy0f, fx1, fy1f), 14, outline=(8, 26, 48), width=1.6)
    D.text(((fx0 + fx1) / 2, fy0f + 21), "阳光  →  光敏电阻  →  硬件随机  →  股票代码 / 操盘策略",
           bold(20), GOLD_L)
    D.text(((fx0 + fx1) / 2, fy0f + 46), "不荐股 · 不分析 · 不割韭菜 · 只让你先回到光里",
           reg(17), (168, 190, 214))

    # ---------------- 降采样输出
    img = img.convert("RGB").resize((W, H), Image.LANCZOS)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT, "PNG", optimize=True)
    print("wrote", OUT, img.size, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    build()
