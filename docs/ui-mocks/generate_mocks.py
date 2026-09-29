#!/usr/bin/env python3
"""Round 480×480 UI mocks adapted from macropad dark style."""
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import math

OUT = Path(__file__).resolve().parent
BG, TEXT, MUTED, DIM = '#0C0C0E', '#F5F5F7', '#A7A7AE', '#686870'
RED, GREEN, AMBER, PANEL, LINE = '#E11D2E', '#38D996', '#E8A838', '#17171B', '#2A2A30'
W = H = 480
CX = CY = 240
SAFE_R = 210  # content inside circle

font_candidates = [
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
    '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
]
bold_candidates = [
    '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
    '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
]

def font(size, bold=False):
    for path in (bold_candidates if bold else font_candidates):
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()

def text(d, xy, value, size, fill=TEXT, bold=False, anchor='mm'):
    d.text(xy, value, font=font(size, bold), fill=fill, anchor=anchor)

def circle_mask(im):
    mask = Image.new('L', (W, H), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, W - 1, H - 1), fill=255)
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    out.paste(im, mask=mask)
    # dark ring outside for preview on light chat bg
    canvas = Image.new('RGB', (W + 24, H + 24), '#1a1a1e')
    canvas.paste(out.convert('RGB'), (12, 12), out.split()[-1])
    return canvas

def base():
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    # outer soft rim
    d.ellipse((4, 4, W - 5, H - 5), outline=LINE, width=2)
    return im, d

def grok_mark(d, cx, cy, scale=1.0, fill=RED):
    s = scale
    pts = [
        (cx - 28 * s, cy - 18 * s),
        (cx, cy - 36 * s),
        (cx + 28 * s, cy - 18 * s),
        (cx + 18 * s, cy + 22 * s),
        (cx - 18 * s, cy + 22 * s),
    ]
    d.polygon(pts, fill=fill)
    d.line((cx - 12 * s, cy - 2 * s, cx + 12 * s, cy - 2 * s), fill=BG, width=max(2, int(3 * s)))
    d.line((cx - 8 * s, cy + 8 * s, cx + 8 * s, cy + 8 * s), fill=BG, width=max(2, int(2 * s)))

def anim_disk(d, color=PANEL, ring=None):
    r = 88
    d.ellipse((CX - r, CY - r - 20, CX + r, CY + r - 20), fill=color, outline=ring or LINE, width=4)
    return CY - 20

def arc_button(d, angle_deg, label, fill=PANEL, outline=LINE):
    """Button center on lower arc."""
    rad = math.radians(angle_deg)
    r = 165
    bx = CX + r * math.sin(rad)
    by = CY + r * math.cos(rad) * 0.92 + 10
    bw, bh = 78, 36
    d.rounded_rectangle((bx - bw / 2, by - bh / 2, bx + bw / 2, by + bh / 2), radius=18, fill=fill, outline=outline, width=2)
    text(d, (bx, by), label, 13, TEXT, True)

def msg_band(d, title, message, y=318):
    text(d, (CX, y), title, 22, TEXT, True)
    text(d, (CX, y + 28), message, 14, MUTED)

def make_home():
    im, d = base()
    anim_disk(d, PANEL, '#3A7BD5')
    grok_mark(d, CX, CY - 28, 1.15)
    text(d, (CX, 78), 'Grok Bot', 14, MUTED, True)
    msg_band(d, 'round · idle', 'toque um slot ou espere MQTT', 300)
    # online pill
    d.rounded_rectangle((CX - 48, 360, CX + 48, 384), radius=12, fill=PANEL)
    d.ellipse((CX - 38, 367, CX - 28, 377), fill=GREEN)
    text(d, (CX + 6, 372), 'online', 12, TEXT)
    arc_button(d, -50, 'A')
    arc_button(d, 0, 'B')
    arc_button(d, 50, 'C')
    return circle_mask(im)

