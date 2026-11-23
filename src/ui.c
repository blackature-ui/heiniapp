/* ui.c – Sofort-Modus-Oberfläche für Steuerkreuz und Fernseher */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"
#include "gfx.h"
#include "text.h"
#include "assets.h"
#include "logik.h"
#include "aktion.h"
#define AKTION_PUFFER (AKTION_TEXT_MAX + 1)

/* ---------- Zustand ---------- */
typedef struct { int16_t x, y, b, h; } Elem;
#define MAX_ELEM 400
static Elem elem[MAX_ELEM], alt[MAX_ELEM];
static int n_elem, n_alt, fokus, nochmal;
static uint32_t taste_roh, taste;
static int roll, ber_y0 = KOPF_H, ber_y1 = FUSS_Y, cur_y, inhalt_h;
static int x0 = RAND_X, breite = INHALT_B;
static int reihe_x, reihe_y, reihe_h;     /* Knöpfe nebeneinander */
static int letzt_fokus;
/* Karten: Höhe aus dem letzten Bild */
#define MAX_KARTEN 64
static int karten_h[MAX_KARTEN], karten_h_neu[MAX_KARTEN], n_karten, karte_start, karte_offen;
static const char *karte_titel; static uint16_t karte_band;

static int dlg_typ;   /* 0 keiner */
static uint32_t meldung_bis; static char meldung_text[200];

uint32_t ui_taste(void) { return taste; }
int ui_dialog_offen(void) { return dlg_typ != 0; }

void ui_seite_neu(void) { fokus = 0; roll = 0; n_alt = 0; memset(karten_h, 0, sizeof karten_h); }

static int sy(int y) { return ber_y0 + y - roll; }   /* Inhalt → Bildschirm */

static void dialog_eingabe(uint32_t t);

void ui_bild_start(const Eingabe *e) {
    taste_roh = e ? e->gedrueckt : 0;
    taste = 0;
    if (dlg_typ) dialog_eingabe(taste_roh);
    else {
        taste = taste_roh;
        if (n_alt > 0) {
            if (fokus >= n_alt) fokus = n_alt - 1;
            const Elem *f = &alt[fokus];
            int cx = f->x + f->b / 2;
            if (taste & (T_HOCH | T_RUNTER)) {
                int runter = (taste & T_RUNTER) != 0, best = -1, bd = 1 << 30;
                for (int i = 0; i < n_alt; i++) {
                    int dy = alt[i].y - f->y;
                    if (runter ? dy <= 0 : dy >= 0) continue;
                    int ady = abs(dy), dx = abs(alt[i].x + alt[i].b / 2 - cx);
                    int d = ady * 4 + dx;
                    if (d < bd) { bd = d; best = i; }
                }
                if (best >= 0) fokus = best;
                else if (!runter) roll = 0;                    /* ganz oben: Rollen zurück */
                else if (inhalt_h > ber_y1 - ber_y0) roll = inhalt_h - (ber_y1 - ber_y0);
            }
            if (taste & T_LINKS) { if (fokus > 0 && alt[fokus - 1].y == f->y) fokus--; }
            if (taste & T_RECHTS) { if (fokus + 1 < n_alt && alt[fokus + 1].y == f->y) fokus++; }
        } else {
            int seite = ber_y1 - ber_y0;
            if (taste & T_RUNTER) roll += 48;
            if (taste & T_HOCH) roll -= 48;
            if (roll > inhalt_h - seite) roll = inhalt_h - seite;
            if (roll < 0) roll = 0;
        }
        if (taste & T_C_RUNTER) roll += 120;
        if (taste & T_C_HOCH) roll -= 120;
        if (roll < 0) roll = 0;
    }
    n_elem = 0; nochmal = 0; n_karten = 0; karte_offen = 0;
}

int ui_bild_ende(void) {
    memcpy(alt, elem, sizeof(Elem) * n_elem);
    n_alt = n_elem;
    if (fokus >= n_alt) fokus = n_alt ? n_alt - 1 : 0;
    for (int i = 0; i < n_karten; i++) if (karten_h[i] != karten_h_neu[i]) { karten_h[i] = karten_h_neu[i]; nochmal = 1; }
    /* Fokus ins Bild rollen */
    if (n_alt && !(taste & (T_C_HOCH | T_C_RUNTER))) {
        const Elem *f = &alt[fokus];
        int seite = ber_y1 - ber_y0;
        int neu = roll;
        if (f->y - 12 < neu) neu = f->y - 12;
        if (f->y + f->h + 12 > neu + seite) neu = f->y + f->h + 12 - seite;
        if (fokus == 0 && f->y < seite - 40) neu = 0;
        if (neu < 0) neu = 0;
        if (neu != roll) { roll = neu; nochmal = 1; }
    }
    int r = nochmal;
    nochmal = 0;
    return r;
}

void ui_bereich(int y0, int y1) {
    ber_y0 = y0; ber_y1 = y1; cur_y = 8; x0 = RAND_X; breite = INHALT_B;
    gfx_clip(0, y0, BILD_B, y1);
}
void ui_bereich_ende(void) {
    inhalt_h = cur_y + 8;
    gfx_clip_aus();
    /* Rollhinweise */
    if (roll > 0) gfx_text_mitte(BILD_B / 2, ber_y0 - 2, S_HOCH, C_TEXT2, 0, 1);
    if (inhalt_h - roll > ber_y1 - ber_y0) gfx_text_mitte(BILD_B / 2, ber_y1 - 14, S_RUNTER, C_TEXT2, 0, 1);
}
int ui_y(void) { return sy(cur_y); }
void ui_abstand(int px) { cur_y += px; }

