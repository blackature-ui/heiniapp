/* plat_n64.c – Hardware der Nintendo 64 direkt angesprochen (ohne libdragon-Bibliothek):
 * VI (Bild 640x480, Zeilensprung, doppelt gepuffert), SI/PIF (Controller, Echtzeituhr),
 * PI (SRAM 32 KB), COP0-Zähler (Zeit). */
#include <stdint.h>
#include <string.h>
#include "../plat.h"

#define REG(a) (*(volatile uint32_t *)(a))
#define UNCACHED(p) ((void *)(((uintptr_t)(p) & 0x1FFFFFFF) | 0xA0000000))
#define PHYS(p) ((uint32_t)(uintptr_t)(p) & 0x1FFFFFFF)

/* VI */
#define VI_CTRL     REG(0xA4400000)
#define VI_ORIGIN   REG(0xA4400004)
#define VI_WIDTH    REG(0xA4400008)
#define VI_V_INTR   REG(0xA440000C)
#define VI_CURRENT  REG(0xA4400010)
#define VI_X_SCALE  REG(0xA4400030)
#define VI_Y_SCALE  REG(0xA4400034)
/* MI */
#define MI_MASK     REG(0xA430000C)
/* PI */
#define PI_DRAM     REG(0xA4600000)
#define PI_CART     REG(0xA4600004)
#define PI_RD_LEN   REG(0xA4600008)
#define PI_WR_LEN   REG(0xA460000C)
#define PI_STATUS   REG(0xA4600010)
#define PI_DOM2_LAT REG(0xA4600024)
#define PI_DOM2_PWD REG(0xA4600028)
#define PI_DOM2_PGS REG(0xA460002C)
#define PI_DOM2_RLS REG(0xA4600030)
/* SI */
#define SI_DRAM     REG(0xA4800000)
#define SI_RD64     REG(0xA4800004)
#define SI_WR64     REG(0xA4800010)
#define SI_STATUS   REG(0xA4800018)
#define PIF_RAM     0x1FC007C0

extern char vi_handler[], vi_handler_ende[];
extern void heap_init(char *ende);
volatile uint32_t vi_anfang[2];
volatile uint32_t vi_zaehler;

static const uint32_t VI_PAL_I[14] = {
    0, 0, 0, 2, 0, 0x0404233a, 0x00000270, 0x00150c69, 0x0c6f0c6e, 0x00800300, 0x005d0237, 0x0009026b, 0, 0 };
static const uint32_t VI_NTSC_I[14] = {
    0, 0, 0, 2, 0, 0x03e52239, 0x0000020c, 0x00000c15, 0x0c150c15, 0x006c02ec, 0x002301fd, 0x000e0204, 0, 0 };
static const uint32_t VI_MPAL_I[14] = {
    0, 0, 0, 2, 0, 0x04651e39, 0x0000020c, 0x00000c10, 0x0c1c0c1c, 0x006c02ec, 0x002301fd, 0x000b0202, 0, 0 };

static uint16_t bildpuffer[2][BILD_B * BILD_H] __attribute__((aligned(64)));
static int zeigt;            /* welcher Puffer gerade zu sehen ist */
static uint32_t ms_basis, zaehler_alt, ms_rest;

static inline uint32_t cop0_count(void) { uint32_t c; __asm__ volatile("mfc0 %0, $9" : "=r"(c)); return c; }
static inline void cop0_status(uint32_t s) { __asm__ volatile("mtc0 %0, $12\n nop\n nop" :: "r"(s)); }

static void dcache_alles_zurueck(void) {
    for (uintptr_t a = 0x80000000; a < 0x80002000; a += 16) __asm__ volatile(".set push\n.set mips3\ncache 0x01, 0(%0)\n.set pop" :: "r"(a));
}
static void icache_alles_leeren(void) {
    for (uintptr_t a = 0x80000000; a < 0x80004000; a += 32) __asm__ volatile(".set push\n.set mips3\ncache 0x00, 0(%0)\n.set pop" :: "r"(a));
}

static void puffer_zeigen(int i) {
    uint32_t a = PHYS(bildpuffer[i]);
    vi_anfang[1] = a;
    vi_anfang[0] = a + BILD_B * 2;   /* gerades Halbbild: eine Zeile tiefer */
}

int plat_tv_typ;   /* 0 PAL, 1 NTSC, 2 MPAL */

