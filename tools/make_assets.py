#!/usr/bin/env python3
import warnings; warnings.filterwarnings("ignore")
"""make_assets.py – erzeugt die Grafik der Heini-App 64 aus dem Programmcode der Heini-App.

Aufruf: python3 tools/make_assets.py <heini-app/public> <src/gen>

- Schrift: Roboto Flex (public/fonts, OFL) in mehreren Größen und Stärken, 4-Bit-Kantenglättung
- Symbole: die Lucide-Pfade aus index.html (ICON_PFADE, ISC-Lizenz), mit rsvg-convert gerastert
- Emojis: Noto Color Emoji (OFL) für alle Emojis aus den Daten (src/gen/emoji.txt, von export2c.js) und der Oberfläche
- Profilbilder: public/profilbilder/NN.webp als Figur (132 % der Kreisgröße wie .ava.figur), RGBA4444
- Farben: Material-You-Schema je Profilfarbe, hell und dunkel, aus dem CSS in index.html
Ausgabe: src/gen/assets.c und src/gen/assets_gen.h
"""
import io
import json
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

public, gen = sys.argv[1], sys.argv[2]
index = open(os.path.join(public, 'index.html'), encoding='utf-8').read()
EMOJI_FONT = '/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf'

out_c, out_h = [], []
def c(s): out_c.append(s)
def h(s): out_h.append(s)

def arr_u8(name, data):
    c('static const uint8_t %s[%d] = {' % (name, max(1, len(data))))
    for i in range(0, len(data), 32):
        c('  ' + ','.join(str(b) for b in data[i:i + 32]) + ',')
    if not data: c('  0')
    c('};')

def arr_u16(name, data, static=True):
    c('%sconst uint16_t %s[%d] = {' % ('static ' if static else '', name, max(1, len(data))))
    for i in range(0, len(data), 24):
        c('  ' + ','.join('0x%04x' % v for v in data[i:i + 24]) + ',')
    if not data: c('  0')
    c('};')

def nibbles(vals):
    vals = list(vals)
    if len(vals) % 2: vals.append(0)
    return [(vals[i] << 4) | vals[i + 1] for i in range(0, len(vals), 2)]