static int element(int x, int y, int b, int h) {
    int i = n_elem;
    if (n_elem < MAX_ELEM) elem[n_elem++] = (Elem){ (int16_t)x, (int16_t)y, (int16_t)b, (int16_t)h };
    letzt_fokus = (i == fokus) && !dlg_typ;
    return letzt_fokus;
}
int ui_fokus_hier(void) { return n_elem == fokus && !dlg_typ; }
int ui_fokussiert_letztes(void) { return letzt_fokus; }

/* ---------- Inhalte ---------- */
void ui_ueberschrift(const char *s) {
    cur_y += 6;
    gfx_text(x0, sy(cur_y), s, C_TEXT, 1, 2);
    cur_y += 40;
}

void ui_karte(const char *titel, uint16_t band) {
    int h = n_karten < MAX_KARTEN ? karten_h[n_karten] : 0;
    if (h <= 0) h = 60;
    int y = sy(cur_y);
    gfx_runde_box(x0, y, breite, h, 12, C_FLAECHE);
    karte_titel = titel; karte_band = band;
    karte_start = cur_y;
    if (titel && titel[0]) {
        gfx_runde_box(x0, y, breite, 30, 12, band);
        gfx_rechteck(x0, y + 18, breite, 12, band);
        int hell = (band >> 11) * 3 + ((band >> 6) & 31) * 6 + ((band >> 1) & 31) > 31 * 6;
        gfx_text_max(x0 + 14, y + 7, titel, breite - 28, hell ? C_SCHWARZ : C_WEISS, 1);
        cur_y += 38;
    } else cur_y += 12;
    karte_offen = 1;
    x0 += 14; breite -= 28;
}
void ui_karte_ende(void) {
    if (!karte_offen) return;
    x0 -= 14; breite += 28;
    cur_y += 10;
    if (n_karten < MAX_KARTEN) karten_h_neu[n_karten++] = cur_y - karte_start;
    karte_offen = 0;
    cur_y += 12;
}

void ui_text(const char *s, uint16_t farbe) {
    const char *st[64]; int ln[64];
    int n = umbrechen(s, breite / 8, st, ln, 64);
    for (int i = 0; i < n; i++) {
        gfx_text_n(x0, sy(cur_y), st[i], ln[i], farbe, 0, 1);
        cur_y += 18;
    }
    cur_y += 2;
}
void ui_text_gross(const char *s, uint16_t farbe, int gross) {
    const char *st[16]; int ln[16];
    int n = umbrechen(s, breite / (8 * gross), st, ln, 16);
    for (int i = 0; i < n; i++) { gfx_text_n(x0, sy(cur_y), st[i], ln[i], farbe, 1, gross); cur_y += 16 * gross + 4; }
}
void ui_zeile(const char *links, const char *rechts, uint16_t farbe) {
    int rb = rechts ? gfx_textbreite(rechts, 1) : 0;
    gfx_text_max(x0, sy(cur_y), links, breite - rb - 12, farbe, 0);
    if (rechts) gfx_text(x0 + breite - rb, sy(cur_y), rechts, farbe, 1, 1);
    cur_y += 20;
}

static int knopf_zeichnen(int x, int y, const char *s, uint16_t farbe, uint16_t schrift) {
    int b = gfx_textbreite(s, 1) + 32;
    if (b > breite) b = breite;
    int f = element(x, y, b, 32);
    if (f) {
        gfx_runde_box(x - 3, sy(y) - 3, b + 6, 38, 19, C_WEISS);
        gfx_runde_box(x, sy(y), b, 32, 16, farbe == C_FLAECHE2 ? gfx_mischen(farbe, C_WEISS, 60) : gfx_mischen(farbe, C_WEISS, 50));
    } else gfx_runde_box(x, sy(y), b, 32, 16, farbe);
    gfx_text_max(x + 16, sy(y) + 8, s, b - 32, schrift, 1);
    return b;
}
int ui_knopf_farbe(const char *s, uint16_t farbe, uint16_t schrift) {
    reihe_y = cur_y; reihe_h = 40;
    int b = knopf_zeichnen(x0, cur_y, s, farbe, schrift);
    reihe_x = x0 + b + 10;
    cur_y += 40;
    return letzt_fokus && (taste & T_A);
}
int ui_knopf(const char *s) { return ui_knopf_farbe(s, C_FLAECHE2, C_TEXT); }
int ui_knopf_neben_farbe(const char *s, uint16_t farbe, uint16_t schrift) {
    int b = gfx_textbreite(s, 1) + 32;
    if (reihe_x + b > x0 + breite) return ui_knopf_farbe(s, farbe, schrift);   /* passt nicht mehr: neue Zeile */
    b = knopf_zeichnen(reihe_x, reihe_y, s, farbe, schrift);
    reihe_x += b + 10;
    return letzt_fokus && (taste & T_A);
}
int ui_knopf_neben(const char *s) { return ui_knopf_neben_farbe(s, C_FLAECHE2, C_TEXT); }

