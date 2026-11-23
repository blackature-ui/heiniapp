/* model.h – Datenmodell der Heini-App 64.
 *
 * Entspricht den Tabellen der Web-App (Masterdokument B1). Verweise auf andere Datensätze sind
 * Indizes in die jeweilige Tabelle (-1 = keiner). Zeiten sind Sekunden seit 01.01.2020 00:00
 * Ortszeit (Europe/Berlin, Wanduhr), Tage sind Tagesnummern seit 01.01.2020 (0 = kein Tag).
 * Texte sind Latin-1 mit den Symbolen aus text.h.
 *
 * Grunddaten kommen aus dem JSON-Export (src/gen/data.c, schreibgeschützt im ROM). Alles, was am
 * Fernseher passiert, wird als Aktion ins Journal (save.c) geschrieben und beim Start erneut
 * ausgeführt.
 */
#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROFILE 32
#define KEIN (-1)

typedef int32_t zeit_t;   /* Sekunden seit 2020-01-01 00:00 Ortszeit, 0 = keine Zeit */
typedef int16_t tag_t;    /* Tage seit 2020-01-01, 0 = kein Tag */

/* ---------- dynamische Tabellen ---------- */
#define TABELLE(T) struct { T *v; int n, cap; }
void *tabelle_platz(void *v, int *cap, int n, size_t gross);
#define TAB_NEU(t) ( (t).v = tabelle_platz((t).v, &(t).cap, (t).n + 1, sizeof(*(t).v)), \
                     &(t).v[(t).n++] )

/* ---------- Datensätze ---------- */
typedef struct {
    const char *name, *status, *wohnort, *strasse, *plz, *auto_name, *paypal, *park_hinweis, *startort;
    const char *unvertraeglich, *mag_nicht;   /* mit " · " getrennte Liste */
    uint8_t farbe;          /* Index in FARBEN */
    uint8_t bild;           /* 1..48, 0 = keins */
    uint8_t gastgeber, aktiv, heini, auto_, elektro, auto_plaetze, kein_alkohol, parkplaetze, auto_einchecken;
    int8_t wunsch_temp;     /* 15..24, 0 = egal */
    int16_t verbrauch;      /* Zehntel-Liter (bzw. Zehntel-kWh) je 100 km */
} Profil;

enum { RAUM_BAD = 0, RAUM_SCHLAF = 1 };
typedef struct { int8_t profil; int16_t treffen; const char *name, *info; uint8_t art; int8_t reihenfolge; uint8_t weg; } Raum;
typedef struct { int16_t raum; const char *name; uint8_t fuer_zwei; uint8_t weg; } Platz;
typedef struct { int8_t profil; const char *name; uint8_t zaehlbar, anzahl; uint8_t weg; } Utensil;

typedef struct { int8_t profil; int16_t art; /* Index in KONSOLEN, -1 = andere */ const char *name; int16_t pos; uint8_t weg; } Konsole;
enum { DING_SPIEL = 0, DING_ZUBEHOER = 1 };
typedef struct { int16_t konsole; uint8_t art; const char *name; uint8_t anzahl; uint8_t weg; } KonsolenDing;
typedef struct { int8_t profil, von; int16_t jahr; const char *geschenk; uint8_t weg; } Wichtel;

typedef struct {
    const char *titel, *adresse, *info;
    int8_t gastgeber, gastgeber2, ersteller;
    tag_t von, bis;
    zeit_t beendet_um, kasse_zu_um;
    uint8_t ausserhalb, geloescht;
} Treffen;

enum { ZUSAGE_KOMME = 0, ZUSAGE_VIELLEICHT = 1, ZUSAGE_NEIN = 2 };
typedef struct { int16_t treffen; int8_t profil; uint8_t status; } Zusage;
typedef struct { int16_t treffen; int8_t profil; zeit_t angekommen, abgereist; } Anwesenheit;

/* „Rein“-Besuch im Bad. start = Datum + Beginn, dauer = geplante Minuten, fertig = Freigabe (0 = läuft) */
typedef struct {
    int16_t treffen, raum; int8_t profil, partner;
    zeit_t start, fertig; int16_t dauer; uint8_t geloescht;
    uint8_t offen, lang;          /* offen = „Rein“-Uhr ohne festes Ende; lang = „Dauert länger“ */
    zeit_t partner_seit, partner_bis;
} Buchung;
typedef struct { int16_t raum, treffen; int8_t profil, partner; zeit_t seit; uint8_t geloescht; } Warte;
typedef struct { int16_t treffen, raum; int8_t profil, erledigt_von; zeit_t ausgeloest, erledigt_um; uint8_t geloescht; } Alarm;
typedef struct { int16_t treffen, platz; int8_t profil; tag_t nacht; uint8_t geloescht; } Schlaf;
typedef struct { int16_t treffen, utensil; int8_t profil; uint8_t anzahl; } Ausleihe;

