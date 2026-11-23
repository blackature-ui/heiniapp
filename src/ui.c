/* ui.c – Oberfläche im Aussehen der Heini-App: Kopf, Reiterleiste, Karten, Knöpfe, Listen, Blätter */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"
#include "gfx.h"
#include "text.h"
#include "assets.h"
#include "logik.h"
#include "aktion.h"
#define PUFFER (AKTION_TEXT_MAX + 1)

/* ---------- Zustand ---------- */
typedef struct { int16_t x, y, b, h; uint8_t fest; } Elem;
#define MAX_ELEM 400
static Elem elem[MAX_ELEM], alt[MAX_ELEM];
static int n_elem, n_alt, fokus = -1, nochmal;
static uint32_t taste;
static int roll, ber_y0 = KOPF_HAUPT, ber_y1 = LEISTE_Y, cur_y, inhalt_h;
static int x0 = RAND_X, breite = INHALT_B;
static int reihe_x, reihe_y;
static int letzt_fokus;
#define MAX_KARTEN 64
static int karten_h[MAX_KARTEN], karten_h_neu[MAX_KARTEN], n_karten, karte_start, karte_offen;
static int dlg_typ;
static uint32_t meldung_bis; static char meldung_text[220];

uint32_t ui_taste(void) { return taste; }
int ui_dialog_offen(void) { return dlg_typ != 0; }
static int seite_gewechselt, roll_halten;
void ui_roll_auf(int y) { roll = y < 0 ? 0 : y; roll_halten = 1; }
void ui_nochmal(void) { nochmal = 1; }
void ui_seite_neu(void) { fokus = -1; roll = 0; n_alt = 0; memset(karten_h, 0, sizeof karten_h); seite_gewechselt = 1; }
static int sy(int y) { return ber_y0 + y - roll; }
static int elem_y(const Elem *e) { return e->fest ? e->y : ber_y0 + e->y - roll; }

static void dialog_eingabe(uint32_t t);

void ui_bild_start(const Eingabe *e) {
    uint32_t roh = e ? e->gedrueckt : 0;
    taste = 0;
    if (roh) roll_halten = 0;
    if (dlg_typ) dialog_eingabe(roh);
    else {
        taste = roh;
        if (n_alt > 0 && fokus >= 0) {
            if (fokus >= n_alt) fokus = n_alt - 1;
            const Elem *f = &alt[fokus];
            int fy = elem_y(f), cx = f->x + f->b / 2;
            if (taste & (T_HOCH | T_RUNTER)) {
                int runter = (taste & T_RUNTER) != 0, best = -1, bd = 1 << 30;
                for (int i = 0; i < n_alt; i++) {
                    int dy = elem_y(&alt[i]) - fy;
                    if (runter ? dy <= 4 : dy >= -4) continue;
                    int d = abs(dy) * 4 + abs(alt[i].x + alt[i].b / 2 - cx);
                    if (d < bd) { bd = d; best = i; }
                }
                if (best >= 0) fokus = best;
                else if (!runter) roll = 0;
                else if (inhalt_h > ber_y1 - ber_y0) roll = inhalt_h - (ber_y1 - ber_y0);
            }
            if (taste & (T_LINKS | T_RECHTS)) {
                int rechts = (taste & T_RECHTS) != 0, best = -1, bd = 1 << 30;
                for (int i = 0; i < n_alt; i++) {
                    if (i == fokus || abs(elem_y(&alt[i]) - fy) > 6) continue;
                    int dx = alt[i].x - f->x;
                    if (rechts ? dx <= 0 : dx >= 0) continue;
                    if (abs(dx) < bd) { bd = abs(dx); best = i; }
                }
                if (best >= 0) fokus = best;
            }
        } else if (n_alt == 0) {
            int seite = ber_y1 - ber_y0;
            if (taste & T_RUNTER) roll += 48;
            if (taste & T_HOCH) roll -= 48;
            if (roll > inhalt_h - seite) roll = inhalt_h - seite;
        }
        if (taste & T_C_RUNTER) roll += 140;
        if (taste & T_C_HOCH) roll -= 140;
        if (taste & T_C_RUNTER) { int seite = ber_y1 - ber_y0; if (roll > inhalt_h - seite) roll = inhalt_h - seite; }
        if (roll < 0) roll = 0;
        if (taste & (T_C_RUNTER | T_C_HOCH)) {   /* Fokus auf das erste sichtbare Element */
            for (int i = 0; i < n_alt; i++) if (!alt[i].fest && alt[i].y >= roll) { fokus = i; break; }
        }
    }
    n_elem = 0; nochmal = 0; n_karten = 0; karte_offen = 0; seite_gewechselt = 0;
}

int ui_bild_ende(void) {
    if (seite_gewechselt) { seite_gewechselt = 0; n_alt = 0; fokus = -1; return 1; }
    memcpy(alt, elem, sizeof(Elem) * (size_t)n_elem);
    n_alt = n_elem;
    if (fokus < 0 && n_alt) {   /* neue Seite: erstes Element im Inhalt */
        fokus = 0;
        for (int i = 0; i < n_alt; i++) if (!alt[i].fest) { fokus = i; break; }
        nochmal = 1;
    }
    if (fokus >= n_alt) fokus = n_alt ? n_alt - 1 : -1;
    for (int i = 0; i < n_karten; i++) if (karten_h[i] != karten_h_neu[i]) { karten_h[i] = karten_h_neu[i]; nochmal = 1; }
    if (n_alt && fokus >= 0 && !alt[fokus].fest && !roll_halten && !(taste & (T_C_HOCH | T_C_RUNTER))) {
        const Elem *f = &alt[fokus];
        int seite = ber_y1 - ber_y0, neu = roll;
        if (f->y - 14 < neu) neu = f->y - 14;
        if (f->y + f->h + 14 > neu + seite) neu = f->y + f->h + 14 - seite;
        if (f->y + f->h < seite - 30) neu = 0;
        if (neu < 0) neu = 0;
        if (neu != roll) { roll = neu; nochmal = 1; }
    }
    int r = nochmal;
    nochmal = 0;
    return r;
}

void ui_bereich(int y0, int y1) {
    ber_y0 = y0; ber_y1 = y1; cur_y = 10; x0 = RAND_X; breite = INHALT_B;
    gfx_clip(0, y0, BILD_B, y1);
}
void ui_bereich_ende(void) {
    inhalt_h = cur_y + 12;
    gfx_clip_aus();
}
int ui_y(void) { return sy(cur_y); }
int ui_x(void) { return x0; }
int ui_breite(void) { return breite; }
void ui_abstand(int px) { cur_y += px; }
void ui_weiter(int dy) { cur_y += dy; }