static void person_zeile(int p, const char *titel, const char *unter, const char *rechts, int f) {
    int y = sy(cur_y);
    if (f) { gfx_runde_box(x0 - 6, y - 2, breite + 12, 56, 10, C_WEISS); gfx_runde_box(x0 - 3, y + 1, breite + 6, 50, 9, C_FLAECHE2); }
    if (p >= 0) avatar(p, x0 + 2, y + 2, 48);
    else { gfx_kreis(x0 + 26, y + 26, 24, C_FLAECHE2); }
    int tx = x0 + 62, rb = rechts ? gfx_textbreite(rechts, 1) + 8 : 0;
    gfx_text_max(tx, y + (unter && unter[0] ? 8 : 18), titel, breite - 62 - rb, C_TEXT, 1);
    if (unter && unter[0]) gfx_text_max(tx, y + 28, unter, breite - 62 - rb, C_TEXT2, 0);
    if (rechts) gfx_text(x0 + breite - rb + 8, y + 18, rechts, C_TEXT, 1, 1);
    cur_y += 58;
}
int ui_eintrag(int p, const char *titel, const char *unter, const char *rechts) {
    int f = element(x0, cur_y, breite, 52);
    person_zeile(p, titel, unter, rechts, f);
    return f && (taste & T_A);
}
void ui_person(int p, const char *titel, const char *unter, const char *rechts) { person_zeile(p, titel, unter, rechts, 0); }

void ui_balken(int anteil, uint16_t farbe) {
    if (anteil < 0) anteil = 0;
    if (anteil > 1000) anteil = 1000;
    int y = sy(cur_y);
    gfx_runde_box(x0, y, breite, 10, 5, C_FLAECHE2);
    if (anteil > 0) gfx_runde_box(x0, y, breite * anteil / 1000 < 10 ? 10 : breite * anteil / 1000, 10, 5, farbe);
    cur_y += 18;
}
void ui_avatare(const int *pr, int n) {
    int x = x0, y = cur_y;
    for (int i = 0; i < n; i++) {
        if (x + 48 > x0 + breite) { x = x0; y += 70; }
        avatar(pr[i], x, sy(y), 48);
        gfx_text_mitte(x + 24, sy(y) + 52, tmp("%.6s", pname(pr[i])), C_TEXT2, 0, 1);
        x += 62;
    }
    cur_y = y + (n ? 72 : 0);
}

/* ---------- Zeichnen ---------- */
void avatar(int p, int x, int y, int g) {
    int bild = p >= 0 && p < db.profile.n ? db.profile.v[p].bild : 0;
    uint16_t rand = pfarbe(p);
    if (bild >= 1 && bild <= 48) gfx_bild_rund(x, y, g, g == 48 ? avatar48[bild - 1] : avatar24[bild - 1], rand);
    else {
        gfx_kreis(x + g / 2, y + g / 2, g / 2 + 2, rand);
        const char *n = pname(p);
        char k[3] = { n[0], 0, 0 };
        gfx_text_mitte(x + g / 2, y + g / 2 - (g == 48 ? 16 : 8), k, phell(p) ? C_SCHWARZ : C_WEISS, 1, g == 48 ? 2 : 1);
    }
}
/* Ring mit Anteil (0..1000), beginnt oben, im Uhrzeigersinn */
void ui_ring(int cx, int cy, int r, int dicke, int anteil, uint16_t farbe, uint16_t hinten) {
    int r2 = r * r, i2 = (r - dicke) * (r - dicke);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int d = dx * dx + dy * dy;
            if (d > r2 || d < i2) continue;
            /* Winkel als Rautenwinkel 0..4000 ab oben im Uhrzeigersinn */
            int x = dx, y = -dy, w;
            int ax = abs(x), ay = abs(y), s = ax + ay;
            if (!s) continue;
            if (x >= 0 && y > 0) w = 1000 * ax / s;
            else if (x > 0 && y <= 0) w = 1000 + 1000 * ay / s;
            else if (x <= 0 && y < 0) w = 2000 + 1000 * ax / s;
            else w = 3000 + 1000 * ay / s;
            uint16_t c = w < anteil * 4 ? farbe : hinten;
            int px = cx + dx, py = cy + dy;
            if (px >= lw.clip_x0 && px < lw.clip_x1 && py >= lw.clip_y0 && py < lw.clip_y1) lw.px[py * lw.zeile + px] = c;
        }
}

/* ---------- Kopf und Fuß ---------- */
static const char *const REITER[5] = { S_HAUS " Heute", S_UHR " Plan", S_EURO " Kasse", S_BETT " Treffen", S_BIRNE " Ideen" };
void kopf(const char *titel, int mit_reitern, int reiter) {
    uint16_t band = ich >= 0 ? pfarbe(ich) : RGB(0x7b, 0x16, 0x19);
    uint16_t schrift = ich >= 0 && phell(ich) ? C_SCHWARZ : C_WEISS;
    gfx_rechteck(0, 0, BILD_B, 58, band);
    gfx_text(RAND_X, 22, titel, schrift, 1, 2);
    const char *uhr = tmp("%s  %s", datum_text(tag_von(jetzt)), uhr_text(jetzt));
    int ub = gfx_textbreite(uhr, 1);
    int ax = BILD_B - RAND_X - 32;
    if (ich >= 0) gfx_bild_rund(ax, 18, 24, db.profile.v[ich].bild >= 1 ? avatar24[db.profile.v[ich].bild - 1] : avatar24[0], C_WEISS);
    gfx_text(ax - ub - 12, 26, uhr, schrift, 1, 1);
    if (mit_reitern) {
        gfx_rechteck(0, 58, BILD_B, 32, C_FLAECHE);
        int w = INHALT_B / 5;
        for (int i = 0; i < 5; i++) {
            int x = RAND_X + i * w;
            if (i == reiter) { gfx_runde_box(x + 4, 61, w - 8, 26, 13, gfx_mischen(band, C_WEISS, phell(ich) ? 0 : 40)); }
            gfx_text_mitte(x + w / 2, 66, REITER[i], i == reiter ? (phell(ich) ? C_SCHWARZ : C_WEISS) : C_TEXT2, i == reiter, 1);
        }
    }
}
void fuss(const char *hinweis) {
    gfx_rechteck(0, FUSS_Y, BILD_B, BILD_H - FUSS_Y, C_HG);
    gfx_linie_h(RAND_X, FUSS_Y, INHALT_B, C_LINIE);
    gfx_text_max(RAND_X, FUSS_Y + 8, hinweis, INHALT_B, C_TEXT2, 0);
}

