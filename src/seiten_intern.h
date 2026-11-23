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
enum { ID_TROTZDEM = 1, ID_MENUE, ID_UHR, ID_ART, ID_BETRAG, ID_TITEL, ID_FUER, ID_ZAHLER, ID_NACHRICHT, ID_CHAT_AKTION,
       ID_REAKTION, ID_UMFRAGE_FRAGE, ID_UMFRAGE_ANTW, ID_SCHLAF, ID_ZUSAGE, ID_KASSE_ZU, ID_BEENDEN, ID_VERLASSEN,
       ID_STATUS, ID_IDEE, ID_KOMMENTAR, ID_AGENDA_TAG, ID_AGENDA_ZEIT, ID_AGENDA_NAME, ID_JOURNAL_WEG, ID_AUSGABE_WEG,
       ID_REGEL, ID_EINCHECKEN, ID_ZAHLUNG, ID_IDEE_ART, ID_MEDIUM, ID_ALARM_ABBRUCH, ID_PROFIL_WECHSEL, ID_UMFRAGE_WAHL,
       ID_TREFFEN_MENUE, ID_ABSAGEN, ID_FORM, ID_PROFILFELD, ID_RAUM, ID_PLATZ, ID_UTENSIL, ID_KONSOLE, ID_DING, ID_WICHTEL,
       ID_PAAR, ID_WOHNEN, ID_TRIP, ID_FAHRT, ID_KOSTEN, ID_ESSEN, ID_BESUCH, ID_TEMP, ID_PLUS };
#endif