static int element_f(int x, int y, int b, int h, int fest) {
    int i = n_elem;
    if (n_elem < MAX_ELEM) elem[n_elem++] = (Elem){ (int16_t)x, (int16_t)y, (int16_t)b, (int16_t)h, (uint8_t)fest };
    letzt_fokus = (i == fokus) && !dlg_typ;
    return letzt_fokus;
}
static int element(int x, int y, int b, int h) { return element_f(x, y, b, h, 0); }
int ui_fokus_hier(void) { return n_elem == fokus && !dlg_typ; }
int ui_fokussiert_letztes(void) { return letzt_fokus; }
void ui_fokus_letztes(void) { fokus = n_elem >= 2 ? n_elem - 2 : 0; }
void ui_fokus_rahmen(int x, int y, int b, int h, int r) {
    gfx_runder_rahmen(x - 4, y - 4, b + 8, h + 8, r + 4, 3, TH->text);
}

/* ---------- Inhalte ---------- */
void ui_ueberschrift(const char *s, const char *rechts) {
    cur_y += 8;
    int y = sy(cur_y);
    gfx_runde_box(x0, y + 3, 4, 20, 2, TH->accent);
    text(x0 + 14, y + 1, s, F_TITEL, TH->text);
    if (rechts) {
        int b = text_breite(rechts, F_FETT) + 20;
        gfx_runde_box(x0 + breite - b, y + 1, b, 24, 12, TH->accentCont);
        text(x0 + breite - b + 10, y + 4, rechts, F_FETT, TH->onAccentCont);
    }
    cur_y += 34;
}

static int karte_art_aktiv;
static uint16_t karte_hg(int art) {
    switch (art) {
    case KA_AKZENT: return TH->accentCont;
    case KA_OK: return TH->okBg;
    case KA_ERR: return TH->errBg;
    case KA_WARN: return TH->warnBg;
    case KA_GOLD: return TH->goldBg1;
    case KA_ALARM: return TH->alarmBg;
    case KA_FLAECHE2: return TH->flaeche2;
    }
    return TH->flaeche;
}
void ui_karte_art(int art, const char *titel, int symbol) {
    int h = n_karten < MAX_KARTEN ? karten_h[n_karten] : 0;
    if (h <= 0) h = 60;
    int y = sy(cur_y), r = art == KA_OK || art == KA_ERR ? 24 : 16;
    if (art == KA_GOLD) {
        gfx_verlauf_box(x0, y, breite, h, r, TH->goldBg1, TH->goldBg2);
        gfx_runder_rahmen(x0, y, breite, h, r, 2, TH->goldRand);
    } else gfx_runde_box(x0, y, breite, h, r, karte_hg(art));
    karte_start = cur_y;
    karte_art_aktiv = art;
    if (titel) {
        if (art == KA_GOLD) {
            if (symbol >= 0) gfx_symbol(symbol, x0 + 16, y + 14, 28, TH->goldText);
            text_max(x0 + 52, y + 16, titel, breite - 70, F_TITEL, TH->goldText);
            cur_y += 52;
        } else {
            gfx_runde_box(x0, y, breite, 52, r, TH->oben);
            gfx_rechteck(x0, y + 30, breite, 22, TH->oben);
            if (symbol >= 0) {
                gfx_kreis(x0 + 32, y + 26, 16, gfx_mischen(TH->oben, TH->onOben, 40));
                gfx_symbol(symbol, x0 + 22, y + 16, 20, TH->onOben);
            }
            text_max(x0 + (symbol >= 0 ? 58 : 16), y + 16, titel, breite - 74, F_FETT, TH->onOben);
            cur_y += 64;
        }
    } else cur_y += 16;
    karte_offen = 1;
    x0 += 16; breite -= 32;
}
void ui_karte(const char *titel, int symbol) { ui_karte_art(KA_NORMAL, titel, symbol); }
void ui_karte_ende(void) {
    if (!karte_offen) return;
    x0 -= 16; breite += 32;
    cur_y += 12;
    if (n_karten < MAX_KARTEN) karten_h_neu[n_karten++] = cur_y - karte_start;
    karte_offen = 0;
    cur_y += 12;
}

void ui_text(const char *s, int f, uint16_t farbe) {
    const char *st[48]; int ln[48];
    int n = text_umbrechen(s, f, breite, st, ln, 48), lh = text_hoehe(f) + 1;
    for (int i = 0; i < n; i++) { text_n(x0, sy(cur_y), st[i], ln[i], f, farbe); cur_y += lh; }
    cur_y += 3;
}
void ui_zeile(const char *links, const char *rechts, uint16_t farbe) {
    int rb = rechts ? text_breite(rechts, F_FETT) : 0;
    text_max(x0, sy(cur_y), links, breite - rb - 12, F_NORMAL, farbe);
    if (rechts) text(x0 + breite - rb, sy(cur_y), rechts, F_FETT, farbe);
    cur_y += 24;
}
void ui_symbolzeile(int symbol, const char *s, uint16_t farbe) {
    gfx_symbol(symbol, x0, sy(cur_y) + 1, 20, farbe);
    const char *st[8]; int ln[8];
    int n = text_umbrechen(s, F_NORMAL, breite - 32, st, ln, 8);
    for (int i = 0; i < n; i++) { text_n(x0 + 32, sy(cur_y), st[i], ln[i], F_NORMAL, farbe); cur_y += 21; }
    cur_y += 5;
}

static void knopf_farben(int art, uint16_t *hg, uint16_t *vg) {
    switch (art) {
    case KN_VOLL: *hg = TH->accent; *vg = TH->onAccent; break;
    case KN_RAND: *hg = karte_offen ? karte_hg(karte_art_aktiv) : TH->bg; *vg = TH->accentText; break;
    case KN_GEFAHR: *hg = TH->alarm; *vg = TH->onAlarm; break;
    case KN_OK: *hg = TH->ok; *vg = TH->okBg; break;
    case KN_ERR: *hg = TH->err; *vg = TH->errBg; break;
    case KN_HELL: *hg = C_WEISS; *vg = TH->alarm; break;
    case KN_KLEIN: *hg = TH->flaeche2; *vg = TH->text; break;
    default: *hg = TH->accentCont; *vg = TH->onAccentCont; break;
    }
}
static int knopf_zeichnen(int x, int y, int b, int h, const char *s, int art, int symbol, int f) {
    uint16_t hg, vg; knopf_farben(art, &hg, &vg);
    int fo = element(x, y, b, h);
    int py = sy(y), r = h / 2;
    if (fo) ui_fokus_rahmen(x, py, b, h, r);
    gfx_runde_box(x, py, b, h, r, fo ? gfx_mischen(hg, vg, 30) : hg);
    if (art == KN_RAND) gfx_runder_rahmen(x, py, b, h, r, 2, TH->edge);
    int tb = text_breite(s, f) + (symbol >= 0 ? 28 : 0), tx = x + (b - tb) / 2;
    if (tx < x + 12) tx = x + 12;
    if (symbol >= 0) { gfx_symbol(symbol, tx, py + (h - 20) / 2, 20, vg); tx += 28; }
    text_max(tx, py + (h - text_hoehe(f)) / 2, s, x + b - 12 - tx, f, vg);
    return fo;
}
static int knopf_b(const char *s, int symbol) { return text_breite(s, F_FETT) + 40 + (symbol >= 0 ? 28 : 0); }
int ui_knopf_art(const char *s, int art, int symbol) {
    int b = knopf_b(s, symbol); if (b > breite) b = breite;
    reihe_y = cur_y;
    int f = knopf_zeichnen(x0, cur_y, b, 44, s, art, symbol, F_FETT);
    reihe_x = x0 + b + 10;
    cur_y += 54;
    return f && (taste & T_A);
}
int ui_knopf(const char *s) { return ui_knopf_art(s, KN_TONAL, -1); }
int ui_knopf_neben_art(const char *s, int art, int symbol) {
    int b = knopf_b(s, symbol);
    if (reihe_x + b > x0 + breite) return ui_knopf_art(s, art, symbol);
    int f = knopf_zeichnen(reihe_x, reihe_y, b, 44, s, art, symbol, F_FETT);
    reihe_x += b + 10;
    return f && (taste & T_A);
}
int ui_knopf_neben(const char *s) { return ui_knopf_neben_art(s, KN_TONAL, -1); }
int ui_knopf_breit(const char *s, int art, int symbol) {
    reihe_y = cur_y; reihe_x = x0 + breite;
    int f = knopf_zeichnen(x0, cur_y, breite, 56, s, art, symbol, F_TITEL);
    cur_y += 66;
    return f && (taste & T_A);
}
int ui_segment(const char *const *namen, int n, int aktiv) {
    int y = cur_y, w = breite / n, ret = -1;
    gfx_runde_box(x0, sy(y), breite, 44, 22, TH->flaeche2);
    for (int i = 0; i < n; i++) {
        int x = x0 + i * w, f = element(x, y, w, 44);
        if (i == aktiv) gfx_runde_box(x + 2, sy(y) + 2, w - 4, 40, 20, C_WEISS);
        if (f) ui_fokus_rahmen(x + 2, sy(y) + 2, w - 4, 40, 20);
        uint16_t vg = i == aktiv ? TH->accent : TH->text;
        int tb = text_breite(namen[i], F_FETT) + (i == aktiv ? 24 : 0), tx = x + (w - tb) / 2;
        if (i == aktiv) { gfx_symbol(IC_CHECK, tx, sy(y) + 12, 20, vg); tx += 24; }
        text(tx, sy(y) + 12, namen[i], F_FETT, vg);
        if (f && (taste & T_A)) ret = i;
    }
    cur_y += 54;
    return ret;
}