/* ---------- Dialoge ---------- */
enum { D_FRAGE = 1, D_AUSWAHL, D_TASTATUR, D_ZAHL, D_PERSONEN, D_UHR };
static int dlg_id, dlg_wahl, dlg_roll, dlg_n, dlg_max, dlg_seite, dlg_feld;
static char dlg_titel[200], dlg_ja[48], dlg_nein[48];
static const char *const *dlg_punkte;
static char dlg_text[AKTION_PUFFER];
static int32_t dlg_wert;
static uint32_t dlg_maske;
static int erg_typ, erg_id, erg_wert; static int32_t erg_zahl; static uint32_t erg_maske;
static char erg_text[AKTION_PUFFER];

static void schliessen(int wert) { erg_typ = dlg_typ; erg_id = dlg_id; erg_wert = wert; dlg_typ = 0; }
static void oeffnen(int typ, int id, const char *titel) {
    dlg_typ = typ; dlg_id = id; dlg_wahl = 0; dlg_roll = 0; dlg_feld = 0; dlg_seite = 0;
    strncpy(dlg_titel, titel ? titel : "", sizeof dlg_titel - 1); dlg_titel[sizeof dlg_titel - 1] = 0;
    erg_typ = 0;
}

void frage(int id, const char *text, const char *ja, const char *nein) {
    oeffnen(D_FRAGE, id, text);
    strncpy(dlg_ja, ja, sizeof dlg_ja - 1); dlg_ja[sizeof dlg_ja - 1] = 0;
    strncpy(dlg_nein, nein ? nein : "", sizeof dlg_nein - 1); dlg_nein[sizeof dlg_nein - 1] = 0;
}
int frage_ergebnis(int id) { if (erg_typ == D_FRAGE && erg_id == id) { erg_typ = 0; return erg_wert; } return -1; }

void auswahl(int id, const char *titel, const char *const *punkte, int n, int start) {
    oeffnen(D_AUSWAHL, id, titel); dlg_punkte = punkte; dlg_n = n; dlg_wahl = start >= 0 && start < n ? start : 0;
}
int auswahl_ergebnis(int id) { if (erg_typ == D_AUSWAHL && erg_id == id) { erg_typ = 0; return erg_wert; } return -1; }

void tastatur(int id, const char *titel, const char *start, int max) {
    oeffnen(D_TASTATUR, id, titel);
    strncpy(dlg_text, start ? start : "", sizeof dlg_text - 1); dlg_text[sizeof dlg_text - 1] = 0;
    dlg_max = max;
}
const char *tastatur_ergebnis(int id) {
    if (erg_typ == D_TASTATUR && erg_id == id && erg_wert == 0) { erg_typ = 0; return erg_text; }
    return NULL;
}
int erg_tastatur_abgeschlossen(int id) {
    if (erg_typ == D_TASTATUR && erg_id == id && erg_wert != 0) { erg_typ = 0; return 1; }
    return 0;
}
void zahl(int id, const char *titel, int32_t cent) { oeffnen(D_ZAHL, id, titel); dlg_wert = cent; dlg_feld = 4; }
int zahl_ergebnis(int id, int32_t *cent) { if (erg_typ == D_ZAHL && erg_id == id) { erg_typ = 0; *cent = erg_zahl; return erg_wert == 0; } return 0; }
void personen(int id, const char *titel, uint32_t maske) { oeffnen(D_PERSONEN, id, titel); dlg_maske = maske; }
int personen_ergebnis(int id, uint32_t *m) { if (erg_typ == D_PERSONEN && erg_id == id) { erg_typ = 0; *m = erg_maske; return erg_wert == 0; } return 0; }
void uhr_dialog(int id, int32_t sek) { oeffnen(D_UHR, id, "Uhr stellen"); dlg_wert = sek; dlg_feld = 3; }
int uhr_ergebnis(int id, int32_t *sek) { if (erg_typ == D_UHR && erg_id == id) { erg_typ = 0; *sek = erg_zahl; return erg_wert == 0; } return 0; }

void meldung(const char *text) {
    strncpy(meldung_text, text, sizeof meldung_text - 1); meldung_text[sizeof meldung_text - 1] = 0;
    meldung_bis = plat_ms() + 4000;
}

/* Tastatur-Belegung */
static const char *const TASTEN[3][4] = {
    { "1234567890ß-", "qwertzuiopü+", "asdfghjklöä#", "yxcvbnm,.!?'" },
    { "!\"§$%&/()=?_", "QWERTZUIOPÜ*", "ASDFGHJKLÖÄ'", "YXCVBNM;:@<>" },
    { "", "", "", "" } };