def rgba4444(im):
    im = im.convert('RGBA')
    return [((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4) for r, g, b, a in im.getdata()]

# ------------------------------------------------------------------ Schrift
SCHRIFT = os.path.join(public, 'fonts', 'roboto-flex.woff2')
EXTRA = '„“”‚‘’–—…€•→←×✓·−›‹⋮'
ZEICHEN = [chr(i) for i in range(32, 127)] + [chr(i) for i in range(160, 256)] + list(EXTRA)
ZIFFERN = list('0123456789:.,-+ %€/hMinSek')
SCHRIFTEN = [   # Name, Größe, Stärke, Zeichen
    ('F_KLEIN', 13, 450, ZEICHEN),
    ('F_NORMAL', 16, 420, ZEICHEN),
    ('F_FETT', 16, 650, ZEICHEN),
    ('F_TITEL', 19, 720, ZEICHEN),
    ('F_KOPF', 27, 400, ZEICHEN),
    ('F_GROSS', 32, 800, ZEICHEN),
    ('F_UHR', 52, 700, ZIFFERN + list('läuft offen')),
]
h('enum { ' + ', '.join(s[0] for s in SCHRIFTEN) + ', F_ANZAHL };')
schrift_namen = []
for name, groesse, staerke, zeichen in SCHRIFTEN:
    f = ImageFont.truetype(SCHRIFT, groesse)
    try: f.set_variation_by_axes([staerke])
    except Exception: pass
    asc, desc = f.getmetrics()
    hoehe = asc + desc
    glyphen, daten = [], []
    for ch in sorted(set(zeichen), key=ord):
        adv = int(round(f.getlength(ch)))
        l, t, r, b = f.getbbox(ch)
        x0 = min(0, l); x1 = max(adv, r)
        w = max(1, x1 - x0)
        im = Image.new('L', (w, hoehe), 0)
        ImageDraw.Draw(im).text((-x0, 0), ch, font=f, fill=255)
        px = [min(15, (v + 8) >> 4) for v in im.getdata()]
        glyphen.append((ord(ch), x0, w, adv, len(daten) * 2))
        daten += nibbles(px)
    arr_u8('sd_%s' % name, daten)
    c('static const Glyph sg_%s[] = {' % name)
    for cp, ox, w, adv, off in glyphen:
        c('  {%d, %d, %d, %d, %d},' % (cp, ox, w, adv, off // 2))
    c('};')
    schrift_namen.append((name, hoehe, asc, len(glyphen)))
c('const Schrift SCHRIFTEN[F_ANZAHL] = {')
for name, hoehe, asc, n in schrift_namen:
    c('  {%d, %d, %d, sg_%s, sd_%s},' % (hoehe, asc, n, name, name))
c('};')

# ------------------------------------------------------------------ Symbole (Lucide)
pfade = json.loads(subprocess.check_output(['node', '-e', r'''
const fs = require('fs'); const s = fs.readFileSync(process.argv[1], 'utf8');
const a = s.indexOf('const ICON_PFADE = {'), z = s.indexOf('const hsym');
let code = s.slice(a, z).split('\n').filter(l => /^(const ICON_PFADE|ICON_PFADE|\s+['"a-z0-9-]+\s*:|\};)/.test(l) || /^\s+'/.test(l)).join('\n');
eval(code + '; process.stdout.write(JSON.stringify(ICON_PFADE))');
''', os.path.join(public, 'index.html')]).decode())
namen = sorted(pfade)
GROESSEN = [20, 28]
h('enum { ' + ', '.join('IC_' + re.sub('[^A-Za-z0-9]', '_', n).upper() for n in namen) + ', IC_ANZAHL };')
h('#define IC_GROESSEN %d' % len(GROESSEN))
h('extern const uint8_t IC_PX[IC_GROESSEN];')
c('const uint8_t IC_PX[IC_GROESSEN] = {%s};' % ','.join(map(str, GROESSEN)))
for g in GROESSEN:
    alle = []
    for n in namen:
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 24 24" fill="none" stroke="white" '
               'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">%s</svg>') % (g, g, pfade[n])
        png = subprocess.run(['rsvg-convert', '-w', str(g), '-h', str(g)], input=svg.encode(), capture_output=True, check=True).stdout
        im = Image.open(io.BytesIO(png)).convert('RGBA').getchannel('A')
        alle += nibbles(min(15, (v + 8) >> 4) for v in im.getdata())
    arr_u8('ic%d' % g, alle)
c('const uint8_t *const IC_DATEN[IC_GROESSEN] = {%s};' % ','.join('ic%d' % g for g in GROESSEN))
h('extern const uint8_t *const IC_DATEN[IC_GROESSEN];')

# ------------------------------------------------------------------ Emojis
ui_emojis = '⏰🐌🎟🧭👻🤷🌅🦉🐣🗺💶📷🎤❔🏃🚽🧻💩🚿😂❤🍺🏆🥇🎉👋🦸🙈⏳🎮🚗🛏👑⭐✨✅❌📌🕒📸🎵🏠🚨🔔💡🍕🛒🥤🎫📦🎯🔥💯👍👎😍😘😊🙂😅🤣😁😎🤔😢😭😡🥳🙏💪👏🤝🍻🍷🥂🍾🍔🍟🌭🍿☕🎂🎁🎄🎅⛄❄🌞🌙⚡💤🧽🪥🧴🛁🚪🔑📅🗓️🎲🃏🎳🎤🎧📺💻📱🍀🌈💕💛💚💙💜🖤🤍🤎🧡🐣🥉🥈🎖💯📅🗓🏡🛏🏨🎫🔗🌳🪨📆🦸🎢🚲🧳🏟🔙🎆🔥⏳🏰👯🌅🦉🎯⚡🏃🔁🚨🏠🎟💛📸💬⏱🧭👻🤷💩🙈👑'
liste = set()
pfad = os.path.join(gen, 'emoji.txt')
if os.path.exists(pfad):
    for z in open(pfad, encoding='utf-8'):
        z = z.strip()
        if z: liste.add(int(z, 16))
for ch in ui_emojis:
    if ord(ch) >= 0x2100 and ord(ch) not in (0xFE0F, 0x200D): liste.add(ord(ch))
ef = ImageFont.truetype(EMOJI_FONT, 109)
emojis = []
for cp in sorted(liste):
    im = Image.new('RGBA', (160, 160), (0, 0, 0, 0))
    try:
        ImageDraw.Draw(im).text((10, 10), chr(cp), font=ef, embedded_color=True)
    except Exception:
        continue
    box = im.getbbox()
    if not box: continue
    im = im.crop(box)
    s = max(im.size)
    q = Image.new('RGBA', (s, s), (0, 0, 0, 0)); q.alpha_composite(im, ((s - im.width) // 2, (s - im.height) // 2))
    emojis.append((cp, q))
EGR = [22, 36]
h('#define EMOJI_GROESSEN %d' % len(EGR))
h('extern const uint8_t EMOJI_PX[EMOJI_GROESSEN];')
c('const uint8_t EMOJI_PX[EMOJI_GROESSEN] = {%s};' % ','.join(map(str, EGR)))
c('const uint32_t EMOJI_CP[] = {%s};' % ','.join('0x%x' % cp for cp, _ in emojis))
c('const int EMOJI_N = %d;' % len(emojis))
h('extern const uint32_t EMOJI_CP[]; extern const int EMOJI_N;')
for g in EGR:
    alle = []
    for cp, q in emojis: alle += rgba4444(q.resize((g, g), Image.LANCZOS))
    arr_u16('em%d' % g, alle)
c('const uint16_t *const EMOJI_DATEN[EMOJI_GROESSEN] = {%s};' % ','.join('em%d' % g for g in EGR))
h('extern const uint16_t *const EMOJI_DATEN[EMOJI_GROESSEN];')
# großes Klopapier für den Alarm
im = Image.new('RGBA', (160, 160), (0, 0, 0, 0)); ImageDraw.Draw(im).text((10, 10), '🧻', font=ef, embedded_color=True)
im = im.crop(im.getbbox()).resize((96, 96), Image.LANCZOS)
arr_u16('klopapier96', rgba4444(im), static=False)
h('extern const uint16_t klopapier96[96 * 96];')

# ------------------------------------------------------------------ Profilbilder (Figur)
AGR = [52, 36]   # Figur zu Kreis 40 bzw. 28
h('#define AVA_GROESSEN 2')
h('extern const uint8_t AVA_PX[AVA_GROESSEN];')
c('const uint8_t AVA_PX[AVA_GROESSEN] = {52, 36};')
for g in AGR:
    alle = []
    for i in range(1, 49):
        im = Image.open(os.path.join(public, 'profilbilder', '%02d.webp' % i)).convert('RGBA')
        alle += rgba4444(im.resize((g, g), Image.LANCZOS))
    arr_u16('ava%d' % g, alle)
c('const uint16_t *const AVA_DATEN[AVA_GROESSEN] = {ava52, ava36};')
h('extern const uint16_t *const AVA_DATEN[AVA_GROESSEN];')

# ------------------------------------------------------------------ Logo
im = Image.open(os.path.join(public, 'icons', 'icon-512.png')).convert('RGBA').resize((128, 128), Image.LANCZOS)
arr_u16('logo128', rgba4444(im), static=False)
h('extern const uint16_t logo128[128 * 128];')

# ------------------------------------------------------------------ Farben
def rgb5551(hx):
    hx = hx.strip().lstrip('#')
    if len(hx) == 3: hx = ''.join(ch * 2 for ch in hx)
    r, g, b = int(hx[0:2], 16), int(hx[2:4], 16), int(hx[4:6], 16)
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1
def vars_von(block):
    return dict(re.findall(r'--([A-Za-z0-9]+):\s*(#[0-9a-fA-F]{3,6})', block))
basis_hell = vars_von(re.search(r':root \{(.*?)\n\}', index, re.S).group(1))
basis_dunkel = vars_von(re.search(r':root\[data-theme=dark\] \{(.*?)\n\}', index, re.S).group(1))
for m in re.finditer(r':root(\[data-theme=dark\])? \{ (--gold[^}]*)\}', index):
    (basis_dunkel if m.group(1) else basis_hell).update(vars_von(m.group(2)))
farben = json.loads(subprocess.check_output(['node', '-e', 'process.stdout.write(JSON.stringify(require(process.argv[1]).WERTE.farben))',
                                              os.path.join(public, 'rules.js')]).decode())
FELDER = ['oben', 'onOben', 'bg', 'flaeche', 'flaeche2', 'line', 'edge', 'text', 'muted', 'faint', 'accent', 'onAccent', 'accentText',
          'accentCont', 'onAccentCont', 'ok', 'okBg', 'warn', 'warnBg', 'err', 'errBg', 'alarm', 'onAlarm', 'alarmBg',
          'goldBg1', 'goldBg2', 'goldRand', 'goldText', 'goldStern']
h('typedef struct { uint16_t ' + ', '.join(FELDER) + '; } Thema;')
h('extern const Thema THEMEN[%d][2];   /* [Profilfarbe][0 hell, 1 dunkel] */' % len(farben))
c('const Thema THEMEN[%d][2] = {' % len(farben))
for f in farben:
    zeile = []
    for dunkel in (0, 1):
        m = re.search(r':root\[data-farbe=%s\]%s \{(.*?)\}' % (f, r'\[data-theme=dark\]' if dunkel else ''), index)
        v = dict(basis_dunkel if dunkel else basis_hell)
        v.update(vars_von(m.group(1)))
        zeile.append('{' + ','.join('0x%04x' % rgb5551(v.get(k, '#ff00ff')) for k in FELDER) + '}')
    c('  {' + ', '.join(zeile) + '},')
c('};')

# ------------------------------------------------------------------ schreiben
kopf_h = ['/* Erzeugt von tools/make_assets.py – nicht von Hand ändern */', '#ifndef ASSETS_GEN_H', '#define ASSETS_GEN_H', '#include <stdint.h>',
          'typedef struct { uint16_t cp; int8_t ox; uint8_t w, adv; uint32_t off; } Glyph;',
          'typedef struct { uint8_t hoehe, ascent; uint16_t n; const Glyph *g; const uint8_t *daten; } Schrift;']
h('extern const Schrift SCHRIFTEN[F_ANZAHL];')
open(os.path.join(gen, 'assets_gen.h'), 'w').write('\n'.join(kopf_h + out_h + ['#endif']) + '\n')
open(os.path.join(gen, 'assets.c'), 'w').write('/* Erzeugt von tools/make_assets.py – nicht von Hand ändern */\n#include "assets_gen.h"\n' + '\n'.join(out_c) + '\n')
print('Schriften %d, Symbole %d, Emojis %d' % (len(SCHRIFTEN), len(namen), len(emojis)))
