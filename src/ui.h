/* ui.h – Bedienung am Fernseher: Sofort-Modus-Oberfläche mit Fokus, Karten, Knöpfen und Dialogen.
 * Jede Seite zeichnet sich in jedem Bild neu; ui merkt sich die Lage der Knöpfe und bewegt den Fokus mit dem Steuerkreuz. */
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "plat.h"

#define RAND_X 36            /* Abstand für den Bildrand am Fernseher */
#define INHALT_B (BILD_B - 2 * RAND_X)
#define KOPF_H 92
#define FUSS_Y 446

void ui_bild_start(const Eingabe *e);
int ui_bild_ende(void);                  /* 1 = noch einmal zeichnen (Fokus musste ins Bild rollen) */
void ui_seite_neu(void);                 /* Fokus und Rollen zurücksetzen (neue Seite) */
uint32_t ui_taste(void);                 /* in diesem Bild gedrückt, wenn kein Dialog offen ist */
int ui_dialog_offen(void);

/* Rollbereich für den Inhalt */
void ui_bereich(int y0, int y1);
void ui_bereich_ende(void);
int ui_y(void);                          /* aktuelle Zeile (Bildschirm) */
void ui_abstand(int px);

/* Inhalte */
void ui_ueberschrift(const char *s);
void ui_karte(const char *titel, uint16_t band);   /* Karte mit Farbband; endet mit ui_karte_ende */
void ui_karte_ende(void);
void ui_text(const char *s, uint16_t farbe);        /* umbrochen */
void ui_text_gross(const char *s, uint16_t farbe, int gross);
void ui_zeile(const char *links, const char *rechts, uint16_t farbe);
int ui_knopf(const char *s);                        /* 1 = mit A gedrückt */
int ui_knopf_farbe(const char *s, uint16_t farbe, uint16_t schrift);
int ui_knopf_neben(const char *s);                  /* weiterer Knopf in derselben Zeile */
int ui_knopf_neben_farbe(const char *s, uint16_t farbe, uint16_t schrift);
int ui_eintrag(int profil, const char *titel, const char *unter, const char *rechts);   /* fokussierbare Zeile mit Bild */
void ui_person(int profil, const char *titel, const char *unter, const char *rechts);   /* nur Anzeige */
int ui_fokus_hier(void);                             /* hat das nächste Element den Fokus? */
int ui_fokussiert_letztes(void);                     /* hatte das zuletzt gezeichnete Element den Fokus? */
void ui_balken(int anteil_1000, uint16_t farbe);
void ui_avatare(const int *profile, int n);

/* Zeichnen */
void avatar(int profil, int x, int y, int groesse /* 24 oder 48 */);
void ui_ring(int cx, int cy, int r, int dicke, int anteil_1000, uint16_t farbe, uint16_t hinten);

/* Dialoge (Ergebnis kommt im nächsten Bild über *_ergebnis) */
void frage(int id, const char *text, const char *ja, const char *nein);
int frage_ergebnis(int id);              /* -1 offen/keins, 0 = ja, 1 = nein */
void auswahl(int id, const char *titel, const char *const *punkte, int n, int start);
int auswahl_ergebnis(int id);            /* -1 keins, sonst Index; -2 abgebrochen */
void tastatur(int id, const char *titel, const char *start, int max_zeichen);
const char *tastatur_ergebnis(int id);   /* NULL = keins */
void zahl(int id, const char *titel, int32_t cent);
int zahl_ergebnis(int id, int32_t *cent);
void personen(int id, const char *titel, uint32_t maske);
int personen_ergebnis(int id, uint32_t *maske);
void uhr_dialog(int id, int32_t sek);
int uhr_ergebnis(int id, int32_t *sek);
void meldung(const char *text);          /* kurze Meldung unten */
void dialoge_zeichnen(void);
int erg_tastatur_abgeschlossen(int id);
int ui_meldung_aktiv(void);
void ui_kachel(int x, int y, int b, int h);
void ui_weiter(int dy);
void ui_fokus_letztes(void);
void ui_mitte(int an);
int ui_systemzeile(const char *text, const char *zeit);
int ui_blase(int profil, const char *name, const char *text, const char *zeit, int eigen, const char *extra, const char *antwort);

/* Kopf, Reiter, Fuß */
void kopf(const char *titel, int mit_reitern, int reiter);
void fuss(const char *hinweis);

#endif