enum { MEDIUM_KEIN = 0, MEDIUM_FOTO, MEDIUM_GIF, MEDIUM_AUDIO };
enum { NACHRICHT_PERSON = 0, NACHRICHT_SYSTEM = 1, NACHRICHT_UMFRAGE = 2 };
typedef struct {
    int8_t profil; uint8_t art, angeheftet, medium, medien_anzahl, geloescht;
    int16_t antwort_auf;
    zeit_t zeit;
    const char *text;
} Nachricht;
#define UMFRAGE_MAX 6
typedef struct { int16_t nachricht; uint8_t n, mehrfach; zeit_t beendet_um, endet_um; const char *antworten[UMFRAGE_MAX]; } Umfrage;
typedef struct { int16_t umfrage; int8_t profil; uint8_t wahl; /* Bitmaske */ } Stimme;
typedef struct { int16_t nachricht; int8_t profil; uint8_t emoji; /* Index in EMOJIS */ } Reaktion;

typedef struct { int8_t profil; int16_t code; int16_t treffen; zeit_t erreicht; uint8_t neu; } Erfolg;
enum { PAAR_OFFEN = 0, PAAR_BESTAETIGT = 1 };
typedef struct { int8_t von, zu, wohnen_von; uint8_t status; uint8_t weg; int8_t wohnen_durch; zeit_t wohnen_um; } Paar;

typedef struct {
    int16_t treffen, trip; int8_t profil, eingetragen_von;
    int32_t betrag;      /* Cent */
    uint8_t art;         /* Index in AUSGABE_ARTEN */
    uint8_t alkohol, geloescht;
    uint32_t fuer;       /* Bitmaske der Profile */
    uint8_t fuer_n; int8_t fuer_reihe[MAX_PROFILE];   /* Reihenfolge wie im Export (Rest-Cent gehen an die ersten) */
    int32_t liefer;      /* Lieferkosten bei Essensbestellung */
    int16_t km; int16_t verbrauch; int16_t spritpreis; int16_t verschleiss; uint8_t hin_zurueck;
    const char *titel;
} Ausgabe;
typedef struct { int16_t ausgabe; int8_t profil; int32_t cent; const char *was; } Posten;
enum { ZAHLUNG_OFFEN = 0, ZAHLUNG_GEMELDET = 1, ZAHLUNG_BESTAETIGT = 2 };
typedef struct { int16_t treffen; int8_t von, an; int32_t betrag; uint8_t status; zeit_t gemeldet_um, bestaetigt_um; } Zahlung;

typedef struct { int16_t nr, tage; int32_t monat; /* JJJJMM */ const char *name; int8_t gastgeber, gastgeber2; /* Heini-Namen */ uint32_t teilnehmer; /* Bitmaske HEINI_NAMEN */ } Frueher;

enum { MED_YOUTUBE = 0, MED_SPOTIFY, MED_LINK, MED_IDEE, MED_BUCKET };
typedef struct { uint8_t art; const char *symbol; uint8_t geloescht; int8_t profil, gastgeber; int16_t pos; zeit_t erledigt_um; const char *titel, *adresse; } Medium;
typedef struct { int16_t medium; int8_t profil; int8_t wert; } Daumen;
/* alte Aktivitäten eines Treffens (vor 1.60.0), nur Anzeige bei beendeten Treffen */
typedef struct { int16_t treffen, medium; int8_t profil; const char *titel; uint8_t erledigt, stimmen; } Aktivitaet;
typedef struct { int16_t treffen; int8_t profil; tag_t datum; int16_t uhrzeit; /* Minuten, -1 = keine */ const char *name, *notiz; uint8_t geloescht; } Agenda;
typedef struct { int16_t treffen; int8_t profil; uint8_t angepinnt, geloescht; zeit_t zeit; const char *text; } Kommentar;
typedef struct { int16_t treffen; int8_t profil; tag_t datum; int16_t uhrzeit; const char *titel, *wann; uint8_t geloescht; } Trip;
enum { RICHTUNG_HIN = 0, RICHTUNG_RUECK = 1 };
typedef struct { int16_t treffen, trip; int8_t profil; uint8_t plaetze, richtung, geloescht; } Fahrt;
typedef struct { int16_t fahrt; int8_t profil; uint8_t geloescht; } Mitfahrer;

