/* gfx.c – Software-Zeichnen in den Bildpuffer */
#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "text.h"

Leinwand lw;
const Thema *TH = &THEMEN[0][1];
void thema_setzen(int farbe, int dunkel) { if (farbe < 0 || farbe >= 20) farbe = 0; TH = &THEMEN[farbe][dunkel ? 1 : 0]; }

void gfx_start(uint16_t *px, int zeile) { lw.px = px; lw.zeile = zeile; gfx_clip_aus(); }
void gfx_clip(int x0, int y0, int x1, int y1) {
    lw.clip_x0 = x0 < 0 ? 0 : x0; lw.clip_y0 = y0 < 0 ? 0 : y0;
    lw.clip_x1 = x1 > BILD_B ? BILD_B : x1; lw.clip_y1 = y1 > BILD_H ? BILD_H : y1;
}
void gfx_clip_aus(void) { gfx_clip(0, 0, BILD_B, BILD_H); }

void gfx_rechteck(int x, int y, int b, int h, uint16_t farbe) {
    int x0 = x, y0 = y, x1 = x + b, y1 = y + h;
    if (x0 < lw.clip_x0) x0 = lw.clip_x0;
    if (y0 < lw.clip_y0) y0 = lw.clip_y0;
    if (x1 > lw.clip_x1) x1 = lw.clip_x1;
    if (y1 > lw.clip_y1) y1 = lw.clip_y1;
    if (x0 >= x1) return;
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
void gfx_fuellen(uint16_t farbe) { gfx_rechteck(lw.clip_x0, lw.clip_y0, lw.clip_x1 - lw.clip_x0, lw.clip_y1 - lw.clip_y0, farbe); }
void gfx_linie_h(int x, int y, int b, uint16_t farbe) { gfx_rechteck(x, y, b, 1, farbe); }
void gfx_linie_v(int x, int y, int h, uint16_t farbe) { gfx_rechteck(x, y, 1, h, farbe); }
void gfx_rahmen(int x, int y, int b, int h, uint16_t farbe) {
    gfx_linie_h(x, y, b, farbe); gfx_linie_h(x, y + h - 1, b, farbe);
    gfx_linie_v(x, y, h, farbe); gfx_linie_v(x + b - 1, y, h, farbe);
}

static inline void punkt(int x, int y, uint16_t f) {
    if (x >= lw.clip_x0 && x < lw.clip_x1 && y >= lw.clip_y0 && y < lw.clip_y1) lw.px[y * lw.zeile + x] = f;
}
uint16_t gfx_mischen(uint16_t a, uint16_t b, int t) {
    int ra = a >> 11, ga = (a >> 6) & 31, ba = (a >> 1) & 31;
    int rb = b >> 11, gb = (b >> 6) & 31, bb = (b >> 1) & 31;
    int r = ra + ((rb - ra) * t >> 8), g = ga + ((gb - ga) * t >> 8), bl = ba + ((bb - ba) * t >> 8);
    return (uint16_t)((r << 11) | (g << 6) | (bl << 1) | 1);
}
static inline void punkt_alpha(int x, int y, uint16_t f, int a15) {
    if (x < lw.clip_x0 || x >= lw.clip_x1 || y < lw.clip_y0 || y >= lw.clip_y1 || !a15) return;
    uint16_t *p = lw.px + y * lw.zeile + x;
    *p = a15 >= 15 ? f : gfx_mischen(*p, f, a15 * 17);
}

/* Ecken mit Kantenglättung: Abdeckung je Randpixel über 4x4 Unterabtastung */
static int deckung(int dx2, int dy2, int r2x4) {
    /* dx2, dy2: Abstand der Pixelmitte zum Kreismittelpunkt in Viertelpixeln */
    int n = 0;
    for (int sy = -1; sy <= 2; sy += 1) for (int sx = -1; sx <= 2; sx += 1) {
        int x = dx2 + sx * 2 - 1, y = dy2 + sy * 2 - 1;
        if (x * x + y * y <= r2x4) n++;
    }
    return n;   /* 0..16 */
}
void gfx_runde_box(int x, int y, int b, int h, int r, uint16_t farbe) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > b) r = b / 2;
    if (r <= 0) { gfx_rechteck(x, y, b, h, farbe); return; }
    gfx_rechteck(x, y + r, b, h - 2 * r, farbe);
    int r2 = (r * 4) * (r * 4);
    for (int i = 0; i < r; i++) {
        /* Zeile i (von oben), Mittelpunkt der Ecke bei (r, r) */
        int dy = (r - i) * 4 - 2, voll = -1;
        for (int j = 0; j < r; j++) {
            int dx = (r - j) * 4 - 2;
            int d = deckung(dx, dy, r2);
            if (d >= 16) { voll = j; break; }
            if (d) {
                int a = d * 15 / 16;
                punkt_alpha(x + j, y + i, farbe, a); punkt_alpha(x + b - 1 - j, y + i, farbe, a);
                punkt_alpha(x + j, y + h - 1 - i, farbe, a); punkt_alpha(x + b - 1 - j, y + h - 1 - i, farbe, a);
            }
        }
        if (voll < 0) voll = r;
        gfx_rechteck(x + voll, y + i, b - 2 * voll, 1, farbe);
        gfx_rechteck(x + voll, y + h - 1 - i, b - 2 * voll, 1, farbe);
    }
}
void gfx_runder_rahmen(int x, int y, int b, int h, int r, int d, uint16_t farbe) {
    /* einfacher Rahmen: äußere Box minus innere über Zeilenspannen */
    if (r * 2 > h) r = h / 2;
    if (r * 2 > b) r = b / 2;
    for (int i = 0; i < h; i++) {
        int yy = y + i;
        int ein = 0, ein2 = -1;
        int dy = i < r ? r - i : (i >= h - r ? i - (h - 1 - r) : 0);
        if (dy) { int dx = 0; while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++; ein = r - dx; }
        int ri = r - d, iy = i - d, ih = h - 2 * d;
        if (iy >= 0 && iy < ih) {
            int dyi = ri > 0 ? (iy < ri ? ri - iy : (iy >= ih - ri ? iy - (ih - 1 - ri) : 0)) : 0;
            int dx = 0;
            if (dyi && ri > 0) { while ((dx + 1) * (dx + 1) + dyi * dyi <= ri * ri) dx++; ein2 = d + ri - dx; }
            else ein2 = d;
        }
        if (ein2 < 0) gfx_rechteck(x + ein, yy, b - 2 * ein, 1, farbe);
        else { gfx_rechteck(x + ein, yy, ein2 - ein, 1, farbe); gfx_rechteck(x + b - ein2, yy, ein2 - ein, 1, farbe); }
    }
}
void gfx_verlauf_box(int x, int y, int b, int h, int r, uint16_t oben, uint16_t unten) {
    gfx_runde_box(x, y, b, h, r, oben);
    int x0 = x + r / 3, b0 = b - 2 * (r / 3);
    for (int i = r; i < h - r; i++) gfx_rechteck(x, y + i, b, 1, gfx_mischen(oben, unten, (i * 256) / (h ? h : 1)));
    (void)x0; (void)b0;
}
void gfx_kreis(int cx, int cy, int r, uint16_t farbe) { gfx_runde_box(cx - r, cy - r, 2 * r + 1, 2 * r + 1, r, farbe); }
void gfx_abdunkeln(int x, int y, int b, int h, int t) {
    for (int yy = y; yy < y + h; yy++) {
        if (yy < lw.clip_y0 || yy >= lw.clip_y1) continue;
        uint16_t *p = lw.px + yy * lw.zeile;
        for (int xx = x < lw.clip_x0 ? lw.clip_x0 : x; xx < x + b && xx < lw.clip_x1; xx++) p[xx] = gfx_mischen(p[xx], C_SCHWARZ, t);
    }
}