void plat_init(void) {
    uint32_t speicher = REG(0xA4000000);           /* Boot-Flags von IPL3 im DMEM */
    plat_tv_typ = (int)((REG(0xA4000008) >> 16) & 0xff);
    if (speicher < (4u << 20) || speicher > (8u << 20)) speicher = 4u << 20;
    heap_init((char *)(uintptr_t)(0x80000000u + speicher - 128 * 1024));   /* 64 KB IPL3 + Stapel */

    /* Ausnahme-Vektor installieren */
    uint32_t n = (uint32_t)(vi_handler_ende - vi_handler);
    memcpy(UNCACHED(0x80000180), vi_handler, n);
    memcpy(UNCACHED(0x80000000), vi_handler, n);
    memcpy(UNCACHED(0x80000080), vi_handler, n);
    icache_alles_leeren();

    /* Bild */
    memset(bildpuffer, 0, sizeof bildpuffer);
    dcache_alles_zurueck();
    const uint32_t *v = plat_tv_typ == 1 ? VI_NTSC_I : plat_tv_typ == 2 ? VI_MPAL_I : VI_PAL_I;
    for (int i = 0; i < 14; i++) if (i != 3 && i != 4) REG(0xA4400000 + i * 4) = v[i];
    zeigt = 0;
    puffer_zeigen(0);
    VI_ORIGIN = vi_anfang[1];
    VI_WIDTH = BILD_B;
    VI_X_SCALE = (1024 * BILD_B + 320) / 640;
    VI_Y_SCALE = (1024 * BILD_H + 120) / 240;
    VI_V_INTR = 2;
    VI_CTRL = 0x3000 | 0x0200 | 0x0040 | 0x0002;   /* Pixel-Vorschub, Resampling, Zeilensprung, 16 bit */

    /* SRAM-Zeiten (Domäne 2) */
    PI_DOM2_LAT = 0x05; PI_DOM2_PWD = 0x0C; PI_DOM2_PGS = 0x0D; PI_DOM2_RLS = 0x02;

    /* Interrupts: nur VI */
    MI_MASK = 0x0515 | 0x0080;
    VI_CURRENT = 0;
    cop0_status(0x30000401);      /* CU0, CU1, IM2 (RCP), IE */

    zaehler_alt = cop0_count();
    ms_basis = 0; ms_rest = 0;
}

uint32_t plat_takt(void) { return cop0_count(); }
uint32_t plat_ms(void) {
    uint32_t c = cop0_count();
    uint32_t d = c - zaehler_alt;           /* 46,875 MHz; Überlauf alle ~91 s, wir fragen öfter */
    zaehler_alt = c;
    ms_rest += d;
    uint32_t ganze = ms_rest / 46875;
    ms_rest -= ganze * 46875;
    ms_basis += ganze;
    return ms_basis;
}

/* ---------- Bild ---------- */
uint16_t *plat_bild_start(int *zeile) {
    *zeile = BILD_B;
    return bildpuffer[zeigt ^ 1];
}
void plat_bild_ende(void) {
    dcache_alles_zurueck();
    zeigt ^= 1;
    puffer_zeigen(zeigt);
    /* bis zum nächsten Halbbild warten, damit der alte Puffer nicht mehr gelesen wird */
    uint32_t z = vi_zaehler;
    uint32_t start = cop0_count();
    while (vi_zaehler == z && cop0_count() - start < 46875u * 40u) {}
    plat_ms();
}
void plat_warten(void) {
    uint32_t z = vi_zaehler;
    uint32_t start = cop0_count();
    while (vi_zaehler == z && cop0_count() - start < 46875u * 40u) {}
    plat_ms();
}

/* ---------- SI / PIF ---------- */
static uint64_t pif_puffer[8] __attribute__((aligned(16)));

static void si_warten(void) { while (SI_STATUS & 3) {} }
static void pif_tausch(uint8_t *block) {
    volatile uint8_t *u = UNCACHED(pif_puffer);
    for (int i = 0; i < 64; i++) u[i] = block[i];
    si_warten();
    SI_DRAM = PHYS(pif_puffer);
    SI_WR64 = PIF_RAM;
    si_warten();
    SI_DRAM = PHYS(pif_puffer);
    SI_RD64 = PIF_RAM;
    si_warten();
    for (int i = 0; i < 64; i++) block[i] = u[i];
}

static uint32_t tasten_roh(int *controller) {
    uint8_t b[64];
    memset(b, 0, sizeof b);
    for (int k = 0; k < 4; k++) {
        uint8_t *c = b + k * 8;
        c[0] = 0xff; c[1] = 0x01; c[2] = 0x04; c[3] = 0x01; c[4] = c[5] = c[6] = c[7] = 0xff;
    }
    b[32] = 0xfe;
    b[63] = 0x01;
    pif_tausch(b);
    uint32_t alle = 0;
    for (int k = 0; k < 4; k++) {
        uint8_t *c = b + k * 8;
        if (c[2] & 0xc0) continue;         /* kein Controller */
        uint16_t t = (uint16_t)((c[4] << 8) | c[5]);
        int8_t x = (int8_t)c[6], y = (int8_t)c[7];
        uint32_t m = 0;
        if (t & 0x8000) m |= T_A;
        if (t & 0x4000) m |= T_B;
        if (t & 0x2000) m |= T_Z;
        if (t & 0x1000) m |= T_START;
        if ((t & 0x0800) || y > 40) m |= T_HOCH;
        if ((t & 0x0400) || y < -40) m |= T_RUNTER;
        if ((t & 0x0200) || x < -40) m |= T_LINKS;
        if ((t & 0x0100) || x > 40) m |= T_RECHTS;
        if (t & 0x0020) m |= T_L;
        if (t & 0x0010) m |= T_R;
        if (t & 0x0008) m |= T_C_HOCH;
        if (t & 0x0004) m |= T_C_RUNTER;
        if (t & 0x0002) m |= T_C_LINKS;
        if (t & 0x0001) m |= T_C_RECHTS;
        if (m && controller) *controller = k;
        alle |= m;
    }
    return alle;
}

