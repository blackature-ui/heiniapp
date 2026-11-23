#!/usr/bin/env python3
"""Erzeugt src/gen/assets.c: Schrift (8x16, normal + fett) und die 48 Profilbilder.

Aufruf: python3 tools/make_assets.py <pfad/zu/heini-app/public> <ausgabe.c>

Schrift: X11-Bitmap-Schrift koi8x16 / koi8x16b (aus den X11-Fonts). ASCII wird übernommen,
Umlaute und andere Latin-1-Buchstaben werden aus Grundbuchstabe + Akzent gebaut, ß, € und die
Symbole (Klo, Klopapier, Herz, ...) sind hier von Hand gezeichnet. Kodierung = Latin-1, die
Symbole liegen auf 0x80..0x9F (siehe src/text.h).
"""
import gzip
import os
import sys

from PIL import Image, PcfFontFile

FONT_DIR = '/usr/share/fonts/X11/cyrillic'

# --- Handgezeichnete Zeichen (8 breit, 16 hoch), '#' = Pixel -----------------------------------
def g(*rows):
    rows = list(rows)
    assert len(rows) == 16, rows
    for r in rows:
        assert len(r) == 8, r
    return rows

E = '........'
CUSTOM = {
    # Code: (Name, Bitmap)
    0x80: ('EURO', g(E, E, E, E, '...####.', '..#....#', '.#......', '#####...', '.#......', '#####...',
                     '.#......', '..#....#', '...####.', E, E, E)),
    0x81: ('KLO', g(E, E, '.####...', '.#..#...', '.#..#...', '.####...', '.#..####', '.#######',
                    '.#######', '..#####.', '...###..', '...##...', '..####..', '..####..', E, E)),
    0x82: ('KLOPAPIER', g(E, E, E, '.####...', '#....#..', '#.##.###', '#.##.#.#', '#....#.#',
                          '.####..#', '.#..#..#', '.#..#..#', '.#..#.#.', '.####.#.', '......#.', E, E)),
    0x83: ('KACKE', g(E, E, '....#...', '...##...', '..####..', '..####..', '.######.', '.#.##.#.',
                      '########', '#.####.#', '########', '.######.', '########', '########', E, E)),
    0x84: ('DUSCHE', g(E, '.....##.', '....#..#', '...#...#', '..###..#', '.#####.#', E, '.#.#.#..',
                       '#.#.#...', '.#.#.#..', '#.#.#...', '.#.#.#..', E, E, E, E)),
    0x85: ('LACHEN', g(E, E, '..####..', '.#....#.', '#.#..#.#', '#......#', '########', '#.####.#',
                       '#..##..#', '.#....#.', '..####..', E, E, E, E, E)),
    0x86: ('HERZ', g(E, E, E, '.##..##.', '########', '########', '########', '.######.', '..####..',
                     '...##...', E, E, E, E, E, E)),
    0x87: ('DAUMEN_HOCH', g(E, E, '...#....', '..##....', '..##....', '.##.....', '########', '##.....#',
                            '##.#####', '##.....#', '##.#####', '##.....#', '#######.', E, E, E)),
    0x88: ('DAUMEN_RUNTER', g(E, E, '#######.', '##.....#', '##.#####', '##.....#', '##.#####', '##.....#',
                              '########', '.##.....', '..##....', '..##....', '...#....', E, E, E)),
    0x89: ('BIER', g(E, E, '.#####..', '#.#.##..', '######..', '#....###', '#.#..#.#', '#.#..#.#',
                     '#.#..#.#', '#.#..###', '#.#..#..', '#....#..', '######..', E, E, E)),
    0x8A: ('POKAL', g(E, E, '########', '########', '.######.', '.######.', '..####..', '...##...',
                      '...##...', '...##...', '..####..', '.######.', '.######.', E, E, E)),
    0x8B: ('KONTROLLER', g(E, E, E, E, '.######.', '#..##..#', '#.#..#.#', '###..###', '#.#..#.#',
                           '#..##..#', '########', '###..###', '.#....#.', E, E, E)),
    0x8C: ('AUTO', g(E, E, E, E, '..####..', '.#.#..#.', '#..#...#', '########', '########',
                     '########', '.##..##.', '.##..##.', E, E, E, E)),
    0x8D: ('BETT', g(E, E, E, '#.......', '#.......', '#.##....', '####....', '#.######', '########',
                     '########', '#......#', '#......#', E, E, E, E)),
    0x8E: ('KRONE', g(E, E, E, '#..##..#', '#..##..#', '##.##.##', '########', '########', '########',
                      '#.#.#.##', '########', E, E, E, E, E)),
    0x8F: ('STERN', g(E, E, '...##...', '...##...', '..####..', '########', '.######.', '..####..',
                      '.######.', '.##..##.', '##....##', E, E, E, E, E)),
    0x90: ('HAKEN', g(E, E, E, E, '.......#', '......##', '.....##.', '#...##..', '##.##...',
                      '.###....', '..#.....', E, E, E, E, E)),
    0x91: ('KREUZ', g(E, E, E, E, '##....##', '.##..##.', '..####..', '...##...', '..####..',
                      '.##..##.', '##....##', E, E, E, E, E)),
    0x92: ('PUNKT', g(E, E, E, E, E, E, '..##....', '.####...', '.####...', '..##....', E, E, E, E, E, E)),
    0x93: ('PFEIL_R', g(E, E, E, '.#......', '.##.....', '.###....', '.####...', '.#####..', '.####...',
                        '.###....', '.##.....', '.#......', E, E, E, E)),
    0x94: ('PFEIL_L', g(E, E, E, '......#.', '.....##.', '....###.', '...####.', '..#####.', '...####.',
                        '....###.', '.....##.', '......#.', E, E, E, E)),
    0x95: ('PFEIL_O', g(E, E, E, E, E, '...##...', '..####..', '.######.', '########', E, E, E, E, E, E, E)),
    0x96: ('PFEIL_U', g(E, E, E, E, E, '########', '.######.', '..####..', '...##...', E, E, E, E, E, E, E)),
    0x97: ('BIRNE', g(E, E, '..####..', '.#....#.', '#......#', '#......#', '#......#', '.#....#.',
                      '..#..#..', '..####..', '..####..', '...##...', E, E, E, E)),
    0x98: ('NADEL', g(E, E, '..####..', '..####..', '...##...', '..####..', '.######.', '########',
                      '...##...', '...##...', '...##...', '...#....', E, E, E, E)),
    0x99: ('UHR', g(E, E, '..####..', '.#..#.#.', '#...#..#', '#...#..#', '#...###.', '#......#',
                    '#......#', '.#....#.', '..####..', E, E, E, E, E)),
    0x9A: ('BOX_LEER', g(E, E, E, '########', '#......#', '#......#', '#......#', '#......#', '#......#',
                         '#......#', '#......#', '########', E, E, E, E)),
    0x9B: ('BOX_VOLL', g(E, E, E, '########', '#......#', '#.....##', '#....###', '##..##.#', '#####..#',
                         '#.##...#', '#......#', '########', E, E, E, E)),
    0x9C: ('KAMERA', g(E, E, E, '..##....', '########', '#..##..#', '#.#..#.#', '#.#..#.#', '#..##..#',
                       '#......#', '########', E, E, E, E, E)),
    0x9D: ('NOTE', g(E, E, '...#####', '...#...#', '...#####', '...#...#', '...#...#', '...#...#',
                     '.###.###', '####.###', '.##...#.', E, E, E, E, E)),
    0x9E: ('HAUS', g(E, E, '...##...', '..####..', '.######.', '########', '.#....#.', '.#.##.#.',
                     '.#.##.#.', '.#.##.#.', '.######.', E, E, E, E, E)),
    0x9F: ('ALARM', g(E, E, '...##...', '..####..', '..####..', '.######.', '.######.', '.######.',
                      '########', '########', E, '...##...', E, E, E, E)),
}