#define T_SPALTEN 12
static const char *const STEUER[5] = { "Groß", "Symbole", "Leerzeichen", "Löschen", "Fertig" };
static int taste_code(int seite, int zeile, int spalte) {
    if (seite == 2) {
        int i = zeile * T_SPALTEN + spalte;
        static const uint8_t mehr[] = { 0xb0, 0xb7, 0xe9, 0xe8, 0xe0, 0xe7, 0xf1, 0xab, 0xbb, 0xbf, 0xa1, 0x7e, 0x5b, 0x5d, 0x7b, 0x7d };
        if (i < 32) return 0x80 + i;
        if (i - 32 < (int)sizeof mehr) return mehr[i - 32];
        return 0;
    }
    const char *z = TASTEN[seite][zeile];
    int k = 0, i = 0, c;
    while ((c = utf8_naechstes(z, &i)) != 0) { if (k == spalte) return c; k++; }
    return 0;
}
static void text_anhaengen(int c) {
    if (utf8_laenge(dlg_text) >= dlg_max) return;
    const char *s = sym(c);
    if (strlen(dlg_text) + strlen(s) + 1 >= sizeof dlg_text) return;
    strcat(dlg_text, s);
}
static void text_loeschen(void) {
    int n = utf8_laenge(dlg_text);
    if (n > 0) dlg_text[utf8_bytes_fuer(dlg_text, n - 1)] = 0;
}

static int32_t uhr_feld_aendern(int32_t sek, int feld, int d) {
    tag_t t = (tag_t)(sek / 86400); int rest = (int)(sek % 86400), h = rest / 3600, mi = rest / 60 % 60;
    int j, m, tg; tag_zu_datum(t, &j, &m, &tg);
    switch (feld) {
    case 0: t += d; break;
    case 1: m += d; if (m > 12) { m = 1; j++; } if (m < 1) { m = 12; j--; } if (tg > 28) tg = 28; t = datum_zu_tag(j, m, tg); break;
    case 2: j += d; if (j < 2020) j = 2020; t = datum_zu_tag(j, m, tg > 28 ? 28 : tg); break;
    case 3: h = (h + d + 24) % 24; break;
    case 4: mi = (mi + d + 60) % 60; break;
    }
    if (feld != 1 && feld != 2) { /* Tag bleibt */ }
    if (t < 0) t = 0;
    return (int32_t)t * 86400 + h * 3600 + mi * 60;
}

static void dialog_eingabe(uint32_t t) {
    switch (dlg_typ) {
    case D_FRAGE:
        if (t & (T_LINKS | T_RECHTS)) dlg_wahl = dlg_nein[0] ? !dlg_wahl : 0;
        if (t & T_A) schliessen(dlg_wahl);
        else if (t & T_B) schliessen(1);
        break;
    case D_AUSWAHL:
        if (t & T_HOCH) dlg_wahl = (dlg_wahl + dlg_n - 1) % dlg_n;
        if (t & T_RUNTER) dlg_wahl = (dlg_wahl + 1) % dlg_n;
        if (t & T_A) schliessen(dlg_wahl);
        else if (t & T_B) schliessen(-2);
        break;
    case D_PERSONEN: {
        int n = db.profile.n + 2;   /* Alle, Profile, Fertig */
        if (t & T_HOCH) dlg_wahl = (dlg_wahl + n - 1) % n;
        if (t & T_RUNTER) dlg_wahl = (dlg_wahl + 1) % n;
        if (t & T_A) {
            if (dlg_wahl == 0) {
                uint32_t alle = 0;
                for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv) alle |= 1u << p;
                dlg_maske = dlg_maske == alle ? 0 : alle;
            } else if (dlg_wahl == n - 1) { erg_maske = dlg_maske; schliessen(0); }
            else dlg_maske ^= 1u << (dlg_wahl - 1);
        }
        if (dlg_typ && (t & T_START)) { erg_maske = dlg_maske; schliessen(0); }
        else if (dlg_typ && (t & T_B)) schliessen(1);
        break;
    }
    case D_ZAHL: {
        static const int32_t STELLE[7] = { 1000000, 100000, 10000, 1000, 100, 10, 1 };
        if (t & T_LINKS) dlg_feld = dlg_feld > 0 ? dlg_feld - 1 : 0;
        if (t & T_RECHTS) dlg_feld = dlg_feld < 6 ? dlg_feld + 1 : 6;
        int32_t s = STELLE[dlg_feld], ziffer = dlg_wert / s % 10;
        if (t & T_HOCH) dlg_wert += ziffer == 9 ? -9 * s : s;
        if (t & T_RUNTER) dlg_wert += ziffer == 0 ? 9 * s : -s;
        if (t & (T_A | T_START)) { erg_zahl = dlg_wert; schliessen(0); }
        else if (t & T_B) schliessen(1);
        break;
    }
    case D_UHR:
        if (t & T_LINKS) dlg_feld = dlg_feld > 0 ? dlg_feld - 1 : 0;
        if (t & T_RECHTS) dlg_feld = dlg_feld < 4 ? dlg_feld + 1 : 4;
        if (t & T_HOCH) dlg_wert = uhr_feld_aendern(dlg_wert, dlg_feld, 1);
        if (t & T_RUNTER) dlg_wert = uhr_feld_aendern(dlg_wert, dlg_feld, -1);
        if (t & (T_A | T_START)) { erg_zahl = dlg_wert; schliessen(0); }
        else if (t & T_B) schliessen(1);
        break;
    case D_TASTATUR: {
        /* dlg_feld = Zeile (0..3 Tasten, 4 Steuerzeile), dlg_wahl = Spalte */
        int zeilen = 5;
        if (t & T_HOCH) dlg_feld = (dlg_feld + zeilen - 1) % zeilen;
        if (t & T_RUNTER) dlg_feld = (dlg_feld + 1) % zeilen;
        int spalten = dlg_feld == 4 ? 5 : T_SPALTEN;
        if (dlg_feld == 4 && dlg_wahl > 4) dlg_wahl = 4;
        if (t & T_LINKS) dlg_wahl = (dlg_wahl + spalten - 1) % spalten;
        if (t & T_RECHTS) dlg_wahl = (dlg_wahl + 1) % spalten;
        if (t & T_Z) dlg_seite = dlg_seite == 1 ? 0 : 1;
        if (t & T_L) dlg_seite = dlg_seite == 2 ? 0 : 2;
        if (t & T_R) text_anhaengen(' ');
        if (t & T_B) { if (dlg_text[0]) text_loeschen(); else { schliessen(1); break; } }
        if (t & T_START) { strcpy(erg_text, dlg_text); schliessen(0); break; }
        if (t & T_A) {
            if (dlg_feld < 4) {
                int c = taste_code(dlg_seite, dlg_feld, dlg_wahl);
                if (c) text_anhaengen(c);
                if (dlg_seite == 1) dlg_seite = 0;   /* Großschreibung gilt für einen Buchstaben */
            } else switch (dlg_wahl) {
                case 0: dlg_seite = dlg_seite == 1 ? 0 : 1; break;
                case 1: dlg_seite = dlg_seite == 2 ? 0 : 2; break;
                case 2: text_anhaengen(' '); break;
                case 3: text_loeschen(); break;
                case 4: strcpy(erg_text, dlg_text); schliessen(0); break;
            }
        }
        break;
    }
    }
}