def make_fleet():
    im, d = base()
    text(d, (CX, 54), 'STATUS', 12, MUTED, True)
    grok_mark(d, CX, CY - 40, 0.7)
    text(d, (CX, CY + 10), 'frota', 12, DIM)
    slots = [
        (-55, 'A', 'PrintMaker', 'Idle', GREEN),
        (0, 'B', 'Fab CAD', 'Working', RED),
        (55, 'C', 'EspForge', 'Stand by', MUTED),
    ]
    for ang, letter, name, st, col in slots:
        rad = math.radians(ang)
        r = 155
        bx = CX + r * math.sin(rad)
        by = CY + 55 + r * math.cos(rad) * 0.35
        d.rounded_rectangle((bx - 70, by - 42, bx + 70, by + 42), radius=12, fill=PANEL, outline=LINE, width=2)
        text(d, (bx - 52, by - 22), letter, 14, RED, True, anchor='lt')
        text(d, (bx, by - 4), name, 13, TEXT, True)
        d.ellipse((bx - 28, by + 16, bx - 18, by + 26), fill=col)
        text(d, (bx + 8, by + 21), st, 11, MUTED)
    text(d, (CX, 430), 'toque um slot', 12, DIM)
    return circle_mask(im)

def make_confirm():
    im, d = base()
    text(d, (CX, 54), 'CONFIRMED', 12, MUTED, True)
    anim_disk(d, PANEL, GREEN)
    d.ellipse((CX - 36, CY - 56, CX + 36, CY + 16), fill=GREEN)
    d.line((CX - 16, CY - 20, CX - 4, CY - 6), fill=BG, width=5)
    d.line((CX - 4, CY - 6, CX + 18, CY - 30), fill=BG, width=5)
    msg_band(d, 'Webhook sent', 'Action completed', 300)
    arc_button(d, -40, 'Back', PANEL, LINE)
    arc_button(d, 40, 'OK', RED, RED)
    return circle_mask(im)

def make_working():
    im, d = base()
    text(d, (CX, 54), 'WORKING', 12, AMBER, True)
    anim_disk(d, PANEL, AMBER)
    # spinning arcs hint
    for a0 in (20, 140, 260):
        d.arc((CX - 70, CY - 90, CX + 70, CY + 50), a0, a0 + 50, fill=AMBER, width=6)
    grok_mark(d, CX, CY - 28, 0.9, AMBER)
    msg_band(d, 'EspForge', 'drivers + HOME…', 300)
    arc_button(d, 0, 'Back')
    return circle_mask(im)

def make_error():
    im, d = base()
    # red rim accent
    d.ellipse((8, 8, W - 9, H - 9), outline=RED, width=6)
    text(d, (CX, 54), 'ERROR', 12, RED, True)
    anim_disk(d, PANEL, RED)
    d.ellipse((CX - 36, CY - 56, CX + 36, CY + 16), fill=RED)
    text(d, (CX, CY - 28), '!', 36, BG, True)
    msg_band(d, 'WiFi timeout', 'toque Retry', 300)
    arc_button(d, -40, 'Back')
    arc_button(d, 40, 'Retry', RED, RED)
    return circle_mask(im)

def make_done():
    im, d = base()
    text(d, (CX, 54), 'DONE', 12, GREEN, True)
    anim_disk(d, PANEL, GREEN)
    grok_mark(d, CX, CY - 28, 1.0, GREEN)
    msg_band(d, 'Pronto', 'v0.1 scaffold OK', 300)
    arc_button(d, 0, 'Home')
    return circle_mask(im)

def main():
    screens = {
        'home': make_home,
        'fleet': make_fleet,
        'confirm': make_confirm,
        'working': make_working,
        'done': make_done,
        'error': make_error,
    }
    for name, fn in screens.items():
        im = fn()
        path = OUT / f'grokbot-round-{name}-v0.2.png'
        im.save(path)
        print(path.name, im.size)
    (OUT / 'README.md').write_text(
        'Mocks circulares 480×480 (preview com margem) adaptados do estilo macropad v0.1.\n'
        'Layout: centro = animação Grok Bot; faixa média = título/mensagem; arco inferior = botões touch.\n'
        'Gerar: `python3 generate_mocks.py`\n',
        encoding='utf-8',
    )

if __name__ == '__main__':
    main()