static void zeile_zeichnen(int links_breite, const char *titel, const char *unter, const char *rechts, int f, int y) {
    int py = sy(y), h = 58;
    if (f) { ui_fokus_rahmen(x0 - 6, py, breite + 12, h, 12); gfx_runde_box(x0 - 6, py, breite + 12, h, 12, gfx_mischen(karte_offen ? karte_hg(karte_art_aktiv) : TH->bg, TH->text, 30)); }
    int tx = x0 + links_breite, rb = rechts ? text_breite(rechts, F_FETT) + 10 : 0;
    int zw = breite - links_breite - rb;
    if (unter && unter[0]) {
        text_max(tx, py + 9, titel, zw, F_FETT, TH->text);
        text_max(tx, py + 31, unter, zw, F_KLEIN, TH->muted);
    } else text_max(tx, py + 19, titel, zw, F_FETT, TH->text);
    if (rechts) text_rechts(x0 + breite, py + 19, rechts, F_FETT, TH->muted);
}
int ui_eintrag(int p, const char *titel, const char *unter, const char *rechts) {
    int f = element(x0 - 6, cur_y, breite + 12, 58);
    zeile_zeichnen(56, titel, unter, rechts, f, cur_y);
    avatar(p, x0, sy(cur_y) + 9, 40);
    cur_y += 62;
    return f && (taste & T_A);
}
int ui_eintrag_symbol(int symbol, const char *emoji, const char *titel, const char *unter, const char *rechts) {
    int f = element(x0 - 6, cur_y, breite + 12, 58);
    zeile_zeichnen(56, titel, unter, rechts, f, cur_y);
    int py = sy(cur_y);
    gfx_kreis(x0 + 20, py + 29, 20, TH->flaeche2);
    if (emoji && emoji[0]) { int i = 0, cp = utf8_naechstes(emoji, &i), e = emoji_index((uint32_t)cp); if (e >= 0) gfx_emoji(e, x0 + 9, py + 18, 22); else text_mitte(x0 + 20, py + 19, emoji, F_FETT, TH->text); }
    else if (symbol >= 0) gfx_symbol(symbol, x0 + 10, py + 19, 20, TH->accentText);
    cur_y += 62;
    return f && (taste & T_A);
}
void ui_info_symbol(int symbol, const char *titel, const char *unter) {   /* wie ui_eintrag_symbol, aber nicht anwählbar */
    zeile_zeichnen(56, titel, unter, NULL, 0, cur_y);
    int py = sy(cur_y);
    gfx_kreis(x0 + 20, py + 29, 20, TH->flaeche2);
    if (symbol >= 0) gfx_symbol(symbol, x0 + 10, py + 19, 20, TH->accentText);
    cur_y += 62;
}
void ui_person(int p, const char *titel, const char *unter, const char *rechts) {
    zeile_zeichnen(56, titel, unter, rechts, 0, cur_y);
    avatar(p, x0, sy(cur_y) + 9, 40);
    cur_y += 62;
}
int ui_kachel_knopf(int x, int y, int b, int h) { int f = element(x, y - ber_y0 + roll, b, h); return f && (taste & T_A); }
int ui_flaeche(int h) { int f = element(x0, cur_y, breite, h); return f && (taste & T_A); }