void gfx_bild4444(int x, int y, int b, int h, const uint16_t *px) {
    for (int yy = 0; yy < h; yy++) {
        int sy = y + yy;
        if (sy < lw.clip_y0 || sy >= lw.clip_y1) continue;
        uint16_t *p = lw.px + sy * lw.zeile;
        const uint16_t *q = px + yy * b;
        for (int xx = 0; xx < b; xx++) {
            uint16_t c = q[xx];
            int a = c & 15, sx = x + xx;
            if (!a || sx < lw.clip_x0 || sx >= lw.clip_x1) continue;
            int r = c >> 12, g = (c >> 8) & 15, bl = (c >> 4) & 15;
            uint16_t f = (uint16_t)(((r << 1 | r >> 3) << 11) | ((g << 1 | g >> 3) << 6) | ((bl << 1 | bl >> 3) << 1) | 1);
            p[sx] = a == 15 ? f : gfx_mischen(p[sx], f, a * 17);
        }
    }
}

/* ---------- Symbole ---------- */
void gfx_symbol(int id, int x, int y, int g, uint16_t farbe) {
    if (id < 0 || id >= IC_ANZAHL) return;
    int k = g >= 26 ? 1 : 0;
    int n = IC_PX[k];
    const uint8_t *d = IC_DATEN[k] + (size_t)id * (size_t)(n * n / 2);
    for (int yy = 0; yy < n; yy++)
        for (int xx = 0; xx < n; xx++) {
            int i = yy * n + xx;
            int a = (i & 1) ? (d[i >> 1] & 15) : (d[i >> 1] >> 4);
            if (a) punkt_alpha(x + xx, y + yy, farbe, a);
        }
}