typedef struct { int16_t treffen, raum; tag_t nacht; int8_t temp; } Temperatur;
typedef struct { int8_t profil; uint8_t typ; zeit_t zeit; const char *text; } Verlauf;

/* ---------- Konstanten aus rules.js / logik.js (gen/data.c) ---------- */
typedef struct { const char *schluessel, *name; uint16_t farbe; uint8_t hell; } Farbe;     /* hell = dunkle Schrift */
typedef struct { const char *schluessel, *name, *kurz; uint16_t farbe; } KonsolenArt;
typedef struct { const char *code, *name, *hinweis, *emoji; uint8_t art; /* 'T','U','H' */ uint8_t geheim; } ErfolgArt;
typedef struct { const char *schluessel, *text, *satz, *emoji; int16_t icon; } AusgabeArt;

extern const Farbe FARBEN[]; extern const int FARBEN_N;
extern const KonsolenArt KONSOLEN[]; extern const int KONSOLEN_N;
extern const ErfolgArt ERFOLGE[]; extern const int ERFOLGE_N;
extern const AusgabeArt AUSGABE_ARTEN[]; extern const int AUSGABE_ARTEN_N;
extern const char *const WEISHEITEN[]; extern const int WEISHEITEN_N;
extern const char *const SPRUECHE_UEBERZOGEN[]; extern const int SPRUECHE_UEBERZOGEN_N;
extern const char *const HEINAI_SPRUECHE[]; extern const int HEINAI_SPRUECHE_N;
extern const char *const HEINI_NAMEN[]; extern const int HEINI_NAMEN_N;
extern const char *const EMOJIS[]; extern const int EMOJIS_N;   /* Reaktions-Emojis (UTF-8) */
extern const char *const MEDIEN_ARTEN[]; /* Anzeigenamen */
extern const char *const EXPORT_STAND;   /* z. B. "22.11.2026 21:50" */
extern const uint32_t EXPORT_KENNUNG;    /* data_version des Exports */
extern const zeit_t EXPORT_ZEIT;
extern const char *const ONEDRIVE_LINK;
extern const char *const ESHOP_LINK;
extern const int REGEL_DAUER_BAD, REGEL_VERSCHLEISS, REGEL_VERSCHLEISS_CT;

/* ---------- die ganze Datenbank ---------- */
typedef struct {
    TABELLE(Profil) profile;
    TABELLE(Raum) raeume; TABELLE(Platz) plaetze; TABELLE(Utensil) utensilien;
    TABELLE(Konsole) konsolen; TABELLE(KonsolenDing) dinge; TABELLE(Wichtel) wichtel;
    TABELLE(Treffen) treffen; TABELLE(Zusage) zusagen; TABELLE(Anwesenheit) anwesenheit;
    TABELLE(Buchung) buchungen; TABELLE(Warte) warteliste; TABELLE(Alarm) alarme;
    TABELLE(Schlaf) schlaf; TABELLE(Ausleihe) ausleihen;
    TABELLE(Nachricht) nachrichten; TABELLE(Umfrage) umfragen; TABELLE(Stimme) stimmen; TABELLE(Reaktion) reaktionen;
    TABELLE(Erfolg) erfolge; TABELLE(Paar) paare;
    TABELLE(Ausgabe) ausgaben; TABELLE(Posten) posten; TABELLE(Zahlung) zahlungen;
    TABELLE(Frueher) frueher; TABELLE(Medium) medien; TABELLE(Daumen) daumen;
    TABELLE(Agenda) agenda; TABELLE(Aktivitaet) aktivitaeten; TABELLE(Kommentar) kommentare;
    TABELLE(Trip) trips; TABELLE(Fahrt) fahrten; TABELLE(Mitfahrer) mitfahrer;
    TABELLE(Temperatur) temperatur; TABELLE(Verlauf) verlauf;
} Datenbank;

extern Datenbank db;
extern zeit_t jetzt;            /* aktuelle Ortszeit (Sekunden seit 2020) */
extern int ich;                 /* gewähltes Profil */
extern int regel_dauer_bad, regel_verschleiss, regel_verschleiss_ct;
extern int design_dunkel;      /* Einstellung Hell/Dunkel (Gerät) */

/* Grunddaten aus dem Export in db kopieren (gen/data.c) */
void daten_laden(void);

#endif
