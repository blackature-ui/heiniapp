/* aktion.h – alles, was am Fernseher geändert wird, läuft als Aktion. Jede Aktion wird geprüft (Regeln aus
 * server.js/rules.js), ausgeführt und ins Journal im SRAM geschrieben; beim Start wird das Journal erneut
 * ausgeführt. So bleiben die Grunddaten aus dem Export unverändert im ROM. */
#ifndef AKTION_H
#define AKTION_H
#include "model.h"

enum {
    A_UHR = 1, A_ANKUNFT, A_VERLASSEN, A_REIN, A_FERTIG, A_PARTNER, A_LANG, A_WARTEN, A_NICHT_WARTEN,
    A_ALARM, A_ALARM_BRING, A_ALARM_ABBRUCH, A_ZUSAGE, A_SCHLAF, A_SCHLAF_WEG, A_AUSLEIHE,
    A_NACHRICHT, A_REAKTION, A_STIMME, A_UMFRAGE_ENDE, A_UMFRAGE_NEU, A_ANHEFTEN,
    A_AUSGABE, A_AUSGABE_WEG, A_KASSE_ZU, A_KASSE_AUF, A_ZAHLUNG,
    A_DAUMEN, A_ERLEDIGT, A_KOMMENTAR, A_AGENDA, A_REGEL, A_TREFFEN_BEENDEN, A_IDEE, A_STATUS,
    A_EINCHECKEN_FUER, A_AGENDA_WEG, A_KOMMENTAR_WEG, A_NACHRICHT_WEG,
    A_ANZAHL
};

#define AKTION_TEXT_MAX 240
typedef struct {
    uint8_t typ;
    int8_t profil;
    zeit_t zeit;
    int16_t a, b, c, d;
    int32_t betrag;
    uint32_t maske;
    char text[AKTION_TEXT_MAX + 1];
} Aktion;

/* Führt die Aktion für das Profil `ich` zur Zeit `jetzt` aus. Ergebnis: NULL = gemacht, sonst Fehlertext.
 * Beginnt der Fehlertext mit '?', ist es eine Nachfrage (z. B. „Lena ist dran. Trotzdem rein?“). */
const char *tu(Aktion *a);
Aktion *aktion_neu(int typ);              /* leere Aktion mit ich und jetzt */

/* Journal */
void journal_laden(void);                 /* Grunddaten laden und Journal ausführen */
int journal_frei(void);                   /* freie Bytes im SRAM */
int journal_eintraege(void);
int journal_da(void);                     /* 0 = kein SRAM gefunden */
zeit_t journal_letzte_zeit(void);
void journal_loeschen(void);

void system_nachricht(const char *text);
extern int letzte_neue_nachricht;         /* Index der zuletzt angelegten Nachricht */

#endif