static void kasten(int x, int y, int b, int h) {
    gfx_runde_box(x - 3, y - 3, b + 6, h + 6, 18, C_LINIE);
    gfx_runde_box(x, y, b, h, 16, C_FLAECHE);
}
static void dlg_knopf(int x, int y, int b, const char *s, int f, uint16_t farbe) {
    if (f) gfx_runde_box(x - 3, y - 3, b + 6, 38, 19, C_WEISS);
    gfx_runde_box(x, y, b, 32, 16, f ? gfx_mischen(farbe, C_WEISS, 60) : farbe);
    gfx_text_mitte(x + b / 2, y + 8, s, C_TEXT, 1, 1);
}

void dialoge_zeichnen(void) {
    gfx_clip_aus();
    if (dlg_typ) {
        /* Hintergrund abdunkeln (jede zweite Zeile) */
        for (int y = 0; y < BILD_H; y += 2) gfx_rechteck(0, y, BILD_B, 1, C_SCHWARZ);
    }
    switch (dlg_typ) {
    case D_FRAGE: {
        const char *st[12]; int ln[12];
        int n = umbrechen(dlg_titel, 60, st, ln, 12);
        int h = 40 + n * 20 + 60, y = (BILD_H - h) / 2, x = 60, b = BILD_B - 120;
        kasten(x, y, b, h);
        for (int i = 0; i < n; i++) gfx_text_n(x + 24, y + 24 + i * 20, st[i], ln[i], C_TEXT, i == 0, 1);
        int by = y + h - 50;
        if (dlg_nein[0]) {
            dlg_knopf(x + 24, by, (b - 72) / 2, dlg_ja, dlg_wahl == 0, C_FLAECHE2);
            dlg_knopf(x + 48 + (b - 72) / 2, by, (b - 72) / 2, dlg_nein, dlg_wahl == 1, C_FLAECHE2);
        } else dlg_knopf(x + 24, by, b - 48, dlg_ja, 1, C_FLAECHE2);
        break;
    }
    case D_AUSWAHL: {
        int sicht = dlg_n < 10 ? dlg_n : 10;
        if (dlg_wahl < dlg_roll) dlg_roll = dlg_wahl;
        if (dlg_wahl >= dlg_roll + sicht) dlg_roll = dlg_wahl - sicht + 1;
        int h = 60 + sicht * 34, y = (BILD_H - h) / 2, x = 90, b = BILD_B - 180;
        kasten(x, y, b, h);
        gfx_text_max(x + 20, y + 18, dlg_titel, b - 40, C_TEXT, 1);
        for (int i = 0; i < sicht; i++) {
            int k = dlg_roll + i, yy = y + 48 + i * 34;
            if (k == dlg_wahl) gfx_runde_box(x + 12, yy - 4, b - 24, 30, 15, C_FLAECHE2);
            gfx_text_max(x + 26, yy + 3, dlg_punkte[k], b - 52, k == dlg_wahl ? C_WEISS : C_TEXT2, k == dlg_wahl);
        }
        if (dlg_roll > 0) gfx_text(x + b - 30, y + 48, S_HOCH, C_TEXT2, 0, 1);
        if (dlg_roll + sicht < dlg_n) gfx_text(x + b - 30, y + h - 26, S_RUNTER, C_TEXT2, 0, 1);
        break;
    }
    case D_PERSONEN: {
        int n = db.profile.n + 2, sicht = n < 11 ? n : 11;
        if (dlg_wahl < dlg_roll) dlg_roll = dlg_wahl;
        if (dlg_wahl >= dlg_roll + sicht) dlg_roll = dlg_wahl - sicht + 1;
        int h = 60 + sicht * 32, y = (BILD_H - h) / 2, x = 120, b = BILD_B - 240;
        kasten(x, y, b, h);
        gfx_text_max(x + 20, y + 16, dlg_titel, b - 40, C_TEXT, 1);
        for (int i = 0; i < sicht; i++) {
            int k = dlg_roll + i, yy = y + 46 + i * 32;
            if (k == dlg_wahl) gfx_runde_box(x + 12, yy - 4, b - 24, 30, 15, C_FLAECHE2);
            if (k == 0) gfx_text(x + 26, yy + 3, "Alle / keiner", C_TEXT, 1, 1);
            else if (k == n - 1) gfx_text(x + 26, yy + 3, S_HAKEN " Fertig", C_GRUEN, 1, 1);
            else {
                int p = k - 1;
                gfx_text(x + 26, yy + 3, (dlg_maske & (1u << p)) ? S_BOX_JA : S_BOX, C_TEXT, 0, 1);
                avatar(p, x + 50, yy - 1, 24);
                gfx_text(x + 84, yy + 3, pname(p), db.profile.v[p].aktiv ? C_TEXT : C_TEXT2, 0, 1);
            }
        }
        break;
    }
    case D_ZAHL: {
        int h = 190, y = (BILD_H - h) / 2, x = 120, b = BILD_B - 240;
        kasten(x, y, b, h);
        gfx_text_max(x + 20, y + 18, dlg_titel, b - 40, C_TEXT, 1);
        static const int32_t STELLE_W[7] = { 1000000, 100000, 10000, 1000, 100, 10, 1 };
        char z[8]; snprintf(z, sizeof z, "%07ld", (long)dlg_wert);
        int zx = x + b / 2 - (8 * 24 + 8) / 2, zy = y + 70;
        for (int i = 0; i < 7; i++) {
            int px = zx + i * 24 + (i >= 5 ? 16 : 0);
            if (i == dlg_feld) { gfx_runde_box(px - 2, zy - 6, 28, 44, 6, C_FLAECHE2); gfx_text(px + 4, zy - 26, S_HOCH, C_TEXT2, 0, 1); gfx_text(px + 4, zy + 40, S_RUNTER, C_TEXT2, 0, 1); }
            char c[2] = { z[i], 0 };
            gfx_text(px, zy, c, i < 4 && dlg_wert < STELLE_W[i] ? C_TEXT2 : C_WEISS, 1, 2);
        }
        gfx_text(zx + 5 * 24 - 4, zy, ",", C_WEISS, 1, 2);
        gfx_text(zx + 7 * 24 + 22, zy, S_EURO, C_WEISS, 1, 2);
        gfx_text_mitte(x + b / 2, y + h - 28, "Steuerkreuz: Ziffer   A: fertig   B: abbrechen", C_TEXT2, 0, 1);
        break;
    }
    case D_UHR: {
        int h = 190, y = (BILD_H - h) / 2, x = 100, b = BILD_B - 200;
        kasten(x, y, b, h);
        gfx_text(x + 20, y + 18, dlg_titel, C_TEXT, 1, 1);
        tag_t t = (tag_t)(dlg_wert / 86400); int rest = (int)(dlg_wert % 86400);
        int j, m, tg; tag_zu_datum(t, &j, &m, &tg);
        const char *teile[5] = { tmp("%02d", tg), tmp("%02d", m), tmp("%04d", j), tmp("%02d", rest / 3600), tmp("%02d", rest / 60 % 60) };
        const char *trenn[5] = { ".", ".", "   ", ":", "" };
        int px = x + 40, zy = y + 76;
        gfx_text(x + 20, y + 44, WOCHENTAG_LANG[wochentag(t)], C_TEXT2, 0, 1);
        for (int i = 0; i < 5; i++) {
            int w = gfx_textbreite(teile[i], 2);
            if (i == dlg_feld) { gfx_runde_box(px - 4, zy - 6, w + 8, 44, 6, C_FLAECHE2); gfx_text_mitte(px + w / 2, zy - 26, S_HOCH, C_TEXT2, 0, 1); gfx_text_mitte(px + w / 2, zy + 40, S_RUNTER, C_TEXT2, 0, 1); }
            gfx_text(px, zy, teile[i], C_WEISS, 1, 2);
            px += w;
            px += gfx_text(px, zy, trenn[i], C_WEISS, 1, 2);
        }
        gfx_text_mitte(x + b / 2, y + h - 28, "Steuerkreuz: stellen   A: übernehmen   B: abbrechen", C_TEXT2, 0, 1);
        break;
    }
    case D_TASTATUR: {
        int x = 40, y = 70, b = BILD_B - 80, h = 360;
        kasten(x, y, b, h);
        gfx_text_max(x + 20, y + 14, dlg_titel, b - 40, C_TEXT, 1);
        /* Textfeld, die letzten Zeilen */
        gfx_runde_box(x + 16, y + 40, b - 32, 64, 10, C_HG);
        const char *st[32]; int ln[32];
        char mit_cursor[AKTION_PUFFER + 4];
        snprintf(mit_cursor, sizeof mit_cursor, "%s_", dlg_text);
        int n = umbrechen(mit_cursor, (b - 56) / 8, st, ln, 32), ab = n > 3 ? n - 3 : 0;
        for (int i = ab; i < n; i++) gfx_text_n(x + 28, y + 46 + (i - ab) * 18, st[i], ln[i], C_TEXT, 0, 1);
        gfx_text(x + b - 110, y + 108, tmp("%d/%d", utf8_laenge(dlg_text), dlg_max), C_TEXT2, 0, 1);
        int kx = x + 22, ky = y + 130, kb = (b - 44) / T_SPALTEN;
        for (int z = 0; z < 4; z++)
            for (int s = 0; s < T_SPALTEN; s++) {
                int c = taste_code(dlg_seite, z, s);
                int px = kx + s * kb, py = ky + z * 38, f = dlg_feld == z && dlg_wahl == s;
                if (f) gfx_runde_box(px - 2, py - 2, kb, 36, 8, C_WEISS);
                gfx_runde_box(px, py, kb - 4, 32, 7, f ? gfx_mischen(C_FLAECHE2, C_WEISS, 80) : C_FLAECHE2);
                if (c) gfx_text_mitte(px + (kb - 4) / 2, py + 8, sym(c), C_TEXT, 1, 1);
            }
        int sy2 = ky + 4 * 38 + 4, sb[5] = { 2, 2, 4, 2, 2 }, sx = kx;
        for (int i = 0; i < 5; i++) {
            int w = sb[i] * kb, f = dlg_feld == 4 && dlg_wahl == i;
            int an = (i == 0 && dlg_seite == 1) || (i == 1 && dlg_seite == 2);
            if (f) gfx_runde_box(sx - 2, sy2 - 2, w, 36, 8, C_WEISS);
            gfx_runde_box(sx, sy2, w - 4, 32, 7, i == 4 ? C_GRUEN : an ? C_BLAU : (f ? gfx_mischen(C_FLAECHE2, C_WEISS, 80) : C_FLAECHE2));
            gfx_text_mitte(sx + (w - 4) / 2, sy2 + 8, STEUER[i], C_TEXT, 1, 1);
            sx += w;
        }
        gfx_text_mitte(BILD_B / 2, y + h - 22, "A tippen  B löschen  Z groß  L Symbole  R Leerzeichen  START fertig", C_TEXT2, 0, 1);
        break;
    }
    }
    if (meldung_text[0] && plat_ms() < meldung_bis) {
        const char *st[6]; int ln[6];
        int n = umbrechen(meldung_text, 66, st, ln, 6);
        int h = 20 + n * 18, y = FUSS_Y - h - 12, x = 48, b = BILD_B - 96;
        gfx_runde_box(x - 2, y - 2, b + 4, h + 4, 14, C_GELB);
        gfx_runde_box(x, y, b, h, 12, RGB(0x30, 0x2a, 0x10));
        for (int i = 0; i < n; i++) gfx_text_n(x + 16, y + 10 + i * 18, st[i], ln[i], C_WEISS, 0, 1);
    } else meldung_text[0] = 0;
}
int ui_meldung_aktiv(void) { return meldung_text[0] != 0; }

