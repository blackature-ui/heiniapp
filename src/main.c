/* main.c – Heini-App 64: Start, Uhr, Hauptschleife */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "plat.h"
#include "gfx.h"
#include "text.h"
#include "ui.h"
#include "logik.h"
#include "aktion.h"
#include "seiten.h"

int32_t uhr_aus_datum(int j, int mo, int d, int h, int mi, int s) {
    return ((int32_t)datum_zu_tag(j, mo, d) * 24 + h) * 3600 + mi * 60 + s;
}

volatile uint32_t mess_zeichnen_takte, mess_zeichnen_max;   /* Zeichendauer (für tools/n64sim) */
static void zeichnen(const Eingabe *e) {
    uint32_t t0 = plat_takt();
    int z;
    uint16_t *px = plat_bild_start(&z);
    gfx_start(px, z);
    ui_bild_start(e);
    for (int i = 0; i < 4; i++) {
        int vorher = seite_aktuell();
        nochmal_zeichnen = 0;
        seiten_zeichnen();
        int r = ui_bild_ende();
        if (!r && seite_aktuell() == vorher) break;
        ui_bild_start(NULL);
    }
    dialoge_zeichnen();
    mess_zeichnen_takte = plat_takt() - t0;
    if (mess_zeichnen_takte > mess_zeichnen_max) mess_zeichnen_max = mess_zeichnen_takte;
    plat_bild_ende();
}

#ifdef HOST
extern int host_selbsttest(const char *pfad);
extern int host_weiter(void);
int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "--selbsttest")) {
        plat_init();
        jetzt = EXPORT_ZEIT;
        journal_laden();
        return host_selbsttest(argv[2]);
    }
    extern void host_argumente(int argc, char **argv);
    host_argumente(argc, argv);
#else
int main(void) {
#endif
    plat_init();
    jetzt = EXPORT_ZEIT;
    journal_laden();
    int32_t s;
    if (plat_uhr(&s)) { uhr_setzen(s); uhr_gestellt = 2; }
    else {
        zeit_t z = journal_letzte_zeit();
        uhr_setzen(z > EXPORT_ZEIT ? z : EXPORT_ZEIT);
        uhr_gestellt = 0;
    }
    seiten_start();
    Eingabe e;
    memset(&e, 0, sizeof e);
    zeit_t zuletzt = -1;
    int meldung_vorher = 0;
    for (;;) {
#ifdef HOST
        if (!host_weiter()) break;
#endif
        plat_eingabe(&e);
        jetzt = uhr_basis + (zeit_t)((plat_ms() - uhr_basis_ms) / 1000);
        int m = ui_meldung_aktiv();
        if (!e.gedrueckt && !nochmal_zeichnen && jetzt == zuletzt && m == meldung_vorher) { plat_warten(); continue; }
        meldung_vorher = m;
        zuletzt = jetzt;
        uhren_aufraeumen(jetzt);
        zeichnen(&e);
    }
    return 0;
}