void ui_balken(int anteil, uint16_t farbe) {
    if (anteil < 0) anteil = 0;
    if (anteil > 1000) anteil = 1000;
    int y = sy(cur_y);
    gfx_runde_box(x0, y, breite, 10, 5, TH->flaeche2);
    if (anteil > 0) gfx_runde_box(x0, y, breite * anteil / 1000 < 10 ? 10 : breite * anteil / 1000, 10, 5, farbe);
    cur_y += 18;
}
void ui_avatare(const int *pr, int n) {
    int x = x0, y = cur_y;
    for (int i = 0; i < n; i++) {
        if (x + 40 > x0 + breite) { x = x0; y += 66; }
        avatar(pr[i], x + 4, sy(y) + 4, 40);
        text_mitte(x + 24, sy(y) + 48, tmp("%.7s", pname(pr[i])), F_KLEIN, TH->muted);
        x += 58;
    }
    cur_y = y + (n ? 70 : 0);
}
static int mitte_alt = -1;
void ui_mitte(int an) {
    if (an) { mitte_alt = x0; x0 = BILD_B / 2 - 150; }
    else if (mitte_alt >= 0) { x0 = mitte_alt; mitte_alt = -1; }
}
int ui_systemzeile(const char *s, const char *zeit) {
    const char *st[8]; int ln[8];
    int b = breite - 100, n = text_umbrechen(s, F_KLEIN, b - 24, st, ln, 8);
    int h = n * 17 + 26, x = x0 + 50, f = element(x, cur_y, b, h), y = sy(cur_y);
    if (f) ui_fokus_rahmen(x, y, b, h, 12);
    gfx_runde_box(x, y, b, h, 12, TH->flaeche);
    for (int i = 0; i < n; i++) text_n(x + 12, y + 5 + i * 17, st[i], ln[i], F_KLEIN, TH->text);
    text_rechts(x + b - 10, y + h - 18, zeit, F_KLEIN, TH->muted);
    cur_y += h + 6;
    return f && (taste & T_A);
}
int ui_blase(int profil, const char *name, const char *s, const char *zeit, int eigen, const char *extra, const char *antwort) {
    const char *st[40]; int ln[40];
    int maxb = breite * 3 / 4 - 24;
    int n = text_umbrechen(s && s[0] ? s : " ", F_NORMAL, maxb, st, ln, 40), bb = 0;
    for (int i = 0; i < n; i++) { int w = text_breite_n(st[i], ln[i], F_NORMAL); if (w > bb) bb = w; }
    if (!eigen) { int w = text_breite(name, F_FETT); if (w > bb) bb = w; }
    if (extra) { int w = text_breite(extra, F_KLEIN); if (w > bb) bb = w > maxb ? maxb : w; }
    if (antwort) { int w = text_breite(antwort, F_KLEIN) + 12; if (w > bb) bb = w > maxb ? maxb : w; }
    int zb = text_breite(zeit, F_KLEIN);
    bb += 28; if (bb < zb + 40) bb = zb + 40;
    int h = n * 21 + 30 + (eigen ? 0 : 20) + (extra ? 20 : 0) + (antwort ? 22 : 0);
    int bx = eigen ? x0 + breite - bb : x0 + 48;
    int f = element(bx, cur_y, bb, h), y = sy(cur_y);
    if (!eigen) avatar(profil, x0, y, 40);
    uint16_t hg = eigen ? TH->accentCont : TH->flaeche, vg = eigen ? TH->onAccentCont : TH->text;
    if (f) ui_fokus_rahmen(bx, y, bb, h, 16);
    gfx_runde_box(bx, y, bb, h, 16, hg);
    int ty = y + 8;
    if (!eigen) { text(bx + 14, ty, name, F_FETT, phell(profil) ? TH->accentText : gfx_mischen(pfarbe(profil), C_WEISS, 150)); ty += 20; }
    if (antwort) { gfx_rechteck(bx + 14, ty + 1, 3, 17, TH->accent); text_max(bx + 22, ty + 1, antwort, bb - 36, F_KLEIN, TH->muted); ty += 22; }
    for (int i = 0; i < n; i++) { text_n(bx + 14, ty, st[i], ln[i], F_NORMAL, vg); ty += 21; }
    if (extra) { text_max(bx + 14, ty + 1, extra, bb - 28, F_KLEIN, TH->accentText); ty += 20; }
    text_rechts(bx + bb - 12, y + h - 20, zeit, F_KLEIN, TH->muted);
    cur_y += h + 8;
    return f && (taste & T_A);
}

/* ---------- Bilder ---------- */
void avatar(int p, int x, int y, int k) {
    int bild = p >= 0 && p < db.profile.n ? db.profile.v[p].bild : 0;
    uint16_t hg = pfarbe(p);
    int r = k / 2;
    gfx_kreis(x + r, y + r, r, hg);
    if (bild >= 1 && bild <= 48) {
        int g = k >= 36 ? 1 : 0;
        int n = AVA_PX[g == 1 ? 0 : 1];
        int fx = x + r - n / 2, fy = y + r - n / 2;
        if (k >= 60) { /* groß: doppelt */
            const uint16_t *q = AVA_DATEN[0] + (size_t)(bild - 1) * 52 * 52;
            int m = 52 * k * 132 / 100 / 52;
            (void)m;
            for (int yy = 0; yy < 96; yy++) for (int xx = 0; xx < 96; xx++) {
                uint16_t c = q[(yy * 52 / 96) * 52 + xx * 52 / 96];
                int a = c & 15, px = x + r - 48 + xx, py = y + r - 48 + yy;
                if (!a || px < lw.clip_x0 || px >= lw.clip_x1 || py < lw.clip_y0 || py >= lw.clip_y1) continue;
                int rr = c >> 12, gg = (c >> 8) & 15, bb = (c >> 4) & 15;
                uint16_t f = (uint16_t)(((rr << 1 | rr >> 3) << 11) | ((gg << 1 | gg >> 3) << 6) | ((bb << 1 | bb >> 3) << 1) | 1);
                uint16_t *d = lw.px + py * lw.zeile + px;
                *d = a == 15 ? f : gfx_mischen(*d, f, a * 17);
            }
            return;
        }
        gfx_bild4444(fx, fy, n, n, AVA_DATEN[g == 1 ? 0 : 1] + (size_t)(bild - 1) * (size_t)(n * n));
    } else {
        const char *nm = pname(p);
        char kz[3] = { nm[0], nm[1] ? nm[1] : 0, 0 };
        if (kz[1] >= 'a' && kz[1] <= 'z') kz[1] = (char)(kz[1] - 32);
        text_mitte(x + r, y + r - text_hoehe(k >= 36 ? F_FETT : F_KLEIN) / 2, kz, k >= 36 ? F_FETT : F_KLEIN, phell(p) ? C_SCHWARZ : C_WEISS);
    }
}
/* Ring (0..1000 ab oben im Uhrzeigersinn), striche > 0: gestrichelt wie die Uhr der App */
void ui_ring(int cx, int cy, int r, int dicke, int anteil, uint16_t farbe, uint16_t hinten, int striche) {
    int r2 = r * r, i2 = (r - dicke) * (r - dicke);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int d = dx * dx + dy * dy;
            if (d > r2 || d < i2) continue;
            int x = dx, y = -dy, ax = abs(x), ay = abs(y), s = ax + ay, w;
            if (!s) continue;
            if (x >= 0 && y > 0) w = 1000 * ax / s;
            else if (x > 0 && y <= 0) w = 1000 + 1000 * ay / s;
            else if (x <= 0 && y < 0) w = 2000 + 1000 * ax / s;
            else w = 3000 + 1000 * ay / s;
            if (striche && ((w * striche / 4000) * 4000 / striche + 4000 / striche * 2 / 3 < w)) continue;
            int px = cx + dx, py = cy + dy;
            if (px >= lw.clip_x0 && px < lw.clip_x1 && py >= lw.clip_y0 && py < lw.clip_y1) lw.px[py * lw.zeile + px] = w < anteil * 4 ? farbe : hinten;
        }
}