/* ---------- Emojis ---------- */
int emoji_index(uint32_t cp) {
    int lo = 0, hi = EMOJI_N - 1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (EMOJI_CP[m] == cp) return m; if (EMOJI_CP[m] < cp) lo = m + 1; else hi = m - 1; }
    return -1;
}
void gfx_emoji(int i, int x, int y, int g) {
    if (i < 0) return;
    int k = g >= 30 ? 1 : 0, n = EMOJI_PX[k];
    gfx_bild4444(x, y, n, n, EMOJI_DATEN[k] + (size_t)i * (size_t)(n * n));
}

/* ---------- Text ---------- */
/* Symbol-Codes der App (U+0080..U+009F, siehe text.h): Lucide-Symbol (>= 0) oder Emoji (Codepunkt | 0x80000000) */
static const int32_t SYM[32] = {
    0x20AC, IC_BATH, (int32_t)(0x1F9FB | 0x80000000u), (int32_t)(0x1F4A9 | 0x80000000u), (int32_t)(0x1F6BF | 0x80000000u), (int32_t)(0x1F602 | 0x80000000u),
    IC_HEART, IC_THUMBS_UP, (int32_t)(0x1F44E | 0x80000000u), IC_BEER, IC_TROPHY, IC_GAMEPAD_2, IC_CAR, IC_BED, (int32_t)(0x1F451 | 0x80000000u), IC_SPARKLES,
    IC_CHECK, IC_X, 0x2022, IC_CHEVRON_RIGHT, IC_CHEVRON_LEFT, IC_CHEVRON_UP, IC_CHEVRON_DOWN, IC_LIGHTBULB, IC_MAP_PIN, IC_CLOCK,
    IC_SQUARE, IC_CHECK, IC_CAMERA, IC_MUSIC, IC_HOUSE, IC_BELL };

