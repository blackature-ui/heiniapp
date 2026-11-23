/* ui.h – Bedienung am Fernseher im Aussehen der Heini-App (Material You): Sofort-Modus mit Fokus fürs Steuerkreuz */
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "plat.h"
#include "gfx.h"

#define RAND_X 28
#define INHALT_B (BILD_B - 2 * RAND_X)
#define KOPF_HAUPT 88       /* Kopf mit „Wer ist da?“ */
#define KOPF_SEITE 58
#define LEISTE_H 56         /* Reiterleiste unten */
#define LEISTE_Y (BILD_H - LEISTE_H)

enum { KA_NORMAL = 0, KA_AKZENT, KA_OK, KA_ERR, KA_WARN, KA_GOLD, KA_ALARM, KA_FLAECHE2 };   /* Kartenarten */
enum { KN_TONAL = 0, KN_VOLL, KN_RAND, KN_GEFAHR, KN_OK, KN_ERR, KN_HELL, KN_KLEIN };          /* Knopfarten */
enum { KOPF_NICHTS = 0, KOPF_RUHM, KOPF_CHAT, KOPF_ICH, KOPF_WERDA, KOPF_ZURUECK, KOPF_EXTRA };

void ui_bild_start(const Eingabe *e);
int ui_bild_ende(void);
void ui_seite_neu(void);
uint32_t ui_taste(void);
int ui_dialog_offen(void);

void ui_bereich(int y0, int y1);
void ui_bereich_ende(void);
int ui_y(void);
int ui_x(void);
int ui_breite(void);
void ui_abstand(int px);
void ui_weiter(int dy);
void ui_roll_auf(int inhalt_y);    /* einmal dorthin rollen (bis zur nächsten Taste) */

/* Inhalte */
void ui_ueberschrift(const char *s, const char *rechts);      /* Abschnitt mit Balken links */
void ui_karte(const char *titel, int symbol);                 /* Karte mit Farbband (titel NULL: ohne Band) */
void ui_karte_art(int art, const char *titel, int symbol);
void ui_karte_ende(void);
void ui_text(const char *s, int f, uint16_t farbe);
void ui_zeile(const char *links, const char *rechts, uint16_t farbe);
void ui_symbolzeile(int symbol, const char *s, uint16_t farbe);
int ui_knopf(const char *s);
int ui_knopf_art(const char *s, int art, int symbol);
int ui_knopf_neben(const char *s);
int ui_knopf_neben_art(const char *s, int art, int symbol);
int ui_knopf_breit(const char *s, int art, int symbol);
int ui_segment(const char *const *namen, int n, int aktiv);
int ui_eintrag(int profil, const char *titel, const char *unter, const char *rechts);
int ui_eintrag_symbol(int symbol, const char *emoji, const char *titel, const char *unter, const char *rechts);
void ui_person(int profil, const char *titel, const char *unter, const char *rechts);
int ui_kachel_knopf(int x, int y, int b, int h);
int ui_flaeche(int h);
int ui_fokus_hier(void);
int ui_fokussiert_letztes(void);
void ui_fokus_letztes(void);
void ui_balken(int anteil_1000, uint16_t farbe);
void ui_avatare(const int *profile, int n);
void ui_mitte(int an);
int ui_systemzeile(const char *text, const char *zeit);
int ui_blase(int profil, const char *name, const char *text, const char *zeit, int eigen, const char *extra, const char *antwort);
void ui_fokus_rahmen(int x, int y, int b, int h, int r);

void avatar(int profil, int x, int y, int kreis /* 40, 28 oder 72 */);
void ui_ring(int cx, int cy, int r, int dicke, int anteil_1000, uint16_t farbe, uint16_t hinten, int striche);

int kopf_haupt(const char *titel);
int kopf_seite(const char *titel, int extra_symbol);
int reiterleiste(int aktiv);

void frage(int id, const char *text, const char *ja, const char *nein);
int frage_ergebnis(int id);
void auswahl(int id, const char *titel, const char *const *punkte, int n, int start);
int auswahl_ergebnis(int id);
void tastatur(int id, const char *titel, const char *start, int max_zeichen);
const char *tastatur_ergebnis(int id);
int erg_tastatur_abgeschlossen(int id);
void zahl(int id, const char *titel, int32_t wert);
void zahl_art(int id, const char *titel, int32_t wert, int stellen, int nachkomma, const char *einheit);
int zahl_ergebnis(int id, int32_t *wert);
void personen(int id, const char *titel, uint32_t maske);
int personen_ergebnis(int id, uint32_t *maske);
void uhr_dialog(int id, int32_t sek);
void datum_dialog(int id, const char *titel, int32_t sek);
int uhr_ergebnis(int id, int32_t *sek);
void meldung(const char *text);
int ui_meldung_aktiv(void);
void dialoge_zeichnen(void);

#endif