/* ---------- Kopf und Reiterleiste ---------- */
static int kopf_knopf(int x, int y, int b, int h, int symbol, int pr) {
    int f = element_f(x, y, b, h, 1);
    if (f) gfx_runder_rahmen(x - 2, y - 2, b + 4, h + 4, (h + 4) / 2, 3, TH->onOben);
    if (symbol >= 0) gfx_symbol(symbol, x + (b - 28) / 2, y + (h - 28) / 2, 28, TH->onOben);
    if (pr >= 0) { gfx_kreis(x + b / 2, y + h / 2, 18, TH->onOben); avatar(pr, x + b / 2 - 16, y + h / 2 - 16, 32); }
    return f && (taste & T_A);
}
int kopf_haupt(const char *titel) {
    int ret = KOPF_NICHTS;
    gfx_rechteck(0, 0, BILD_B, KOPF_HAUPT, TH->oben);
    text_max(RAND_X - 6, 12, titel, 300, F_KOPF, TH->onOben);
    if (ich >= 0) {
        if (kopf_knopf(BILD_B - RAND_X - 44 * 3 - 8, 6, 44, 44, IC_TROPHY, -1)) ret = KOPF_RUHM;
        if (kopf_knopf(BILD_B - RAND_X - 44 * 2 - 4, 6, 44, 44, IC_MESSAGE_SQUARE, -1)) ret = KOPF_CHAT;
        if (kopf_knopf(BILD_B - RAND_X - 44, 6, 44, 44, -1, ich)) ret = KOPF_ICH;
    }
    /* Wer ist da? */
    int l = laufendes_treffen(), t = l >= 0 ? l : naechstes_treffen();
    int da = 0, kommt = 0, x = RAND_X - 6, lst[MAX_PROFILE], n = 0;
    if (t >= 0) {
        for (int p = 0; p < db.profile.n; p++) {
            if (!db.profile.v[p].aktiv) continue;
            if (l >= 0 && ist_anwesend(l, p)) { da++; lst[n++] = p; }
            else if (hat_komme(t, p)) { kommt++; if (l < 0) lst[n++] = p; }
        }
        int bx = x, by = 54;
        int b = (n > 8 ? 8 : n) * 18 + 14 + text_breite(l >= 0 ? tmp("%d da · %d kommt", da, kommt) : tmp("%d kommen", kommt), F_KLEIN) + 16;
        int f = element_f(bx - 4, by - 2, b, 30, 1);
        if (f) gfx_runder_rahmen(bx - 6, by - 4, b + 4, 34, 17, 3, TH->onOben);
        for (int i = 0; i < n && i < 8; i++) { gfx_kreis(x + 13, by + 13, 13, TH->oben); avatar(lst[i], x + 1, by + 1, 24); x += 18; }
        text(x + 14, by + 5, l >= 0 ? tmp("%d da · %d kommt", da, kommt) : tmp("%d kommen", kommt), F_KLEIN, TH->onOben);
        if (f && (taste & T_A)) ret = KOPF_WERDA;
    }
    return ret;
}
int kopf_seite(const char *titel, int extra) {
    int ret = KOPF_NICHTS;
    gfx_rechteck(0, 0, BILD_B, KOPF_SEITE, TH->oben);
    if (kopf_knopf(RAND_X - 12, 7, 44, 44, IC_ARROW_LEFT, -1)) ret = KOPF_ZURUECK;
    text_max(RAND_X + 42, 13, titel, BILD_B - RAND_X * 2 - 100, F_KOPF, TH->onOben);
    if (extra >= 0 && kopf_knopf(BILD_B - RAND_X - 44, 7, 44, 44, extra, -1)) ret = KOPF_EXTRA;
    return ret;
}
static const int REITER_IC[5] = { IC_HOUSE, IC_CALENDAR, IC_EURO, IC_MAP_PIN, IC_FORMEN };
int reiterleiste(int aktiv) {
    int ret = -1;
    gfx_rechteck(0, LEISTE_Y, BILD_B, LEISTE_H, TH->flaeche);
    gfx_linie_h(0, LEISTE_Y, BILD_B, TH->line);
    int w = (BILD_B - 2 * RAND_X) / 5;
    for (int i = 0; i < 5; i++) {
        int cx = RAND_X + i * w + w / 2, y = LEISTE_Y + 12;
        int f = element_f(cx - 36, y - 2, 72, 36, 1);
        if (i == aktiv) gfx_runde_box(cx - 34, y, 68, 32, 16, TH->accentCont);
        if (f) gfx_runder_rahmen(cx - 38, y - 4, 76, 40, 20, 3, TH->text);
        gfx_symbol(REITER_IC[i], cx - 14, y + 2, 28, i == aktiv ? TH->onAccentCont : TH->muted);
        if (f && (taste & T_A)) ret = i;
    }
    text(6, LEISTE_Y + 20, "L", F_KLEIN, TH->faint);
    text_rechts(BILD_B - 6, LEISTE_Y + 20, "R", F_KLEIN, TH->faint);
    return ret;
}

/* ---------- Dialoge ---------- */
enum { D_FRAGE = 1, D_AUSWAHL, D_TASTATUR, D_ZAHL, D_PERSONEN, D_UHR, D_DATUM };
static int dlg_id, dlg_wahl, dlg_roll, dlg_n, dlg_max, dlg_seite, dlg_feld, dlg_stellen, dlg_nachkomma;
static char dlg_titel[220], dlg_ja[64], dlg_nein[64], dlg_einheit[16];
static const char *const *dlg_punkte;
static char dlg_text[PUFFER];
static int32_t dlg_wert;
static uint32_t dlg_maske;
static int erg_typ, erg_id, erg_wert; static int32_t erg_zahl; static uint32_t erg_maske;
static char erg_text[PUFFER];

static void schliessen(int wert) { erg_typ = dlg_typ; erg_id = dlg_id; erg_wert = wert; dlg_typ = 0; }
static void oeffnen(int typ, int id, const char *titel) {
    dlg_typ = typ; dlg_id = id; dlg_wahl = 0; dlg_roll = 0; dlg_feld = 0; dlg_seite = 0;
    strncpy(dlg_titel, titel ? titel : "", sizeof dlg_titel - 1); dlg_titel[sizeof dlg_titel - 1] = 0;
    erg_typ = 0;
}
void frage(int id, const char *s, const char *ja, const char *nein) {
    oeffnen(D_FRAGE, id, s);
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
    dlg_max = max > AKTION_TEXT_MAX ? AKTION_TEXT_MAX : max;
}
const char *tastatur_ergebnis(int id) {
    if (erg_typ == D_TASTATUR && erg_id == id && erg_wert == 0) { erg_typ = 0; return erg_text; }
    return NULL;
}
int erg_tastatur_abgeschlossen(int id) {
    if (erg_typ == D_TASTATUR && erg_id == id && erg_wert != 0) { erg_typ = 0; return 1; }
    return 0;
}
void zahl_art(int id, const char *titel, int32_t wert, int stellen, int nachkomma, const char *einheit) {
    oeffnen(D_ZAHL, id, titel); dlg_wert = wert < 0 ? 0 : wert; dlg_stellen = stellen; dlg_nachkomma = nachkomma;
    strncpy(dlg_einheit, einheit ? einheit : "", sizeof dlg_einheit - 1); dlg_einheit[sizeof dlg_einheit - 1] = 0;
    dlg_feld = stellen - nachkomma - 1;
}
void zahl(int id, const char *titel, int32_t cent) { zahl_art(id, titel, cent, 7, 2, "€"); }
int zahl_ergebnis(int id, int32_t *w) { if (erg_typ == D_ZAHL && erg_id == id) { erg_typ = 0; *w = erg_zahl; return erg_wert == 0; } return 0; }
void personen(int id, const char *titel, uint32_t maske) { oeffnen(D_PERSONEN, id, titel); dlg_maske = maske; }
int personen_ergebnis(int id, uint32_t *m) { if (erg_typ == D_PERSONEN && erg_id == id) { erg_typ = 0; *m = erg_maske; return erg_wert == 0; } return 0; }
void uhr_dialog(int id, int32_t sek) { oeffnen(D_UHR, id, "Tag und Uhrzeit"); dlg_wert = sek; dlg_feld = 3; }
void datum_dialog(int id, const char *titel, int32_t sek) { oeffnen(D_DATUM, id, titel); dlg_wert = (sek / 86400) * 86400; dlg_feld = 0; }
int uhr_ergebnis(int id, int32_t *sek) { if ((erg_typ == D_UHR || erg_typ == D_DATUM) && erg_id == id) { erg_typ = 0; *sek = erg_zahl; return erg_wert == 0; } return 0; }
void meldung(const char *s) {
    strncpy(meldung_text, s, sizeof meldung_text - 1); meldung_text[sizeof meldung_text - 1] = 0;
    meldung_bis = plat_ms() + 2800;
}
int ui_meldung_aktiv(void) { return meldung_text[0] != 0; }