SZ = g(E, E, E, E, '..###...', '.#...#..', '.#...#..', '.#..#...', '.#.##...', '.#...#..',
       '.#....#.', '.#....#.', '.#.###..', E, E, E)

ACCENTS = {
    'diaer': ['.#..#...', E],
    'acute': ['....#...', '...#....'],
    'grave': ['..#.....', '...#....'],
    'circ': ['...##...', '..#..#..'],
    'tilde': ['..##.#..', '.#.##...'],
    'ring': ['...##...', '...##...'],
}
# Latin-1-Code -> (Grundbuchstabe, Akzent)
LATIN1 = {}
for base, acc, up, low in [
    ('A', 'grave', 0xC0, 0xE0), ('A', 'acute', 0xC1, 0xE1), ('A', 'circ', 0xC2, 0xE2), ('A', 'tilde', 0xC3, 0xE3),
    ('A', 'diaer', 0xC4, 0xE4), ('A', 'ring', 0xC5, 0xE5), ('E', 'grave', 0xC8, 0xE8), ('E', 'acute', 0xC9, 0xE9),
    ('E', 'circ', 0xCA, 0xEA), ('E', 'diaer', 0xCB, 0xEB), ('I', 'grave', 0xCC, 0xEC), ('I', 'acute', 0xCD, 0xED),
    ('I', 'circ', 0xCE, 0xEE), ('I', 'diaer', 0xCF, 0xEF), ('N', 'tilde', 0xD1, 0xF1), ('O', 'grave', 0xD2, 0xF2),
    ('O', 'acute', 0xD3, 0xF3), ('O', 'circ', 0xD4, 0xF4), ('O', 'tilde', 0xD5, 0xF5), ('O', 'diaer', 0xD6, 0xF6),
    ('U', 'grave', 0xD9, 0xF9), ('U', 'acute', 0xDA, 0xFA), ('U', 'circ', 0xDB, 0xFB), ('U', 'diaer', 0xDC, 0xFC),
    ('Y', 'acute', 0xDD, 0xFD)]:
    LATIN1[up] = (base, acc)
    LATIN1[low] = (base.lower(), acc)


