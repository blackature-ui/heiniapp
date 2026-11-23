/* seiten.h – die Seiten der Heini-App 64 */
#ifndef SEITEN_H
#define SEITEN_H
#include "model.h"

enum { SE_LOGO = 0, SE_PROFILWAHL, SE_HAUPT, SE_CHAT, SE_RUHM, SE_PROFIL, SE_TREFFEN, SE_FRUEHER, SE_EINST, SE_WERDA, SE_ALARM,
       SE_MEIN, SE_TREFFEN_FORM, SE_BILDWAHL, SE_FARBWAHL, SE_RAEUME, SE_GAMING, SE_VERLAUF, SE_TOUR, SE_TRIPS };
enum { R_HEUTE = 0, R_PLAN, R_KASSE, R_TREFFEN, R_IDEEN };

extern int uhr_gestellt;
void seite_auf(int seite, int param);
void seite_zurueck(void);
void seiten_zeichnen(void);
void seiten_start(void);
extern int nochmal_zeichnen;
extern zeit_t uhr_basis; extern uint32_t uhr_basis_ms;
void uhr_setzen(zeit_t z);
int seite_aktuell(void);

#endif