static const char *const TASTEN[2][4] = {
    { "1234567890ß-", "qwertzuiopü+", "asdfghjklöä#", "yxcvbnm,.!?'" },
    { "!\"§$%&/()=?_", "QWERTZUIOPÜ*", "ASDFGHJKLÖÄ'", "YXCVBNM;:@<>" } };
#define T_SPALTEN 12
static const char *const STEUER[5] = { "Groß", "Emojis", "Leerzeichen", "Löschen", "Fertig" };
static int taste_code(int seite, int zeile, int spalte) {
    if (seite == 2) {   /* Emojis aus der Liste */
        int i = zeile * T_SPALTEN + spalte;
        return i < EMOJI_N ? (int)EMOJI_CP[i + dlg_roll * T_SPALTEN < EMOJI_N ? i + dlg_roll * T_SPALTEN : i] : 0;
    }
    const char *z = TASTEN[seite][zeile];
    int k = 0, i = 0, c;
    while ((c = utf8_naechstes(z, &i)) != 0) { if (k == spalte) return c; k++; }
    return 0;
}
static void text_anhaengen(int c) {
    if (utf8_laenge(dlg_text) >= dlg_max) return;
    char s[5]; int n;
    if (c < 0x80) { s[0] = (char)c; n = 1; }
    else if (c < 0x800) { s[0] = (char)(0xC0 | (c >> 6)); s[1] = (char)(0x80 | (c & 63)); n = 2; }
    else if (c < 0x10000) { s[0] = (char)(0xE0 | (c >> 12)); s[1] = (char)(0x80 | ((c >> 6) & 63)); s[2] = (char)(0x80 | (c & 63)); n = 3; }
    else { s[0] = (char)(0xF0 | (c >> 18)); s[1] = (char)(0x80 | ((c >> 12) & 63)); s[2] = (char)(0x80 | ((c >> 6) & 63)); s[3] = (char)(0x80 | (c & 63)); n = 4; }
    s[n] = 0;
    if (strlen(dlg_text) + (size_t)n + 1 >= sizeof dlg_text) return;
    strcat(dlg_text, s);
}
static void text_loeschen(void) { int n = utf8_laenge(dlg_text); if (n > 0) dlg_text[utf8_bytes_fuer(dlg_text, n - 1)] = 0; }

static int32_t uhr_feld_aendern(int32_t sek, int feld, int d) {
    tag_t t = (tag_t)(sek / 86400); int rest = (int)(sek % 86400), h = rest / 3600, mi = rest / 60 % 60;
    int j, m, tg; tag_zu_datum(t, &j, &m, &tg);
    switch (feld) {
    case 0: t = (tag_t)(t + d); break;
    case 1: m += d; if (m > 12) { m = 1; j++; } if (m < 1) { m = 12; j--; } t = datum_zu_tag(j, m, tg > 28 ? 28 : tg); break;
    case 2: j += d; if (j < 2020) j = 2020; t = datum_zu_tag(j, m, tg > 28 ? 28 : tg); break;
    case 3: h = (h + d + 24) % 24; break;
    case 4: mi = (mi + d * (mi % 5 == 0 && d ? 5 : 1) + 60) % 60; break;
    }
    if (t < 0) t = 0;
    return (int32_t)t * 86400 + h * 3600 + mi * 60;
}
static int32_t zehner(int n) { int32_t z = 1; while (n-- > 0) z *= 10; return z; }

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
        int n = db.profile.n + 2;
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
        if (t & T_LINKS) dlg_feld = dlg_feld > 0 ? dlg_feld - 1 : 0;
        if (t & T_RECHTS) dlg_feld = dlg_feld < dlg_stellen - 1 ? dlg_feld + 1 : dlg_stellen - 1;
        int32_t s = zehner(dlg_stellen - 1 - dlg_feld), ziffer = dlg_wert / s % 10;
        if (t & T_HOCH) dlg_wert += ziffer == 9 ? -9 * s : s;
        if (t & T_RUNTER) dlg_wert += ziffer == 0 ? 9 * s : -s;
        if (t & (T_A | T_START)) { erg_zahl = dlg_wert; schliessen(0); }
        else if (t & T_B) schliessen(1);
        break;
    }
    case D_UHR: case D_DATUM: {
        int max = dlg_typ == D_UHR ? 4 : 2;
        if (t & T_LINKS) dlg_feld = dlg_feld > 0 ? dlg_feld - 1 : 0;
        if (t & T_RECHTS) dlg_feld = dlg_feld < max ? dlg_feld + 1 : max;
        if (t & T_HOCH) dlg_wert = uhr_feld_aendern(dlg_wert, dlg_feld, 1);
        if (t & T_RUNTER) dlg_wert = uhr_feld_aendern(dlg_wert, dlg_feld, -1);
        if (t & (T_A | T_START)) { erg_zahl = dlg_wert; schliessen(0); }
        else if (t & T_B) schliessen(1);
        break;
    }
    case D_TASTATUR: {
        int zeilen = 5;
        if (t & T_HOCH) { if (dlg_seite == 2 && dlg_feld == 0 && dlg_roll > 0) dlg_roll--; else dlg_feld = (dlg_feld + zeilen - 1) % zeilen; }
        if (t & T_RUNTER) { if (dlg_seite == 2 && dlg_feld == 3 && (dlg_roll + 4) * T_SPALTEN < EMOJI_N) dlg_roll++; else dlg_feld = (dlg_feld + 1) % zeilen; }
        int spalten = dlg_feld == 4 ? 5 : T_SPALTEN;
        if (dlg_feld == 4 && dlg_wahl > 4) dlg_wahl = 4;
        if (t & T_LINKS) dlg_wahl = (dlg_wahl + spalten - 1) % spalten;
        if (t & T_RECHTS) dlg_wahl = (dlg_wahl + 1) % spalten;
        if (t & T_Z) dlg_seite = dlg_seite == 1 ? 0 : 1;
        if (t & T_L) { dlg_seite = dlg_seite == 2 ? 0 : 2; dlg_roll = 0; }
        if (t & T_R) text_anhaengen(' ');
        if (t & T_B) { if (dlg_text[0]) text_loeschen(); else { schliessen(1); break; } }
        if (t & T_START) { strcpy(erg_text, dlg_text); schliessen(0); break; }
        if (t & T_A) {
            if (dlg_feld < 4) {
                int c = taste_code(dlg_seite, dlg_feld, dlg_wahl);
                if (dlg_seite == 2) { int i = (dlg_feld + dlg_roll) * T_SPALTEN + dlg_wahl; c = i < EMOJI_N ? (int)EMOJI_CP[i] : 0; }
                if (c) text_anhaengen(c);
                if (dlg_seite == 1) dlg_seite = 0;
            } else switch (dlg_wahl) {
                case 0: dlg_seite = dlg_seite == 1 ? 0 : 1; break;
                case 1: dlg_seite = dlg_seite == 2 ? 0 : 2; dlg_roll = 0; break;
                case 2: text_anhaengen(' '); break;
                case 3: text_loeschen(); break;
                case 4: strcpy(erg_text, dlg_text); schliessen(0); break;
            }
        }
        break;
    }
    }
}