static const Glyph *glyph(const Schrift *s, uint32_t cp) {
    int lo = 0, hi = s->n - 1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (s->g[m].cp == cp) return &s->g[m]; if (s->g[m].cp < cp) lo = m + 1; else hi = m - 1; }
    return NULL;
}
static uint32_t abbilden(uint32_t c) {
    switch (c) {
    case 0x2212: case 0x2010: case 0x2011: return '-';
    case 0x00A0: case 0x202F: case 0x2009: return ' ';
    case 0x2190: return 0x94; case 0x2192: return 0x93;
    }
    return c;
}
/* Breite und Zeichnen eines Zeichens; zeichnen = 0: nur messen */
static int zeichen(int x, int y, uint32_t c, int f, uint16_t farbe, int zeichnen) {
    const Schrift *s = &SCHRIFTEN[f];
    c = abbilden(c);
    if (c >= 0x80 && c <= 0x9F) {
        int32_t m = SYM[c - 0x80];
        if (m >= 0 && m < IC_ANZAHL) {
            int g = s->hoehe >= 26 ? 28 : 20;
            if (zeichnen) gfx_symbol(m, x, y + (s->hoehe - g) / 2, g, farbe);
            return g + 3;
        }
        c = (uint32_t)m & 0x7FFFFFFF;
    }
    if (c >= 0x2100) {
        int e = emoji_index(c);
        if (e >= 0) {
            int g = s->hoehe >= 30 ? 36 : 22;
            if (zeichnen) gfx_emoji(e, x, y + (s->hoehe - g) / 2, g);
            return g + 2;
        }
    }
    const Glyph *gl = glyph(s, c);
    if (!gl) gl = glyph(s, '?');
    if (!gl) return 0;
    if (!zeichnen) return gl->adv;
    int gx = x + gl->ox, w = gl->w, h = s->hoehe;
    if (gx >= lw.clip_x1 || y >= lw.clip_y1 || gx + w <= lw.clip_x0 || y + h <= lw.clip_y0) return gl->adv;
    const uint8_t *d = s->daten + gl->off;
    int i = 0;
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        int sichtbar = py >= lw.clip_y0 && py < lw.clip_y1;
        uint16_t *p = lw.px + py * lw.zeile;
        for (int xx = 0; xx < w; xx++, i++) {
            int a = (i & 1) ? (d[i >> 1] & 15) : (d[i >> 1] >> 4);
            if (!a || !sichtbar) continue;
            int px = gx + xx;
            if (px < lw.clip_x0 || px >= lw.clip_x1) continue;
            p[px] = a >= 15 ? farbe : gfx_mischen(p[px], farbe, a * 17);
        }
    }
    return gl->adv;
}
int text_n(int x, int y, const char *s, int n, int f, uint16_t farbe) {
    int x0 = x, i = 0, c;
    if (!s) return 0;
    while (n-- > 0 && (c = utf8_naechstes(s, &i)) != 0) {
        if (c == '\n') break;
        x += zeichen(x, y, (uint32_t)c, f, farbe, 1);
        if (x > lw.clip_x1 + 40) { while (n-- > 0 && (c = utf8_naechstes(s, &i)) != 0 && c != '\n') x += zeichen(x, y, (uint32_t)c, f, farbe, 0); break; }
    }
    return x - x0;
}
int text(int x, int y, const char *s, int f, uint16_t farbe) { return text_n(x, y, s, 1 << 30, f, farbe); }
int text_breite_n(const char *s, int n, int f) {
    int b = 0, i = 0, c;
    if (!s) return 0;
    while (n-- > 0 && (c = utf8_naechstes(s, &i)) != 0 && c != '\n') b += zeichen(0, 0, (uint32_t)c, f, 0, 0);
    return b;
}
int text_breite(const char *s, int f) { return text_breite_n(s, 1 << 30, f); }
int text_max(int x, int y, const char *s, int max_b, int f, uint16_t farbe) {
    if (text_breite(s, f) <= max_b) return text(x, y, s, f, farbe);
    int pkt = text_breite("…", f), b = 0, i = 0, n = 0, c;
    while ((c = utf8_naechstes(s, &i)) != 0 && c != '\n') {
        int w = zeichen(0, 0, (uint32_t)c, f, 0, 0);
        if (b + w + pkt > max_b) break;
        b += w; n++;
    }
    b = text_n(x, y, s, n, f, farbe);
    return b + text(x + b, y, "…", f, farbe);
}
int text_mitte(int cx, int y, const char *s, int f, uint16_t farbe) { int b = text_breite(s, f); return text(cx - b / 2, y, s, f, farbe); }
int text_rechts(int rx, int y, const char *s, int f, uint16_t farbe) { int b = text_breite(s, f); return text(rx - b, y, s, f, farbe); }

int text_umbrechen(const char *s, int f, int max_b, const char **starts, int *laengen, int max) {
    int n = 0;
    const char *p = s ? s : "";
    while (*p && n < max) {
        int i = 0, zeichen_n = 0, b = 0, leer_i = -1, leer_n = 0, c;
        while (p[i] && p[i] != '\n') {
            int vor = i;
            c = utf8_naechstes(p, &i);
            int w = zeichen(0, 0, (uint32_t)c, f, 0, 0);
            if (b + w > max_b && zeichen_n > 0) { i = vor; break; }
            if (c == ' ') { leer_i = vor; leer_n = zeichen_n; }
            b += w; zeichen_n++;
        }
        int laenge = i, anzahl = zeichen_n;
        if (p[i] && p[i] != '\n' && leer_i > 0) { laenge = leer_i; anzahl = leer_n; }
        starts[n] = p; laengen[n] = anzahl; n++;
        p += laenge;
        if (*p == '\n' || *p == ' ') p++;
    }
    if (n == 0 && max > 0) { starts[0] = p; laengen[0] = 0; n = 1; }
    return n;
}
