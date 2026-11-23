/* gfx.c – Software-Zeichnen in den Bildpuffer. Bewusst einfach: die CPU der N64 schafft das locker,
 * weil nur bei Änderungen neu gezeichnet wird. */
#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "assets.h"
#include "text.h"

Leinwand lw;

void gfx_start(uint16_t *px, int zeile) {
    lw.px = px; lw.zeile = zeile;
    gfx_clip_aus();
}
void gfx_clip(int x0, int y0, int x1, int y1) {
    lw.clip_x0 = x0 < 0 ? 0 : x0; lw.clip_y0 = y0 < 0 ? 0 : y0;
    lw.clip_x1 = x1 > BILD_B ? BILD_B : x1; lw.clip_y1 = y1 > BILD_H ? BILD_H : y1;
}
void gfx_clip_aus(void) { gfx_clip(0, 0, BILD_B, BILD_H); }

void gfx_fuellen(uint16_t farbe) {
    uint64_t f = farbe; f |= f << 16; f |= f << 32;
    for (int y = 0; y < BILD_H; y++) {
        uint64_t *p = (uint64_t *)(void *)(lw.px + y * lw.zeile);
        for (int x = 0; x < BILD_B / 4; x++) p[x] = f;
    }
}

void gfx_rechteck(int x, int y, int b, int h, uint16_t farbe) {
    int x0 = x, y0 = y, x1 = x + b, y1 = y + h;
    if (x0 < lw.clip_x0) x0 = lw.clip_x0;
    if (y0 < lw.clip_y0) y0 = lw.clip_y0;
    if (x1 > lw.clip_x1) x1 = lw.clip_x1;
    if (y1 > lw.clip_y1) y1 = lw.clip_y1;
    uint32_t f2 = (uint32_t)farbe << 16 | farbe;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *p = lw.px + yy * lw.zeile;
        int xx = x0;
        if ((xx & 1) && xx < x1) p[xx++] = farbe;
        uint32_t *q = (uint32_t *)(void *)(p + xx);
        for (; xx + 1 < x1; xx += 2) *q++ = f2;
        if (xx < x1) p[xx] = farbe;
    }
}
void gfx_linie_h(int x, int y, int b, uint16_t farbe) { gfx_rechteck(x, y, b, 1, farbe); }
void gfx_linie_v(int x, int y, int h, uint16_t farbe) { gfx_rechteck(x, y, 1, h, farbe); }
void gfx_rahmen(int x, int y, int b, int h, uint16_t farbe) {
    gfx_linie_h(x, y, b, farbe); gfx_linie_h(x, y + h - 1, b, farbe);
    gfx_linie_v(x, y, h, farbe); gfx_linie_v(x + b - 1, y, h, farbe);
}

static inline void punkt(int x, int y, uint16_t f) {
    if (x >= lw.clip_x0 && x < lw.clip_x1 && y >= lw.clip_y0 && y < lw.clip_y1) lw.px[y * lw.zeile + x] = f;
}

void gfx_runde_box(int x, int y, int b, int h, int r, uint16_t farbe) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > b) r = b / 2;
    gfx_rechteck(x, y + r, b, h - 2 * r, farbe);
    for (int i = 0; i < r; i++) {
        /* Einzug der Zeile i vom oberen Rand (Viertelkreis) */
        int dy = r - i, dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
        int ein = r - dx;
        gfx_rechteck(x + ein, y + i, b - 2 * ein, 1, farbe);
        gfx_rechteck(x + ein, y + h - 1 - i, b - 2 * ein, 1, farbe);
    }
}

void gfx_kreis(int cx, int cy, int r, uint16_t farbe) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
        gfx_rechteck(cx - dx, cy + dy, 2 * dx + 1, 1, farbe);
    }
}

void gfx_bild(int x, int y, int b, int h, const uint16_t *px) {
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < b; xx++) {
            uint16_t c = px[yy * b + xx];
            if (c & 1) punkt(x + xx, y + yy, c);
        }
}

void gfx_bild_rund(int x, int y, int b, const uint16_t *px, uint16_t rand) {
    int r = b / 2;
    gfx_kreis(x + r, y + r, r + 2, rand);
    for (int yy = 0; yy < b; yy++)
        for (int xx = 0; xx < b; xx++) {
            int dx = 2 * xx + 1 - b, dy = 2 * yy + 1 - b;
            if (dx * dx + dy * dy > b * b) continue;
            uint16_t c = px[yy * b + xx];
            punkt(x + xx, y + yy, (c & 1) ? c : rand);
        }
}

