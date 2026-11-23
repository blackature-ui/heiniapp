/* logik.h – Fachlogik der Heini-App, portiert aus public/logik.js und den Abläufen in server.js.
 * Alle Funktionen arbeiten auf der Datenbank db und der Uhr jetzt. */
#ifndef LOGIK_H
#define LOGIK_H
#include "model.h"

#define BAD_MAX 60            /* Minuten: so lange läuft eine „Rein“-Uhr höchstens */
#define NOCH_DRIN 30          /* „Bist du noch drin?“ nach 30 Minuten */
#define NACH_TAGE 14

enum { T_KOMMEND = 0, T_LAUFEND, T_UEBERFAELLIG, T_VORBEI, T_WEG };

/* ---------- Profile ---------- */
const char *pname(int p);                 /* Name oder „Jemand“ */
uint16_t pfarbe(int p);
int phell(int p);                         /* helle Profilfarbe → dunkle Schrift */
int partner_von(int p);
int wohnt_mit(int p);
int raum_besitzer(int p);
int profil_aktiv(int p);

/* ---------- Treffen ---------- */
int treffen_status(int t);
int laufendes_treffen(void);
int naechstes_treffen(void);              /* nicht beendet, beginnt heute oder später */
int kasse_treffen(void);                  /* welches Treffen die Kasse zeigt */
int nach_treffen(int p);                  /* zuletzt beendetes mit p, 14 Tage */
int ist_anwesend(int t, int p);
int anwesenheit_von(int t, int p);        /* Index oder -1 */
int zusage_von(int t, int p);             /* ZUSAGE_* oder -1 */
int hat_komme(int t, int p);              /* Gastgeber oder „Komme“ */
int ist_gastgeber(int t, int p);
int treffen_gastgeber(int t, int out[2]); /* Anzahl */
int raum_im_treffen(int r, int t);
int treffen_teilnehmer(int t, int *out);  /* Gastgeber, Komme, Angekommene */
const char *treffen_adresse(int t);
const char *treffen_titel(int t);
int treffen_naechte(int t);               /* Anzahl Nächte (von bis bis-1) */

/* ---------- Bad ---------- */
zeit_t buchung_ende(int b);               /* freigegeben, sonst offen bis jetzt (höchstens 60 Min) bzw. geplant */
int uhr_laeuft(int b);
int gehoert(int b, int p);                /* Besitzer oder Partner, solange drin */
int meine_uhr(int p);                     /* laufende Buchung der Person, sonst -1 */
int drin_in(int raum, int t);             /* laufende Buchung im Bad */
int schlange(int raum, int t, int *out, int max);
int dran_in(int raum, int t);             /* Warte-Index, wenn das Bad frei ist und jemand ansteht */
int wartet_in(int p, int t);              /* Raum, in dem p ansteht, sonst -1 */
int warte_platz(int p, int raum, int t);  /* Warte-Index */
int baeder(int t, int *out, int max);     /* Bäder des Treffens nach Reihenfolge */
int offener_alarm(int t);
void uhren_aufraeumen(zeit_t bis);        /* offene Uhren enden nach 60 Minuten von selbst */

/* ---------- Schlafen, Ausleihen ---------- */
int liegen_auf(int t, int platz, tag_t nacht, int out[2]);
int schlaf_von(int t, int p, tag_t nacht); /* Schlaf-Index */
int ausgeliehen(int t, int u);
int ausleihe_von(int t, int u, int p);

/* ---------- Kasse ---------- */
typedef struct { int von, an; int32_t betrag; } DirektPaar;
int32_t kassen_stand(int t, int32_t stand[MAX_PROFILE], uint32_t *beteiligt);   /* Summe */
int direkt_paare(int t, DirektPaar *out, int max);
int32_t kassen_konto(int t, int p, int32_t *bezahlt, int32_t *anteil);
int ausgabe_teile(int a, int8_t *ids, int32_t *cent, int max);   /* wer zahlt wie viel */
int32_t fahrtkosten(int km, int hin_zurueck, int verbrauch, int spritpreis, int verschleiss);
int fahrt_insassen(int f, int *out, int max);

/* ---------- Chat ---------- */
int umfrage_von_nachricht(int n);
int umfrage_zu(int u);
int umfrage_zaehlen(int u, int anzahl[UMFRAGE_MAX], int *leute);
int stimme_von(int u, int p);
int reaktionen_zaehlen(int n, int *emoji_anzahl /* EMOJIS_N */);

/* ---------- Sprüche ---------- */
const char *weisheit_des_tages(tag_t t);
const char *spruch_ueberzogen(int b);

/* ---------- Ruhmeshalle ---------- */
typedef struct { const char *titel, *erklaerung, *emoji; int profil; int32_t wert; char text[48]; } Rekord;
int rekorde(int t, Rekord *out, int max);  /* t = -1: ewige Bestenliste */
int ruhm_treffen(void);
void erfolge_pruefen(void);                /* neue Trophäen durch Bad, Alarm, Chat … vergeben */
int erfolg_hat(int p, int code);
int erfolg_index(const char *code);

/* ---------- Frühere Treffen ---------- */
typedef struct { int name; int n; int tage; } HeiniZahl;
int frueher_statistik(HeiniZahl *personen, int *n_personen, HeiniZahl *gastgeber, int *n_gast, int *tage_gesamt);

/* Hilfen */
int profil_nach_name(const char *name);
int heini_name_index(int p);              /* Profil → Index in HEINI_NAMEN */

#endif
