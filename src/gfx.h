/* gfx.h – Zeichnen in den 640x480-Bildpuffer (RGBA5551) */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 3) << 6) | (((b) >> 3) << 1) | 1))

/* Grundfarben (dunkles Material-You-Schema wie in der App) */
#define C_HG       RGB(0x12, 0x11, 0x16)
#define C_FLAECHE  RGB(0x1f, 0x1d, 0x24)
#define C_FLAECHE2 RGB(0x2a, 0x28, 0x31)
#define C_LINIE    RGB(0x3a, 0x37, 0x42)
#define C_TEXT     RGB(0xec, 0xe6, 0xf0)
#define C_TEXT2    RGB(0xa9, 0xa3, 0xb3)
#define C_WEISS    RGB(0xff, 0xff, 0xff)
#define C_SCHWARZ  RGB(0x10, 0x10, 0x10)
#define C_GRUEN    RGB(0x4c, 0xc3, 0x6a)
#define C_ROT      RGB(0xe5, 0x48, 0x4d)
#define C_GELB     RGB(0xf2, 0xc1, 0x4e)
#define C_BLAU     RGB(0x5b, 0x9b, 0xf5)

typedef struct { uint16_t *px; int zeile; int clip_x0, clip_y0, clip_x1, clip_y1; } Leinwand;
extern Leinwand lw;

void gfx_start(uint16_t *px, int zeile);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_clip_aus(void);
void gfx_fuellen(uint16_t farbe);
void gfx_rechteck(int x, int y, int b, int h, uint16_t farbe);
void gfx_rahmen(int x, int y, int b, int h, uint16_t farbe);
void gfx_runde_box(int x, int y, int b, int h, int r, uint16_t farbe);
void gfx_kreis(int cx, int cy, int r, uint16_t farbe);
void gfx_linie_h(int x, int y, int b, uint16_t farbe);
void gfx_linie_v(int x, int y, int h, uint16_t farbe);
void gfx_bild(int x, int y, int b, int h, const uint16_t *px);           /* mit 1-bit-Alpha */
void gfx_bild_rund(int x, int y, int b, const uint16_t *px, uint16_t rand); /* rund ausgeschnitten */
uint16_t gfx_mischen(uint16_t a, uint16_t b, int anteil_b_256);

/* Text (8x16-Schrift). fett: 0/1, gross: Vergrößerung 1..4. Ergebnis: Breite in Pixeln */
int gfx_text(int x, int y, const char *s, uint16_t farbe, int fett, int gross);
int gfx_text_n(int x, int y, const char *s, int n, uint16_t farbe, int fett, int gross);
int gfx_textbreite(const char *s, int gross);
/* abgeschnitten mit „...“ auf max_b Pixel */
int gfx_text_max(int x, int y, const char *s, int max_b, uint16_t farbe, int fett);
int gfx_text_mitte(int cx, int y, const char *s, uint16_t farbe, int fett, int gross);

#endif