static uint32_t vorher, wiederhol_ab[32];
void plat_eingabe(Eingabe *e) {
    uint32_t jetzt = plat_ms();
    uint32_t t = tasten_roh(&e->controller);
    uint32_t neu = t & ~vorher;
    uint32_t wdh = 0;
    for (int i = 0; i < 14; i++) {
        uint32_t bit = 1u << i;
        if (neu & bit) wiederhol_ab[i] = jetzt + 400;
        else if ((t & bit) && jetzt >= wiederhol_ab[i] &&
                 (bit & (T_HOCH | T_RUNTER | T_LINKS | T_RECHTS | T_C_HOCH | T_C_RUNTER | T_L | T_R))) {
            wdh |= bit; wiederhol_ab[i] = jetzt + 90;
        }
    }
    e->gedrueckt = neu | wdh;
    e->gehalten = t;
    vorher = t;
}

/* ---------- Echtzeituhr (Joybus, Kanal 4) ---------- */
static int bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
extern int32_t uhr_aus_datum(int j, int mo, int d, int h, int mi, int s);

int plat_uhr(int32_t *sek) {
    uint8_t b[64];
    memset(b, 0, sizeof b);
    /* Status: Kanäle 0..3 überspringen */
    b[4] = 0x01; b[5] = 0x03; b[6] = 0x06; b[7] = b[8] = b[9] = 0xff; b[10] = 0xfe; b[63] = 0x01;
    pif_tausch(b);
    if ((b[5] & 0xc0) || b[7] != 0x00 || b[8] != 0x10) return 0;
    memset(b, 0, sizeof b);
    b[4] = 0x02; b[5] = 0x09; b[6] = 0x07; b[7] = 0x02;
    for (int i = 8; i < 17; i++) b[i] = 0xff;
    b[17] = 0xfe; b[63] = 0x01;
    pif_tausch(b);
    if (b[5] & 0xc0) return 0;
    uint8_t *d = b + 8;
    int s = bcd(d[0] & 0x7f), mi = bcd(d[1] & 0x7f), h = bcd(d[2] & 0x3f);
    int tag = bcd(d[3] & 0x3f), mo = bcd(d[5] & 0x1f), j = 1900 + bcd(d[6]) + (d[7] ? 100 : 0);
    if (mo < 1 || mo > 12 || tag < 1 || tag > 31 || h > 23 || mi > 59 || j < 2020) return 0;
    *sek = uhr_aus_datum(j, mo, tag, h, mi, s);
    return 1;
}

/* ---------- SRAM über PI ---------- */
static uint8_t pi_puffer[512] __attribute__((aligned(16)));
static void pi_warten(void) { while (PI_STATUS & 3) {} }

void plat_speicher_lesen(void *ziel, uint32_t ab, uint32_t laenge) {
    uint8_t *z = ziel;
    volatile uint8_t *u = UNCACHED(pi_puffer);
    while (laenge) {
        uint32_t n = laenge > sizeof pi_puffer ? sizeof pi_puffer : laenge;
        uint32_t m = (n + 1) & ~1u;
        pi_warten();
        PI_DRAM = PHYS(pi_puffer);
        PI_CART = 0x08000000 + ab;
        PI_WR_LEN = m - 1;
        pi_warten();
        for (uint32_t i = 0; i < n; i++) z[i] = u[i];
        z += n; ab += n; laenge -= n;
    }
}
void plat_speicher_schreiben(const void *quelle, uint32_t ab, uint32_t laenge) {
    const uint8_t *q = quelle;
    volatile uint8_t *u = UNCACHED(pi_puffer);
    while (laenge) {
        uint32_t n = laenge > sizeof pi_puffer ? sizeof pi_puffer : laenge;
        uint32_t m = (n + 1) & ~1u;
        for (uint32_t i = 0; i < n; i++) u[i] = q[i];
        if (m != n) u[n] = 0;
        pi_warten();
        PI_DRAM = PHYS(pi_puffer);
        PI_CART = 0x08000000 + ab;
        PI_RD_LEN = m - 1;
        pi_warten();
        q += n; ab += n; laenge -= n;
    }
}
int plat_speicher_da(void) {
    uint8_t alt[8], test[8], zurueck[8];
    plat_speicher_lesen(alt, SPEICHER_GROESSE - 8, 8);
    for (int i = 0; i < 8; i++) test[i] = (uint8_t)(alt[i] ^ (0x5a + i));
    plat_speicher_schreiben(test, SPEICHER_GROESSE - 8, 8);
    plat_speicher_lesen(zurueck, SPEICHER_GROESSE - 8, 8);
    plat_speicher_schreiben(alt, SPEICHER_GROESSE - 8, 8);
    return memcmp(test, zurueck, 8) == 0;
}
