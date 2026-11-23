/* plat_host.c – PC-Ersatz für die N64 zum Testen: rendert in einen Puffer, speichert Bilder als PNG,
 * liest Tasten aus einem Skript, SRAM als Datei.
 *
 *   build/heini-host --sram build/sram.bin --skript "warte:150 a shot:01 R shot:02 ..."
 *
 * Wörter im Skript (je Wort ein Bild): a b z start hoch runter links rechts L R ch cr cl cu (C-Tasten),
 * "-" (nichts), warte:N (N Bilder nichts), ms:N (Uhr N ms vorstellen), shot:NAME (build/shots/NAME.png),
 * uhr:JJJJ-MM-TT-HH:MM (Uhr setzen, wie eine Echtzeituhr) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "../plat.h"
#include "../model.h"
#include "../logik.h"
#include "../text.h"

static uint16_t puffer[BILD_B * BILD_H];
static uint32_t ms;
static char *skript[4096]; static int n_skript, pos_skript, warte_rest;
static uint32_t taste_jetzt;
static const char *sram_pfad = "build/sram.bin";
static uint8_t sram[SPEICHER_GROESSE];
static int32_t rtc = -1;
extern int32_t uhr_aus_datum(int j, int mo, int d, int h, int mi, int s);
extern zeit_t uhr_basis; extern uint32_t uhr_basis_ms;
extern void uhr_setzen(zeit_t z);

void host_argumente(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sram") && i + 1 < argc) sram_pfad = argv[++i];
        else if (!strcmp(argv[i], "--uhr") && i + 1 < argc) { int j, mo, d, h, mi; if (sscanf(argv[++i], "%d-%d-%d-%d:%d", &j, &mo, &d, &h, &mi) == 5) rtc = uhr_aus_datum(j, mo, d, h, mi, 0); }
        else if (!strcmp(argv[i], "--skript") && i + 1 < argc) {
            char *s = strdup(argv[++i]);
            for (char *w = strtok(s, " \n\t"); w && n_skript < 4096; w = strtok(NULL, " \n\t")) skript[n_skript++] = w;
        }
    }
}

void plat_init(void) {
    FILE *f = fopen(sram_pfad, "rb");
    if (f) { fread(sram, 1, sizeof sram, f); fclose(f); }
}
uint32_t plat_ms(void) { return ms; }
uint32_t plat_takt(void) { return 0; }
uint16_t *plat_bild_start(int *zeile) { *zeile = BILD_B; return puffer; }
void plat_bild_ende(void) {}
void plat_warten(void) {}

static void png_speichern(const char *name) {
    char pfad[256];
    snprintf(pfad, sizeof pfad, "build/shots/%s.png", name);
    FILE *f = fopen(pfad, "wb");
    if (!f) { fprintf(stderr, "kann %s nicht schreiben\n", pfad); return; }
    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(p);
    png_init_io(p, f);
    png_set_IHDR(p, info, BILD_B, BILD_H, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(p, info);
    static uint8_t zeile[BILD_B * 3];
    for (int y = 0; y < BILD_H; y++) {
        for (int x = 0; x < BILD_B; x++) {
            uint16_t c = puffer[y * BILD_B + x];
            int r = c >> 11, g = (c >> 6) & 31, b = (c >> 1) & 31;
            zeile[x * 3] = (uint8_t)(r << 3 | r >> 2); zeile[x * 3 + 1] = (uint8_t)(g << 3 | g >> 2); zeile[x * 3 + 2] = (uint8_t)(b << 3 | b >> 2);
        }
        png_write_row(p, zeile);
    }
    png_write_end(p, NULL);
    png_destroy_write_struct(&p, &info);
    fclose(f);
    printf("Bild: %s\n", pfad);
}

/* Liefert 0, wenn das Skript zu Ende ist */
int host_weiter(void) {
    ms += 33;
    taste_jetzt = 0;
    if (warte_rest > 0) { warte_rest--; return 1; }
    while (pos_skript < n_skript) {
        char *w = skript[pos_skript++];
        if (!strncmp(w, "shot:", 5)) { png_speichern(w + 5); continue; }
        if (!strncmp(w, "ms:", 3)) { ms += (uint32_t)atoi(w + 3); continue; }
        if (!strncmp(w, "uhr:", 4)) { int j, mo, d, h, mi; if (sscanf(w + 4, "%d-%d-%d-%d:%d", &j, &mo, &d, &h, &mi) == 5) uhr_setzen(uhr_aus_datum(j, mo, d, h, mi, 0)); continue; }
        if (!strncmp(w, "warte:", 6)) { warte_rest = atoi(w + 6) - 1; return 1; }
        static const struct { const char *n; uint32_t t; } K[] = {
            {"a", T_A}, {"b", T_B}, {"z", T_Z}, {"start", T_START}, {"hoch", T_HOCH}, {"runter", T_RUNTER}, {"links", T_LINKS},
            {"rechts", T_RECHTS}, {"L", T_L}, {"R", T_R}, {"ch", T_C_HOCH}, {"cr", T_C_RECHTS}, {"cl", T_C_LINKS}, {"cu", T_C_RUNTER}, {"-", 0} };
        for (unsigned i = 0; i < sizeof K / sizeof K[0]; i++) if (!strcmp(w, K[i].n)) taste_jetzt = K[i].t;
        return 1;
    }
    return 0;
}
void plat_eingabe(Eingabe *e) { e->gedrueckt = taste_jetzt; e->gehalten = taste_jetzt; e->controller = 0; }