uint16_t gfx_mischen(uint16_t a, uint16_t b, int t) {
    int ra = a >> 11, ga = (a >> 6) & 31, ba = (a >> 1) & 31;
    int rb = b >> 11, gb = (b >> 6) & 31, bb = (b >> 1) & 31;
    int r = ra + ((rb - ra) * t >> 8), g = ga + ((gb - ga) * t >> 8), bl = ba + ((bb - ba) * t >> 8);
    return (uint16_t)((r << 11) | (g << 6) | (bl << 1) | 1);
}

/* ---------- Text ---------- */
static void zeichen(int x, int y, unsigned char c, uint16_t farbe, int fett, int gross) {
    const uint8_t *g = fett ? font_bold[c] : font_regular[c];
    if (x >= lw.clip_x1 || y >= lw.clip_y1 || x + 8 * gross <= lw.clip_x0 || y + 16 * gross <= lw.clip_y0) return;
    if (gross == 1 && x >= lw.clip_x0 && x + 8 <= lw.clip_x1 && y >= lw.clip_y0 && y + 16 <= lw.clip_y1) {
        /* schneller Weg: ganz sichtbar, ohne Prüfung je Pixel */
        uint16_t *p = lw.px + y * lw.zeile + x;
        for (int yy = 0; yy < 16; yy++, p += lw.zeile) {
            uint8_t z = g[yy];
            if (!z) continue;
            if (z & 0x80) p[0] = farbe;
            if (z & 0x40) p[1] = farbe;
            if (z & 0x20) p[2] = farbe;
            if (z & 0x10) p[3] = farbe;
            if (z & 0x08) p[4] = farbe;
            if (z & 0x04) p[5] = farbe;
            if (z & 0x02) p[6] = farbe;
            if (z & 0x01) p[7] = farbe;
        }
        return;
    }
    for (int yy = 0; yy < 16; yy++) {
        uint8_t z = g[yy];
        if (!z) continue;
        for (int xx = 0; xx < 8; xx++) {
            if (!(z & (0x80 >> xx))) continue;
            if (gross == 1) punkt(x + xx, y + yy, farbe);
            else gfx_rechteck(x + xx * gross, y + yy * gross, gross, gross, farbe);
        }
    }
}

/* Zeichen außerhalb von Latin-1 auf die Schrift abbilden */
static int abbilden(int c) {
    switch (c) {
    case 0x201E: case 0x201C: case 0x201D: case 0x00AB: case 0x00BB: return '"';
    case 0x201A: case 0x2018: case 0x2019: return '\'';
    case 0x2013: case 0x2014: case 0x2212: case 0x2010: case 0x2011: return '-';
    case 0x2192: case 0x25B6: return 0x93;
    case 0x2190: case 0x25C0: return 0x94;
    case 0x2022: return 0xB7;
    case 0x20AC: return 0x80;
    case 0x2026: return 0xB7;
    case 0x00A0: case 0x202F: case 0x2009: return ' ';
    }
    return c > 255 ? '?' : c;
}

/* n = höchstens n Zeichen (nicht Bytes) */
int gfx_text_n(int x, int y, const char *s, int n, uint16_t farbe, int fett, int gross) {
    int x0 = x, i = 0, c;
    while (n-- > 0 && (c = utf8_naechstes(s, &i)) != 0) {
        if (c == '\n') break;
        zeichen(x, y, (unsigned char)abbilden(c), farbe, fett, gross);
        x += 8 * gross;
    }
    return x - x0;
}
int gfx_text(int x, int y, const char *s, uint16_t farbe, int fett, int gross) {
    return gfx_text_n(x, y, s, 1 << 30, farbe, fett, gross);
}
static int zeichen_bis_umbruch(const char *s) {
    int i = 0, n = 0, c;
    while ((c = utf8_naechstes(s, &i)) != 0 && c != '\n') n++;
    return n;
}
int gfx_textbreite(const char *s, int gross) { return zeichen_bis_umbruch(s) * 8 * gross; }
int gfx_text_max(int x, int y, const char *s, int max_b, uint16_t farbe, int fett) {
    int n = zeichen_bis_umbruch(s), max_n = max_b / 8;
    if (n <= max_n) return gfx_text_n(x, y, s, n, farbe, fett, 1);
    if (max_n < 4) return gfx_text_n(x, y, s, max_n, farbe, fett, 1);
    int b = gfx_text_n(x, y, s, max_n - 3, farbe, fett, 1);
    return b + gfx_text(x + b, y, "...", farbe, fett, 1);
}
int gfx_text_mitte(int cx, int y, const char *s, uint16_t farbe, int fett, int gross) {
    int b = gfx_textbreite(s, gross);
    return gfx_text(cx - b / 2, y, s, farbe, fett, gross);
}
