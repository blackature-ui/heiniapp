/* gfx.h – Zeichnen in den 640x480-Bildpuffer (RGBA5551): Flächen, Roboto Flex mit Kantenglättung,
 * Lucide-Symbole, Emojis, Bilder mit Alpha. Farben kommen aus dem Material-You-Schema der Profilfarbe (TH). */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "assets.h"

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 3) << 6) | (((b) >> 3) << 1) | 1))
#define C_WEISS    RGB(0xff, 0xff, 0xff)
#define C_SCHWARZ  RGB(0x00, 0x00, 0x00)

extern const Thema *TH;                 /* aktuelles Farbschema */
void thema_setzen(int farbe, int dunkel);

typedef struct { uint16_t *px; int zeile; int clip_x0, clip_y0, clip_x1, clip_y1; } Leinwand;
extern Leinwand lw;

void gfx_start(uint16_t *px, int zeile);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_clip_aus(void);
void gfx_fuellen(uint16_t farbe);
void gfx_rechteck(int x, int y, int b, int h, uint16_t farbe);
void gfx_rahmen(int x, int y, int b, int h, uint16_t farbe);
void gfx_runde_box(int x, int y, int b, int h, int r, uint16_t farbe);
void gfx_runder_rahmen(int x, int y, int b, int h, int r, int dicke, uint16_t farbe);
void gfx_verlauf_box(int x, int y, int b, int h, int r, uint16_t oben, uint16_t unten);
void gfx_kreis(int cx, int cy, int r, uint16_t farbe);
void gfx_linie_h(int x, int y, int b, uint16_t farbe);
void gfx_linie_v(int x, int y, int h, uint16_t farbe);
uint16_t gfx_mischen(uint16_t a, uint16_t b, int anteil_b_256);
void gfx_bild4444(int x, int y, int b, int h, const uint16_t *px);
void gfx_abdunkeln(int x, int y, int b, int h, int anteil_256);

/* Symbole (Lucide) in 20 oder 28 px */
void gfx_symbol(int id, int x, int y, int groesse, uint16_t farbe);
/* Emojis in 22 oder 36 px */
int emoji_index(uint32_t cp);
void gfx_emoji(int i, int x, int y, int groesse);

/* Text: y = Oberkante der Zeile. Ergebnis: Breite in Pixeln */
int text(int x, int y, const char *s, int f, uint16_t farbe);
int text_n(int x, int y, const char *s, int n_zeichen, int f, uint16_t farbe);
int text_breite(const char *s, int f);
int text_breite_n(const char *s, int n_zeichen, int f);
int text_max(int x, int y, const char *s, int max_b, int f, uint16_t farbe);   /* mit … gekürzt */
int text_mitte(int cx, int y, const char *s, int f, uint16_t farbe);
int text_rechts(int rx, int y, const char *s, int f, uint16_t farbe);
static inline int text_hoehe(int f) { return SCHRIFTEN[f].hoehe; }
/* Umbruch nach Pixelbreite; laengen = Zeichen */
int text_umbrechen(const char *s, int f, int max_b, const char **starts, int *laengen, int max);

#endif