def load_font(name):
    f = PcfFontFile.PcfFontFile(gzip.open(os.path.join(FONT_DIR, name)), 'iso8859-1')
    glyphs = {}
    for c in range(32, 127):
        gl = f.glyph[c]
        if not gl:
            continue
        im = gl[3]
        w, h = im.size
        rows = []
        for y in range(16):
            r = ''
            for x in range(8):
                r += '#' if (x < w and y < h and im.getpixel((x, y))) else '.'
            rows.append(r)
        glyphs[c] = rows
    return glyphs


def orx(a, b):
    return ''.join('#' if (p == '#' or q == '#') else '.' for p, q in zip(a, b))


def build(glyphs, bold):
    out = {}
    for c in range(256):
        out[c] = [E] * 16
    for c, rows in glyphs.items():
        out[c] = rows
    for c, (base, acc) in LATIN1.items():
        rows = list(glyphs[ord(base)])
        a = ACCENTS[acc]
        if bold:
            a = [orx(r, r[1:] + '.') for r in a]
        top = 2 if base.isupper() else 3 if base not in 'i' else 2
        if base == 'i':
            # Punkt vom i entfernen
            rows = [E if y < 6 else r for y, r in enumerate(rows)]
            top = 3
        if base.isupper() and acc != 'diaer':
            # Großbuchstabe um eine Zeile nach unten schieben, damit der Akzent Platz hat
            rows = [E] + rows[:-1]
            top = 1
        for i, ar in enumerate(a):
            rows[top + i] = orx(rows[top + i], ar)
        out[c] = rows
    sz = SZ
    if bold:
        sz = [orx(r, r[1:] + '.') for r in sz]
    out[0xDF] = sz
    for c, (_, rows) in CUSTOM.items():
        out[c] = rows
    out[0xA0] = [E] * 16
    out[0xB7] = CUSTOM[0x92][1]  # Mittelpunkt
    out[0xB0] = g(E, E, '..##....', '.#..#...', '.#..#...', '..##....', E, E, E, E, E, E, E, E, E, E)  # Grad
    out[0xA7] = glyphs[ord('S')]
    return out


def to_bytes(rows):
    return [int(r.replace('#', '1').replace('.', '0'), 2) for r in rows]


def rgba5551(r, g_, b, a):
    return ((r >> 3) << 11) | ((g_ >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0)


def avatar(path, size):
    im = Image.open(path).convert('RGBA')
    im = im.resize((size, size), Image.LANCZOS)
    px = []
    for y in range(size):
        for x in range(size):
            r, g_, b, a = im.getpixel((x, y))
            px.append(rgba5551(r, g_, b, a))
    return px


def main():
    public, out_path = sys.argv[1], sys.argv[2]
    reg = build(load_font('koi8x16.pcf.gz'), False)
    bold = build(load_font('koi8x16b.pcf.gz'), True)
    lines = ['/* Erzeugt von tools/make_assets.py – nicht von Hand ändern. */', '#include "../assets.h"', '']
    for name, f in (('font_regular', reg), ('font_bold', bold)):
        lines.append('const uint8_t %s[256][16] = {' % name)
        for c in range(256):
            lines.append('  {%s},' % ','.join('0x%02x' % v for v in to_bytes(f[c])))
        lines.append('};')
    for size in (48, 24):
        lines.append('const uint16_t avatar%d[48][%d] = {' % (size, size * size))
        for i in range(1, 49):
            px = avatar(os.path.join(public, 'profilbilder', '%02d.webp' % i), size)
            lines.append('  { /* %02d */' % i)
            for k in range(0, len(px), 24):
                lines.append('    ' + ','.join('0x%04x' % v for v in px[k:k + 24]) + ',')
            lines.append('  },')
        lines.append('};')
    # Startlogo: das App-Icon verkleinert (128x128)
    px = avatar(os.path.join(public, 'icons', 'icon-512.png'), 128)
    lines.append('const uint16_t logo128[128*128] = {')
    for k in range(0, len(px), 32):
        lines.append('  ' + ','.join('0x%04x' % v for v in px[k:k + 32]) + ',')
    lines.append('};')
    with open(out_path, 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    # Kontrollbild der Schrift
    if len(sys.argv) > 3:
        sheet = Image.new('L', (16 * 9 * 2, 16 * 17 * 2), 0)
        for c in range(256):
            for y, r in enumerate(bold[c] if False else reg[c]):
                for x, ch in enumerate(r):
                    if ch == '#':
                        sheet.putpixel(((c % 16) * 9 * 2 + x * 2, (c // 16) * 17 * 2 + y * 2), 255)
        sheet.save(sys.argv[3])


if __name__ == '__main__':
    main()
