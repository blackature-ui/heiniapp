/* seiten_intern.h – Gemeinsames der Seiten-Dateien */
#ifndef SEITEN_INTERN_H
#define SEITEN_INTERN_H
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "seiten.h"
#include "ui.h"
#include "gfx.h"
#include "text.h"
#include "assets.h"
#include "logik.h"
#include "aktion.h"
#include "plat.h"

extern int reiter, chat_ende, ruhm_reiter, ruhm_person, kasse_ansicht, ideen_reiter;
int param(void);
int fehler(const char *f);                 /* Meldung zeigen; 1 = Fehler */
int mach(Aktion *a, const char *ok);       /* ausführen, bei Erfolg ok melden; 1 = gemacht */
const char *zusage_text(int z);
const char *tage_bis(tag_t t);
const char *naechte_text(int n);
const char *art_text(int a);
int seiten_kopf(const char *titel, int extra);   /* Kopf einer Unterseite; KOPF_* */

/* seiten2.c */
void seite_chat(void);
void seite_ruhm(void);
void seite_profil(int p);
void seite_einst(void);
void seite_werda(void);
void seite_frueher(void);
void seite_verlauf(void);
void seite_tour(void);
void seite_bildwahl(void);
void seite_farbwahl(void);
void seite_raeume(void);
void seite_gaming(void);
void seite_treffen_form(int t);
void treffen_form_neu(int t);

/* Dialog-Kennungen */
/* Abstand 1000: ID + k (k < 1000) und ID * 1000 + i dürfen sich nie überschneiden */
enum { ID_TROTZDEM = 1000, ID_MENUE = 2000, ID_UHR = 3000, ID_ART = 4000, ID_BETRAG = 5000, ID_TITEL = 6000,
       ID_FUER = 7000, ID_ZAHLER = 8000, ID_NACHRICHT = 9000, ID_CHAT_AKTION = 10000, ID_REAKTION = 11000, ID_UMFRAGE_FRAGE = 12000,
       ID_UMFRAGE_ANTW = 13000, ID_SCHLAF = 14000, ID_ZUSAGE = 15000, ID_KASSE_ZU = 16000, ID_BEENDEN = 17000, ID_VERLASSEN = 18000,
       ID_STATUS = 19000, ID_IDEE = 20000, ID_KOMMENTAR = 21000, ID_AGENDA_TAG = 22000, ID_AGENDA_ZEIT = 23000, ID_AGENDA_NAME = 24000,
       ID_JOURNAL_WEG = 25000, ID_AUSGABE_WEG = 26000, ID_REGEL = 27000, ID_EINCHECKEN = 28000, ID_ZAHLUNG = 29000, ID_IDEE_ART = 30000,
       ID_MEDIUM = 31000, ID_ALARM_ABBRUCH = 32000, ID_PROFIL_WECHSEL = 33000, ID_UMFRAGE_WAHL = 34000, ID_TREFFEN_MENUE = 35000, ID_ABSAGEN = 36000,
       ID_FORM = 37000, ID_PROFILFELD = 38000, ID_RAUM = 39000, ID_PLATZ = 40000, ID_UTENSIL = 41000, ID_KONSOLE = 42000,
       ID_DING = 43000, ID_WICHTEL = 44000, ID_PAAR = 45000, ID_WOHNEN = 46000, ID_TRIP = 47000, ID_FAHRT = 48000,
       ID_KOSTEN = 49000, ID_ESSEN = 50000, ID_BESUCH = 51000, ID_TEMP = 52000, ID_PLUS = 53000 };
#endif