/* ---------- weitere Bausteine ---------- */
void ui_kachel(int x, int y, int b, int h) { element(x, y - ber_y0 + roll, b, h); }
void ui_weiter(int dy) { cur_y += dy; }
void ui_fokus_letztes(void) { fokus = n_elem >= 2 ? n_elem - 2 : 0; }
static int mitte_alt = -1;
void ui_mitte(int an) {
    if (an) { mitte_alt = x0; x0 = BILD_B / 2 - 170; }
    else if (mitte_alt >= 0) { x0 = mitte_alt; mitte_alt = -1; }
}
int ui_systemzeile(const char *text, const char *zeit) {
    const char *st[8]; int ln[8];
    int n = umbrechen(text, 64, st, ln, 8);
    int h = n * 18 + 8, f = element(x0, cur_y, breite, h);
    int y = sy(cur_y);
    gfx_runde_box(x0 + 40, y, breite - 80, h, 10, f ? C_FLAECHE2 : RGB(0x1a, 0x19, 0x20));
    if (f) gfx_rahmen(x0 + 40, y, breite - 80, h, C_WEISS);
    for (int i = 0; i < n; i++) gfx_text_n(x0 + 56, y + 4 + i * 18, st[i], ln[i], C_TEXT2, 0, 1);
    gfx_text(x0 + breite - 40 - 48, y + h - 18, zeit, C_TEXT2, 0, 1);
    cur_y += h + 6;
    return f && (taste & T_A);
}
int ui_blase(int profil, const char *name, const char *text, const char *zeit, int eigen, const char *extra, const char *antwort) {
    const char *st[40]; int ln[40];
    int spalten = 56;
    int n = umbrechen(text && text[0] ? text : " ", spalten, st, ln, 40);
    int breit = 0;
    for (int i = 0; i < n; i++) if (ln[i] > breit) breit = ln[i];
    int nb = gfx_textbreite(name, 1) / 8;
    if (!eigen && nb > breit) breit = nb;
    if (extra) { int e = utf8_laenge(extra); if (e > breit) breit = e > spalten ? spalten : e; }
    if (antwort) { int e = utf8_laenge(antwort) + 2; if (e > breit) breit = e > spalten ? spalten : e; }
    if (breit < 8) breit = 8;
    int bb = breit * 8 + 72;
    int h = n * 18 + 30 + (eigen ? 0 : 18) + (extra ? 18 : 0) + (antwort ? 20 : 0);
    int bx = eigen ? x0 + breite - bb : x0 + 56;
    int f = element(bx, cur_y, bb, h);
    int y = sy(cur_y);
    if (!eigen) avatar(profil, x0, y, 48);
    uint16_t farbe = eigen ? gfx_mischen(pfarbe(profil), C_FLAECHE, 120) : C_FLAECHE;
    if (f) gfx_runde_box(bx - 3, y - 3, bb + 6, h + 6, 15, C_WEISS);
    gfx_runde_box(bx, y, bb, h, 12, farbe);
    int ty = y + 8;
    if (!eigen) { gfx_text(bx + 14, ty, name, phell(profil) ? C_GELB : gfx_mischen(pfarbe(profil), C_WEISS, 140), 1, 1); ty += 18; }
    if (antwort) { gfx_rechteck(bx + 14, ty, 3, 16, C_TEXT2); gfx_text_max(bx + 22, ty, antwort, bb - 36, C_TEXT2, 0); ty += 20; }
    for (int i = 0; i < n; i++) { gfx_text_n(bx + 14, ty, st[i], ln[i], C_TEXT, 0, 1); ty += 18; }
    if (extra) { gfx_text_max(bx + 14, ty, extra, bb - 28, C_BLAU, 0); ty += 18; }
    gfx_text(bx + bb - 14 - 40, y + h - 20, zeit, C_TEXT2, 0, 1);
    cur_y += h + 8;
    return f && (taste & T_A);
}