/* Blatt von unten: abgerundet, Griff oben */
static int blatt(int h) {
    if (h > BILD_H - 30) h = BILD_H - 30;
    int y = BILD_H - h;
    gfx_runde_box(0, y, BILD_B, h + 30, 28, TH->flaeche);
    gfx_runde_box(BILD_B / 2 - 20, y + 10, 40, 4, 2, TH->edge);
    return y;
}
static void blatt_knopf(int x, int y, int b, const char *s, int f, int art) {
    uint16_t hg, vg; knopf_farben(art, &hg, &vg);
    if (f) ui_fokus_rahmen(x, y, b, 48, 24);
    gfx_runde_box(x, y, b, 48, 24, f ? gfx_mischen(hg, vg, 30) : hg);
    text_mitte(x + b / 2, y + (48 - text_hoehe(F_FETT)) / 2, s, F_FETT, vg);
}
static void listenzeile(int x, int y, int b, int f) {
    if (f) { ui_fokus_rahmen(x, y, b, 40, 20); gfx_runde_box(x, y, b, 40, 20, TH->accentCont); }
}

void dialoge_zeichnen(void) {
    gfx_clip_aus();
    if (dlg_typ) gfx_abdunkeln(0, 0, BILD_B, BILD_H, 120);
    switch (dlg_typ) {
    case D_FRAGE: {
        const char *st[12]; int ln[12];
        int n = text_umbrechen(dlg_titel, F_NORMAL, BILD_B - 2 * RAND_X - 20, st, ln, 12);
        int y = blatt(56 + n * 22 + 84);
        for (int i = 0; i < n; i++) text_n(RAND_X + 10, y + 32 + i * 22, st[i], ln[i], i == 0 ? F_FETT : F_NORMAL, TH->text);
        int by = y + 40 + n * 22 + 16, b = (BILD_B - 2 * RAND_X - 20) / 2;
        if (dlg_nein[0]) {
            blatt_knopf(RAND_X, by, b, dlg_nein, dlg_wahl == 1, KN_RAND);
            blatt_knopf(RAND_X + b + 20, by, b, dlg_ja, dlg_wahl == 0, KN_VOLL);
        } else blatt_knopf(RAND_X, by, BILD_B - 2 * RAND_X, dlg_ja, 1, KN_VOLL);
        break;
    }
    case D_AUSWAHL: {
        int sicht = dlg_n < 8 ? dlg_n : 8;
        if (dlg_wahl < dlg_roll) dlg_roll = dlg_wahl;
        if (dlg_wahl >= dlg_roll + sicht) dlg_roll = dlg_wahl - sicht + 1;
        int y = blatt(80 + sicht * 44);
        text_max(RAND_X + 10, y + 28, dlg_titel, BILD_B - 2 * RAND_X - 20, F_TITEL, TH->text);
        for (int i = 0; i < sicht; i++) {
            int k = dlg_roll + i, yy = y + 66 + i * 44;
            listenzeile(RAND_X, yy, BILD_B - 2 * RAND_X, k == dlg_wahl);
            text_max(RAND_X + 18, yy + 11, dlg_punkte[k], BILD_B - 2 * RAND_X - 36, k == dlg_wahl ? F_FETT : F_NORMAL, k == dlg_wahl ? TH->onAccentCont : TH->text);
        }
        if (dlg_roll > 0) gfx_symbol(IC_CHEVRON_UP, BILD_B - RAND_X - 20, y + 30, 20, TH->muted);
        if (dlg_roll + sicht < dlg_n) gfx_symbol(IC_CHEVRON_DOWN, BILD_B - RAND_X - 20, BILD_H - 24, 20, TH->muted);
        break;
    }
    case D_PERSONEN: {
        int n = db.profile.n + 2, sicht = n < 9 ? n : 9;
        if (dlg_wahl < dlg_roll) dlg_roll = dlg_wahl;
        if (dlg_wahl >= dlg_roll + sicht) dlg_roll = dlg_wahl - sicht + 1;
        int y = blatt(76 + sicht * 42);
        text_max(RAND_X + 10, y + 26, dlg_titel, BILD_B - 2 * RAND_X - 20, F_TITEL, TH->text);
        for (int i = 0; i < sicht; i++) {
            int k = dlg_roll + i, yy = y + 62 + i * 42;
            listenzeile(RAND_X, yy, BILD_B - 2 * RAND_X, k == dlg_wahl);
            if (k == 0) text(RAND_X + 18, yy + 11, "Alle / keiner", F_FETT, TH->text);
            else if (k == n - 1) { gfx_symbol(IC_CHECK, RAND_X + 16, yy + 10, 20, TH->ok); text(RAND_X + 44, yy + 11, "Fertig", F_FETT, TH->ok); }
            else {
                int p = k - 1, an = (dlg_maske & (1u << p)) != 0;
                gfx_runde_box(RAND_X + 14, yy + 9, 22, 22, 6, an ? TH->accent : TH->flaeche2);
                if (an) gfx_symbol(IC_CHECK, RAND_X + 15, yy + 10, 20, TH->onAccent);
                avatar(p, RAND_X + 48, yy + 6, 28);
                text(RAND_X + 86, yy + 11, pname(p), F_NORMAL, db.profile.v[p].aktiv ? TH->text : TH->faint);
            }
        }
        break;
    }
    case D_ZAHL: {
        int y = blatt(220);
        text_max(RAND_X + 10, y + 28, dlg_titel, BILD_B - 2 * RAND_X - 20, F_TITEL, TH->text);
        char z[16]; snprintf(z, sizeof z, "%0*ld", dlg_stellen, (long)dlg_wert);
        int zw = 30, gesamt = dlg_stellen * zw + (dlg_nachkomma ? 14 : 0) + text_breite(dlg_einheit, F_GROSS) + 10;
        int zx = BILD_B / 2 - gesamt / 2, zy = y + 92;
        for (int i = 0; i < dlg_stellen; i++) {
            int px = zx + i * zw + (dlg_nachkomma && i >= dlg_stellen - dlg_nachkomma ? 14 : 0);
            if (i == dlg_feld) {
                gfx_runde_box(px - 3, zy - 6, zw, 52, 8, TH->accentCont);
                gfx_symbol(IC_CHEVRON_UP, px + 2, zy - 32, 20, TH->muted); gfx_symbol(IC_CHEVRON_DOWN, px + 2, zy + 50, 20, TH->muted);
            }
            char c[2] = { z[i], 0 };
            int fuehrend = i < dlg_stellen - dlg_nachkomma - 1 && dlg_wert < zehner(dlg_stellen - i - 1);
            text_mitte(px + zw / 2 - 3, zy, c, F_GROSS, fuehrend ? TH->faint : TH->text);
        }
        if (dlg_nachkomma) text(zx + (dlg_stellen - dlg_nachkomma) * zw - 2, zy, ",", F_GROSS, TH->text);
        text(zx + dlg_stellen * zw + (dlg_nachkomma ? 14 : 0) + 6, zy, dlg_einheit, F_GROSS, TH->text);
        text_mitte(BILD_B / 2, BILD_H - 28, "Steuerkreuz: Ziffer   A: fertig   B: abbrechen", F_KLEIN, TH->muted);
        break;
    }
    case D_UHR: case D_DATUM: {
        int y = blatt(220);
        text(RAND_X + 10, y + 28, dlg_titel, F_TITEL, TH->text);
        tag_t t = (tag_t)(dlg_wert / 86400); int rest = (int)(dlg_wert % 86400);
        int j, m, tg; tag_zu_datum(t, &j, &m, &tg);
        const char *teile[5] = { tmp("%02d", tg), tmp("%02d", m), tmp("%04d", j), tmp("%02d", rest / 3600), tmp("%02d", rest / 60 % 60) };
        const char *trenn[5] = { ".", ".", dlg_typ == D_UHR ? "   " : "", ":", "" };
        int n = dlg_typ == D_UHR ? 5 : 3;
        int ges = 0;
        for (int i = 0; i < n; i++) ges += text_breite(teile[i], F_GROSS) + text_breite(trenn[i], F_GROSS);
        int px = BILD_B / 2 - ges / 2, zy = y + 96;
        text_mitte(BILD_B / 2, y + 56, WOCHENTAG_LANG[wochentag(t)], F_NORMAL, TH->muted);
        for (int i = 0; i < n; i++) {
            int w = text_breite(teile[i], F_GROSS);
            if (i == dlg_feld) {
                gfx_runde_box(px - 5, zy - 4, w + 10, 46, 8, TH->accentCont);
                gfx_symbol(IC_CHEVRON_UP, px + w / 2 - 10, zy - 30, 20, TH->muted); gfx_symbol(IC_CHEVRON_DOWN, px + w / 2 - 10, zy + 44, 20, TH->muted);
            }
            px += text(px, zy, teile[i], F_GROSS, TH->text);
            px += text(px, zy, trenn[i], F_GROSS, TH->text);
        }
        text_mitte(BILD_B / 2, BILD_H - 28, "Steuerkreuz: stellen   A: übernehmen   B: abbrechen", F_KLEIN, TH->muted);
        break;
    }
    case D_TASTATUR: {
        int y = blatt(380);
        text_max(RAND_X + 10, y + 24, dlg_titel, BILD_B - 2 * RAND_X - 100, F_FETT, TH->text);
        text_rechts(BILD_B - RAND_X - 6, y + 26, tmp("%d/%d", utf8_laenge(dlg_text), dlg_max), F_KLEIN, TH->muted);
        gfx_runde_box(RAND_X, y + 48, BILD_B - 2 * RAND_X, 70, 20, TH->flaeche2);
        const char *st[32]; int ln[32];
        char mit[PUFFER + 4];
        snprintf(mit, sizeof mit, "%s|", dlg_text);
        int n = text_umbrechen(mit, F_NORMAL, BILD_B - 2 * RAND_X - 36, st, ln, 32), ab = n > 3 ? n - 3 : 0;
        for (int i = ab; i < n; i++) text_n(RAND_X + 18, y + 54 + (i - ab) * 20, st[i], ln[i], F_NORMAL, TH->text);
        int kx = RAND_X, ky = y + 132, kb = (BILD_B - 2 * RAND_X) / T_SPALTEN;
        for (int z = 0; z < 4; z++)
            for (int s = 0; s < T_SPALTEN; s++) {
                int c, px = kx + s * kb, py = ky + z * 44, f = dlg_feld == z && dlg_wahl == s;
                if (dlg_seite == 2) { int i = (z + dlg_roll) * T_SPALTEN + s; c = i < EMOJI_N ? (int)EMOJI_CP[i] : 0; }
                else c = taste_code(dlg_seite, z, s);
                if (f) ui_fokus_rahmen(px + 1, py, kb - 6, 38, 10);
                gfx_runde_box(px + 1, py, kb - 6, 38, 10, f ? TH->accentCont : TH->flaeche2);
                if (dlg_seite == 2) { int e = emoji_index((uint32_t)c); if (e >= 0) gfx_emoji(e, px + (kb - 6) / 2 - 10, py + 8, 22); }
                else if (c) { char u[5] = {0}; int k = 0; if (c < 0x80) u[k++] = (char)c; else { u[k++] = (char)(0xC0 | (c >> 6)); u[k++] = (char)(0x80 | (c & 63)); } text_mitte(px + (kb - 6) / 2 + 1, py + 10, u, F_FETT, TH->text); }
            }
        int sy2 = ky + 4 * 44 + 2, sb[5] = { 2, 2, 4, 2, 2 }, sx = kx;
        for (int i = 0; i < 5; i++) {
            int w = sb[i] * kb, f = dlg_feld == 4 && dlg_wahl == i;
            int an = (i == 0 && dlg_seite == 1) || (i == 1 && dlg_seite == 2);
            uint16_t hg = i == 4 ? TH->accent : an ? TH->accentCont : TH->flaeche2, vg = i == 4 ? TH->onAccent : an ? TH->onAccentCont : TH->text;
            if (f) ui_fokus_rahmen(sx + 1, sy2, w - 6, 38, 19);
            gfx_runde_box(sx + 1, sy2, w - 6, 38, 19, f ? gfx_mischen(hg, vg, 40) : hg);
            text_mitte(sx + (w - 6) / 2, sy2 + 10, STEUER[i], F_FETT, vg);
            sx += w;
        }
        break;
    }
    }
    if (meldung_text[0] && plat_ms() < meldung_bis) {
        const char *st[6]; int ln[6];
        int n = text_umbrechen(meldung_text, F_NORMAL, BILD_B - 2 * RAND_X - 40, st, ln, 6);
        int h = 20 + n * 21, y = LEISTE_Y - h - 12;
        gfx_runde_box(RAND_X, y, BILD_B - 2 * RAND_X, h, 10, TH->text);
        for (int i = 0; i < n; i++) text_n(RAND_X + 20, y + 10 + i * 21, st[i], ln[i], F_NORMAL, TH->bg);
    } else meldung_text[0] = 0;
}