int plat_uhr(int32_t *sek) { if (rtc < 0) return 0; *sek = rtc; return 1; }

int plat_speicher_da(void) { return 1; }
void plat_speicher_lesen(void *ziel, uint32_t ab, uint32_t laenge) { memcpy(ziel, sram + ab, laenge); }
void plat_speicher_schreiben(const void *quelle, uint32_t ab, uint32_t laenge) {
    memcpy(sram + ab, quelle, laenge);
    FILE *f = fopen(sram_pfad, "wb");
    if (f) { fwrite(sram, 1, sizeof sram, f); fclose(f); }
}

/* ---------- Selbsttest gegen die Werte aus logik.js ---------- */
int host_selbsttest(const char *pfad) {
    FILE *f = fopen(pfad, "r");
    if (!f) { perror(pfad); return 2; }
    char z[2048];
    int fehler = 0, geprueft = 0;
    static int32_t stand[64][MAX_PROFILE]; static int32_t summe[64]; static int hat[64];
    while (fgets(z, sizeof z, f)) {
        int t, a, b; long c;
        if (sscanf(z, "T %d %ld", &t, &c) == 2) {
            uint32_t bet;
            summe[t] = kassen_stand(t, stand[t], &bet); hat[t] = 1;
            geprueft++;
            if (summe[t] != c) { printf("FEHLER Summe Treffen %d: %d statt %ld\n", t, summe[t], c); fehler++; }
        } else if (sscanf(z, "S %d %d %ld", &t, &a, &c) == 3) {
            geprueft++;
            if (stand[t][a] != c) { printf("FEHLER Stand Treffen %d Profil %d (%s): %d statt %ld\n", t, a, pname(a), stand[t][a], c); fehler++; }
        } else if (sscanf(z, "P %d %d %d %ld", &t, &a, &b, &c) == 4) {
            DirektPaar d[64]; int n = direkt_paare(t, d, 64), ok = 0;
            for (int i = 0; i < n; i++) if (d[i].von == a && d[i].an == b && d[i].betrag == c) ok = 1;
            geprueft++;
            if (!ok) { printf("FEHLER Direktzahlung Treffen %d: %s an %s %ld fehlt\n", t, pname(a), pname(b), c); fehler++; }
        } else if (z[0] == 'W') {
            int tag; char hex[1024];
            if (sscanf(z, "W %d %1000s", &tag, hex) == 2) {
                char soll[512]; int n = 0;
                for (int i = 0; hex[i] && hex[i + 1]; i += 2) { unsigned v; sscanf(hex + i, "%2x", &v); soll[n++] = (char)v; }
                soll[n] = 0;
                const char *ist = weisheit_des_tages((tag_t)tag);
                geprueft++;
                if (strcmp(soll, ist)) { printf("FEHLER Weisheit: „%s“ statt „%s“\n", ist, soll); fehler++; }
            }
        }
    }
    fclose(f);
    printf("Selbsttest: %d Prüfungen, %d Fehler\n", geprueft, fehler);
    return fehler ? 1 : 0;
}
