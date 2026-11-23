/* plat.h – alles, was vom Gerät abhängt (N64: plat_n64.c mit libdragon, PC-Test: plat_host.c). */
#ifndef PLAT_H
#define PLAT_H
#include <stdint.h>

#define BILD_B 640
#define BILD_H 480

/* Tasten (Bitmaske), alle Controller-Anschlüsse werden zusammengelegt */
enum {
    T_A = 1 << 0, T_B = 1 << 1, T_Z = 1 << 2, T_START = 1 << 3,
    T_HOCH = 1 << 4, T_RUNTER = 1 << 5, T_LINKS = 1 << 6, T_RECHTS = 1 << 7,
    T_L = 1 << 8, T_R = 1 << 9,
    T_C_HOCH = 1 << 10, T_C_RUNTER = 1 << 11, T_C_LINKS = 1 << 12, T_C_RECHTS = 1 << 13
};

typedef struct {
    uint32_t gedrueckt;   /* in diesem Bild neu gedrückt (inkl. Wiederholung beim Halten) */
    uint32_t gehalten;
    int controller;       /* Anschluss (0..3), von dem die letzte Taste kam */
} Eingabe;

void plat_init(void);
void plat_eingabe(Eingabe *e);
/* Bildpuffer zum Zeichnen holen (16 bit RGBA5551, zeile = Pixel je Zeile) */
uint16_t *plat_bild_start(int *zeile);
void plat_bild_ende(void);
void plat_warten(void);          /* bis zum nächsten Halbbild warten (Strom sparen) */
uint32_t plat_ms(void);            /* Millisekunden seit dem Einschalten */
uint32_t plat_takt(void);          /* freier Zähler (N64: COP0 Count, 46,875 MHz) für Messungen */

/* Batteriegepufferter Speicher (SRAM 32 KB) */
#define SPEICHER_GROESSE 32768   /* die letzten 8 Byte nutzt plat_speicher_da() zum Prüfen */
int plat_speicher_da(void);
void plat_speicher_lesen(void *ziel, uint32_t ab, uint32_t laenge);
void plat_speicher_schreiben(const void *quelle, uint32_t ab, uint32_t laenge);

/* Echtzeituhr (falls vorhanden): 1 und Zeit in Sekunden seit 2020-01-01, sonst 0 */
int plat_uhr(int32_t *sek);

#endif
