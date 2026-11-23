/* seiten.c – alle Seiten der Heini-App 64: Profilwahl (Hauptmenü), Heute, Plan, Kasse, Treffen, Ideen,
 * Chat, Ruhmeshalle, Profile, Treffen-Detail, Frühere Treffen, Einstellungen, Klopapier-Alarm. */
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

int nochmal_zeichnen;
int uhr_gestellt;
zeit_t uhr_basis; uint32_t uhr_basis_ms;

typedef struct { int seite, param; } Ebene;
static Ebene stapel[16];
static int tiefe;
static int reiter = R_HEUTE;
static int plan_tag, ruhm_reiter, ruhm_person = -1, alarm_gesehen = -1, chat_ende = 1, logo_start;

enum { ID_TROTZDEM = 1, ID_MENUE, ID_UHR, ID_ART, ID_BETRAG, ID_TITEL, ID_FUER, ID_ZAHLER, ID_NACHRICHT, ID_CHAT_AKTION,
       ID_REAKTION, ID_UMFRAGE_FRAGE, ID_UMFRAGE_ANTW, ID_SCHLAF, ID_ZUSAGE, ID_KASSE_ZU, ID_BEENDEN, ID_VERLASSEN,
       ID_STATUS, ID_IDEE, ID_KOMMENTAR, ID_AGENDA_TAG, ID_AGENDA_ZEIT, ID_AGENDA_NAME, ID_JOURNAL_WEG, ID_AUSGABE_WEG,
       ID_REGEL, ID_EINCHECKEN, ID_ZAHLUNG, ID_IDEE_ART, ID_MEDIUM, ID_ALARM_ABBRUCH, ID_PROFIL_WECHSEL, ID_UMFRAGE_WAHL };

void uhr_setzen(zeit_t z) { uhr_basis = z; uhr_basis_ms = plat_ms(); jetzt = z; }
int seite_aktuell(void) { return tiefe ? stapel[tiefe - 1].seite : SE_LOGO; }
static int param(void) { return tiefe ? stapel[tiefe - 1].param : 0; }
void seite_auf(int s, int p) { if (tiefe < 16) stapel[tiefe++] = (Ebene){ s, p }; ui_seite_neu(); nochmal_zeichnen = 1; }
void seite_zurueck(void) { if (tiefe > 1) tiefe--; ui_seite_neu(); nochmal_zeichnen = 1; }
static void seite_ersetzen(int s, int p) { if (tiefe) tiefe--; seite_auf(s, p); }

static int fehler(const char *f) {
    nochmal_zeichnen = 1;
    if (!f) return 0;
    meldung(f[0] == '?' ? f + 1 : f);
    return 1;
}
static const char *art_text(int a) { return a >= 0 && a < AUSGABE_ARTEN_N ? AUSGABE_ARTEN[a].text : "Ausgabe"; }
static const char *zusage_text(int z) { return z == ZUSAGE_KOMME ? "Komme" : z == ZUSAGE_VIELLEICHT ? "Vielleicht" : z == ZUSAGE_NEIN ? "Komme nicht" : "Noch offen"; }
static const char *tage_bis(tag_t t) {
    int d = t - tag_von(jetzt);
    return d == 0 ? "heute" : d == 1 ? "morgen" : d > 1 ? tmp("in %d Tagen", d) : d == -1 ? "gestern" : tmp("vor %d Tagen", -d);
}
static const char *naechte_text(int n) { return n == 1 ? "1 Nacht" : tmp("%d Nächte", n); }

/* ======================================================================= Logo, Profilwahl */
static void seite_logo(void) {
    gfx_fuellen(RGB(0x7b, 0x16, 0x19));
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            uint16_t c = logo128[y * 128 + x];
            if (c & 1) gfx_rechteck(BILD_B / 2 - 128 + x * 2, 80 + y * 2, 2, 2, c);
        }
    gfx_text_mitte(BILD_B / 2, 352, "Heini-App 64", C_WEISS, 1, 3);
    gfx_text_mitte(BILD_B / 2, 410, "Bad, Klo, Schlafplatz und Chat beim Freundestreffen", C_WEISS, 0, 1);
    if ((ui_taste() & (T_A | T_START)) || plat_ms() - (uint32_t)logo_start > 2200) seite_ersetzen(SE_PROFILWAHL, 0);
    nochmal_zeichnen = 1;
}

static void seite_profilwahl(void) {
    gfx_fuellen(C_HG);
    if (seite_aktuell() == SE_PROFILWAHL) ich = -1;
    kopf("Heini-App 64", 0, 0);
    ui_bereich(64, FUSS_Y);
    ui_text_gross("Wer bist du?", C_TEXT, 2);
    int l = laufendes_treffen(), n = naechstes_treffen();
    if (l >= 0) ui_text(tmp(S_HAUS " Läuft gerade: %s (%s)", treffen_titel(l), zeitraum_text(db.treffen.v[l].von, db.treffen.v[l].bis)), C_GELB);
    else if (n >= 0) ui_text(tmp(S_UHR " Nächstes Treffen %s: %s", tage_bis(db.treffen.v[n].von), treffen_titel(n)), C_TEXT2);
    ui_abstand(6);
    /* Profile als Kacheln, 5 je Zeile */
    int sp = 5, kb = INHALT_B / sp;
    int y0 = ui_y(), k = 0;
    for (int p = 0; p < db.profile.n; p++) {
        if (!db.profile.v[p].aktiv) continue;
        int x = RAND_X + (k % sp) * kb, y = y0 + (k / sp) * 112;
        int f = ui_fokus_hier();
        /* Element anmelden: ein unsichtbarer Knopf über der Kachel */
        extern void ui_kachel(int x, int y, int b, int h);
        ui_kachel(x + 4, y, kb - 8, 104);
        if (f) gfx_runde_box(x + 1, y - 3, kb - 2, 110, 14, C_WEISS);
        gfx_runde_box(x + 4, y, kb - 8, 104, 12, f ? C_FLAECHE2 : C_FLAECHE);
        avatar(p, x + kb / 2 - 24, y + 10, 48);
        gfx_text_mitte(x + kb / 2, y + 68, tmp("%.13s", pname(p)), C_TEXT, 1, 1);
        if (l >= 0 && ist_anwesend(l, p)) gfx_text_mitte(x + kb / 2, y + 86, "ist da", C_GRUEN, 0, 1);
        if (f && (ui_taste() & T_A)) {
            ich = p;
            reiter = R_HEUTE;
            alarm_gesehen = -1;
            seite_auf(SE_HAUPT, 0);
            meldung(tmp("Hallo %s!", pname(p)));
        }
        k++;
    }
    extern void ui_weiter(int dy);
    ui_weiter(((k + sp - 1) / sp) * 112 + 10);
    if (!uhr_gestellt) ui_text(S_UHR " Diese Konsole hat keine Uhr. Bitte einmal die Uhrzeit stellen.", C_GELB);
    if (ui_knopf(tmp(S_UHR " Uhr stellen (%s, %s)", datum_text(tag_von(jetzt)), uhr_text(jetzt)))) uhr_dialog(ID_UHR, jetzt);
    if (ui_knopf_neben(S_PAD " Einstellungen")) seite_auf(SE_EINST, 0);
    int32_t s;
    if (uhr_ergebnis(ID_UHR, &s)) {
        uhr_setzen(s);
        uhr_gestellt = 1;
        Aktion *a = aktion_neu(A_UHR); a->profil = -1; a->zeit = s;
        tu(a);
        meldung(tmp("Uhr gestellt: %s, %s", datum_lang(tag_von(s)), uhr_text(s)));
        nochmal_zeichnen = 1;
    }
    ui_abstand(8);
    ui_text(tmp("Datenstand: Export vom %s · App-Version 1.61.0 · %d Änderungen am Fernseher", EXPORT_STAND, journal_eintraege()), C_TEXT2);
    ui_bereich_ende();
    fuss("Steuerkreuz: auswählen   A: los geht's");
}

/* ======================================================================= Heute */
static void karte_bad(int t, int raum) {
    const Raum *r = &db.raeume.v[raum];
    int b = drin_in(raum, t), dran = dran_in(raum, t);
    int warte[16], nw = schlange(raum, t, warte, 16);
    uint16_t band = b >= 0 ? C_ROT : C_GRUEN;
    ui_karte(tmp("%s %s · %s", S_KLO, r->name, b >= 0 ? "Besetzt" : "Frei"), band);
    if (b >= 0) {
        const Buchung *x = &db.buchungen.v[b];
        int seit = (int)((jetzt - x->start) / 60);
        const char *wer = x->partner >= 0 && !x->partner_bis ? tmp("%s & %s", pname(x->profil), pname(x->partner)) : pname(x->profil);
        ui_person(x->profil, wer, tmp("seit %d Min, läuft offen%s", seit, x->lang ? " · Dauert länger" : " · Dauert nicht lange"), dauer_text((int)(jetzt - x->start)));
    } else if (dran >= 0) {
        ui_text(tmp("%s ist dran.", pname(db.warteliste.v[dran].profil)), C_GELB);
    } else ui_text("Niemand drin. Rein da!", C_TEXT2);
    if (nw) {
        ui_text("Warteschlange:", C_TEXT2);
        for (int i = 0; i < nw; i++) {
            const Warte *w = &db.warteliste.v[warte[i]];
            ui_zeile(tmp("%d. %s%s", i + 1, pname(w->profil), w->partner >= 0 ? tmp(" & %s", pname(w->partner)) : ""), wann_text(w->seit, jetzt), C_TEXT);
        }
    }
    if (r->info && r->info[0]) ui_text(tmp("(i) %s", r->info), C_TEXT2);
    int anw = ist_anwesend(t, ich), meine = meine_uhr(ich), wp = warte_platz(ich, raum, t);
    if (anw && meine < 0 && b < 0) {
        if (ui_knopf_farbe("Rein", C_GRUEN, C_WEISS)) {
            Aktion *a = aktion_neu(A_REIN); a->a = (int16_t)t; a->b = (int16_t)raum;
            const char *f = tu(a);
            if (f && f[0] == '?') frage(ID_TROTZDEM * 100 + raum, f + 1, "Trotzdem rein", "Lieber warten");
            else if (!fehler(f)) meldung(tmp("Viel Spaß im %s! Die Uhr läuft.", r->name));
        }
    }
    if (anw && meine < 0 && b >= 0 && wp < 0) {
        if (ui_knopf("Ich will danach")) { Aktion *a = aktion_neu(A_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; if (!fehler(tu(a))) meldung("Du stehst an. Wer vorne steht, bleibt vorne."); }
        if (partner_von(ich) >= 0 && ist_anwesend(t, partner_von(ich)) && ui_knopf_neben(tmp("Mit %s anstellen", pname(partner_von(ich))))) {
            Aktion *a = aktion_neu(A_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; a->c = 1; fehler(tu(a));
        }
    }
    if (wp >= 0 && ui_knopf("Nicht mehr warten")) { Aktion *a = aktion_neu(A_NICHT_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; fehler(tu(a)); }
    if (frage_ergebnis(ID_TROTZDEM * 100 + raum) == 0) {
        Aktion *a = aktion_neu(A_REIN); a->a = (int16_t)t; a->b = (int16_t)raum; a->c = 1;
        if (!fehler(tu(a))) meldung("Vorgedrängelt! Die Uhr läuft.");
    }
    ui_karte_ende();
}

static void karte_lauf(int t, int b) {
    const Buchung *x = &db.buchungen.v[b];
    int sek = (int)(jetzt - x->start), plan = regel_dauer_bad * 60;
    ui_karte(tmp("Läuft gerade · %s", db.raeume.v[x->raum].name), pfarbe(ich));
    int y = ui_y();
    int anteil = sek * 1000 / (plan > 0 ? plan : 1);
    uint16_t farbe = sek > plan ? C_ROT : C_GRUEN;
    ui_ring(RAND_X + 14 + 60, y + 60, 56, 10, anteil > 1000 ? 1000 : anteil, farbe, C_FLAECHE2);
    gfx_text_mitte(RAND_X + 14 + 60, y + 44, dauer_text(sek), C_WEISS, 1, 2);
    gfx_text_mitte(RAND_X + 14 + 60, y + 80, S_KLOPAPIER, C_TEXT2, 0, 1);
    int tx = RAND_X + 150;
    gfx_text(tx, y + 8, x->partner >= 0 && !x->partner_bis ? tmp("Du und %s seid drin.", pname(x->partner)) : "Du bist drin.", C_TEXT, 1, 1);
    gfx_text(tx, y + 30, tmp("Vorschlag: %d Minuten. Spätestens nach 60 Minuten", regel_dauer_bad), C_TEXT2, 0, 1);
    gfx_text(tx, y + 48, "endet die Uhr von selbst.", C_TEXT2, 0, 1);
    if (sek > plan) gfx_text(tx, y + 72, tmp("%.52s", spruch_ueberzogen(b)), C_GELB, 0, 1);
    if (sek >= NOCH_DRIN * 60) gfx_text(tx, y + 92, "Bist du noch drin?", C_GELB, 1, 1);
    ui_abstand(128);
    if (ui_knopf_farbe("Bad jetzt freigeben", C_GRUEN, C_WEISS)) {
        Aktion *a = aktion_neu(A_FERTIG); a->a = (int16_t)b;
        if (!fehler(tu(a))) {
            int d = dran_in(x->raum, t);
            meldung(d >= 0 ? tmp("Bad ist frei. %s ist dran!", pname(db.warteliste.v[d].profil)) : "Bad ist frei. Danke!");
        }
    }
    if (ui_knopf_neben_farbe(S_KLOPAPIER " Klopapier-Alarm", C_ROT, C_WEISS)) { Aktion *a = aktion_neu(A_ALARM); a->a = (int16_t)t; if (!fehler(tu(a))) meldung("Alarm ausgelöst! Alle Anwesenden sehen ihn."); }
    if (ui_knopf(x->lang ? S_BOX_JA " Dauert länger" : S_BOX " Dauert länger")) { Aktion *a = aktion_neu(A_LANG); a->a = (int16_t)b; a->b = !x->lang; fehler(tu(a)); }
    int q = partner_von(ich);
    if (q >= 0 && x->profil == ich) {
        int mit = x->partner == q && !x->partner_bis;
        if (ui_knopf_neben(tmp("%s Als Pärchen (mit %s)", mit ? S_BOX_JA : S_BOX, pname(q)))) { Aktion *a = aktion_neu(A_PARTNER); a->a = (int16_t)b; a->b = !mit; fehler(tu(a)); }
    } else if (x->profil != ich && ui_knopf_neben("Bin nicht drin")) { Aktion *a = aktion_neu(A_PARTNER); a->a = (int16_t)b; a->b = 0; fehler(tu(a)); }
    ui_karte_ende();
}

static void karte_alarm(int t) {
    int al = offener_alarm(t);
    if (al < 0) return;
    const Alarm *a = &db.alarme.v[al];
    ui_karte(S_KLOPAPIER " Klopapier-Alarm!", C_ROT);
    ui_person(a->profil, tmp("%s braucht Klopapier!", pname(a->profil)), tmp("im %s · %s", a->raum >= 0 ? db.raeume.v[a->raum].name : "Bad", wann_text(a->ausgeloest, jetzt)), NULL);
    if (ist_anwesend(t, ich) && ui_knopf_farbe("Ich bring welche", C_GRUEN, C_WEISS)) { Aktion *x = aktion_neu(A_ALARM_BRING); x->a = (int16_t)al; if (!fehler(tu(x))) meldung("Held des Tages!"); }
    if (a->profil == ich && ui_knopf_neben("Abbrechen")) { Aktion *x = aktion_neu(A_ALARM_ABBRUCH); x->a = (int16_t)al; fehler(tu(x)); }
    ui_karte_ende();
}

static void karte_zusage(int t) {
    int z = zusage_von(t, ich);
    if (ist_gastgeber(t, ich)) { ui_text(S_HAUS " Du bist Gastgeber. Du kommst sowieso.", C_TEXT2); return; }
    ui_text(tmp("Deine Antwort: %s", zusage_text(z)), C_TEXT2);
    static const int ST[3] = { ZUSAGE_KOMME, ZUSAGE_VIELLEICHT, ZUSAGE_NEIN };
    static const char *const TX[3] = { "Komme", "Vielleicht", "Komme nicht" };
    for (int i = 0; i < 3; i++) {
        const char *s = tmp("%s%s", z == ST[i] ? S_HAKEN " " : "", TX[i]);
        int an = i == 0 ? ui_knopf_farbe(s, z == ST[i] ? C_GRUEN : C_FLAECHE2, C_WEISS) : ui_knopf_neben_farbe(s, z == ST[i] ? (i == 2 ? C_ROT : C_BLAU) : C_FLAECHE2, C_WEISS);
        if (an) { Aktion *a = aktion_neu(A_ZUSAGE); a->a = (int16_t)t; a->b = (int16_t)ST[i]; if (!fehler(tu(a))) meldung(i == 0 ? "Super! Das Klo freut sich." : i == 1 ? "Vielleicht ist besser als nein." : "Schade! Nächstes Mal."); }
    }
}

static void karte_kasse_kurz(int t) {
    int32_t stand[MAX_PROFILE]; uint32_t bet;
    int32_t summe = kassen_stand(t, stand, &bet);
    if (!summe || !(bet & (1u << ich))) return;
    int32_t c = stand[ich];
    ui_karte(S_EURO " Kasse", C_FLAECHE2);
    ui_zeile(c > 0 ? "Du bekommst" : c < 0 ? "Du zahlst" : "Du bist quitt", c ? euro_text(c < 0 ? -c : c) : "", c > 0 ? C_GRUEN : c < 0 ? C_ROT : C_TEXT);
    ui_zeile("Ausgaben insgesamt", euro_text(summe), C_TEXT2);
    if (db.treffen.v[t].kasse_zu_um) ui_text("Die Kasse ist zu. Zahlungen stehen unter „Kasse“.", C_TEXT2);
    ui_karte_ende();
}

static void karte_dein_tag(int t) {
    tag_t h = tag_von(jetzt);
    ui_karte(S_STERN " Dein Tag", C_GELB);
    int s = schlaf_von(t, ich, h);
    if (h < db.treffen.v[t].bis) {
        if (s >= 0) { int pl = db.schlaf.v[s].platz; ui_zeile(tmp(S_BETT " Heute Nacht: %s", db.plaetze.v[pl].name), db.raeume.v[db.plaetze.v[pl].raum].name, C_TEXT); }
        else if (!ist_gastgeber(t, ich)) ui_zeile(S_BETT " Heute Nacht: noch kein Schlafplatz", NULL, C_TEXT2);
    }
    int n = 0;
    for (int i = 0; i < db.agenda.n; i++) {
        const Agenda *a = &db.agenda.v[i];
        if (a->geloescht || a->treffen != t || a->datum != h) continue;
        ui_zeile(tmp("%s %s", a->uhrzeit >= 0 ? tmp("%02d:%02d", a->uhrzeit / 60, a->uhrzeit % 60) : "--:--", a->name), NULL, C_TEXT);
        n++;
    }
    if (!n) ui_zeile("Heute steht nichts in der Agenda.", NULL, C_TEXT2);
    for (int f = 0; f < db.fahrten.n; f++) {
        if (db.fahrten.v[f].geloescht || db.fahrten.v[f].treffen != t) continue;
        int ins[12], ni = fahrt_insassen(f, ins, 12), drin = 0;
        for (int i = 0; i < ni; i++) if (ins[i] == ich) drin = 1;
        if (!drin) continue;
        int tr = db.fahrten.v[f].trip;
        ui_zeile(tmp(S_AUTO " %s: %s", tr >= 0 ? db.trips.v[tr].titel : "Fahrt", db.fahrten.v[f].profil == ich ? "du fährst" : tmp("%s fährt", pname(db.fahrten.v[f].profil))), db.fahrten.v[f].richtung ? "zurück" : "hin", C_TEXT);
    }
    ui_karte_ende();
}

static void karte_offenes(void) {
    int n = 0;
    for (int u = db.umfragen.n - 1; u >= 0; u--) {
        int m = db.umfragen.v[u].nachricht;
        if (m < 0 || db.nachrichten.v[m].geloescht || umfrage_zu(u) || stimme_von(u, ich) >= 0) continue;
        if (!n) ui_karte("Offenes für dich", C_BLAU);
        if (ui_eintrag(db.nachrichten.v[m].profil, tmp("Umfrage: %s", db.nachrichten.v[m].text), "Noch nicht abgestimmt · im Chat", S_RECHTS)) { chat_ende = 1; seite_auf(SE_CHAT, m); }
        if (++n >= 3) break;
    }
    if (n) ui_karte_ende();
}

static void seite_heute(void) {
    tag_t h = tag_von(jetzt);
    ui_karte(S_KLO " Klo-Weisheit des Tages", C_FLAECHE2);
    ui_text(weisheit_des_tages(h), C_TEXT);
    ui_karte_ende();
    int l = laufendes_treffen();
    if (l >= 0) {
        int b = meine_uhr(ich);
        if (b >= 0) karte_lauf(l, b);
        karte_alarm(l);
        ui_karte(tmp(S_HAUS " %s", treffen_titel(l)), pfarbe(db.treffen.v[l].gastgeber >= 0 ? db.treffen.v[l].gastgeber : ich));
        ui_text(tmp("%s · Tag %d von %d", zeitraum_text(db.treffen.v[l].von, db.treffen.v[l].bis), h - db.treffen.v[l].von + 1, db.treffen.v[l].bis - db.treffen.v[l].von + 1), C_TEXT2);
        if (!ist_anwesend(l, ich)) {
            int an = anwesenheit_von(l, ich);
            if (an >= 0 && db.anwesenheit.v[an].abgereist) ui_text("Du bist schon abgereist. Schön war's!", C_TEXT2);
            if (ui_knopf_farbe(S_HAUS " Bin angekommen", C_GRUEN, C_WEISS)) {
                Aktion *a = aktion_neu(A_ANKUNFT); a->a = (int16_t)l;
                if (!fehler(tu(a))) meldung("Willkommen! Der Thron bebt.");
            }
        } else {
            int da[MAX_PROFILE], n = 0;
            for (int p = 0; p < db.profile.n; p++) if (ist_anwesend(l, p)) da[n++] = p;
            ui_text(tmp("Wer ist da? (%d)", n), C_TEXT2);
            ui_avatare(da, n);
            if (ui_knopf("Jemanden einchecken")) {
                static const char *namen[MAX_PROFILE]; static int idx[MAX_PROFILE]; int k = 0;
                for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv && !ist_anwesend(l, p)) { idx[k] = p; namen[k++] = pname(p); }
                if (k) auswahl(ID_EINCHECKEN, "Wen einchecken?", namen, k, 0); else meldung("Alle sind schon da.");
            }
            if (ui_knopf_neben("Mein Heini-Treffen")) seite_auf(SE_TREFFEN, l);
            int w = auswahl_ergebnis(ID_EINCHECKEN);
            if (w >= 0) {
                int k = 0, wer = -1;
                for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv && !ist_anwesend(l, p)) { if (k == w) wer = p; k++; }
                Aktion *a = aktion_neu(A_EINCHECKEN_FUER); a->a = (int16_t)l; a->b = (int16_t)wer; fehler(tu(a));
            }
        }
        ui_karte_ende();
        int bs[16], nb = baeder(l, bs, 16);
        for (int i = 0; i < nb; i++) karte_bad(l, bs[i]);
        if (!nb) { ui_karte("Bäder", C_FLAECHE2); ui_text("Für dieses Treffen sind keine Bäder eingetragen.", C_TEXT2); ui_karte_ende(); }
        if (ist_anwesend(l, ich) || hat_komme(l, ich)) karte_dein_tag(l);
        karte_kasse_kurz(l);
    } else {
        for (int i = 0; i < db.treffen.n; i++) if (treffen_status(i) == T_UEBERFAELLIG) {
            ui_karte(tmp("Bitte beenden: %s", treffen_titel(i)), C_GELB);
            ui_text("Das Treffen ist vorbei. Erst beenden, dann ab in die Statistik.", C_TEXT);
            if (ui_knopf("Zum Treffen")) seite_auf(SE_TREFFEN, i);
            ui_karte_ende();
        }
        int n = naechstes_treffen();
        if (n >= 0) {
            const Treffen *t = &db.treffen.v[n];
            ui_karte(tmp(S_UHR " Nächstes Treffen %s", tage_bis(t->von)), pfarbe(t->gastgeber));
            ui_text_gross(treffen_titel(n), C_TEXT, 1);
            ui_text(tmp("%s · %s", zeitraum_text(t->von, t->bis), naechte_text(treffen_naechte(n))), C_TEXT2);
            if (t->gastgeber >= 0) ui_text(tmp(S_HAUS " bei %s · %s", pname(t->gastgeber), treffen_adresse(n)), C_TEXT2);
            else ui_text(tmp(S_HAUS " auswärts · %s", treffen_adresse(n)), C_TEXT2);
            karte_zusage(n);
            if (ui_knopf("Zum Treffen " S_RECHTS)) seite_auf(SE_TREFFEN, n);
            ui_karte_ende();
        } else {
            ui_karte("Kein Treffen geplant", C_FLAECHE2);
            ui_text("Gerade ist kein Treffen eingetragen. Neue Treffen kommen mit dem nächsten Export auf das Modul.", C_TEXT2);
            ui_karte_ende();
        }
        int nach = nach_treffen(ich);
        if (nach >= 0) {
            ui_karte(tmp("Nach dem Treffen: %s", treffen_titel(nach)), C_FLAECHE2);
            Rekord r[12]; int nr = rekorde(nach, r, 12);
            for (int i = 0; i < nr && i < 4; i++) ui_zeile(tmp("%s %s: %s", sym(r[i].symbol), r[i].titel, pname(r[i].profil)), r[i].text, C_TEXT);
            if (ONEDRIVE_LINK[0]) ui_text(tmp(S_KAMERA " Fotos und Videos: %s", ONEDRIVE_LINK), C_TEXT2);
            if (ui_knopf("Zur Ruhmeshalle")) { ruhm_reiter = 0; seite_auf(SE_RUHM, 0); }
            ui_karte_ende();
        }
    }
    /* offene Zahlungen */
    int nz = 0;
    for (int i = 0; i < db.zahlungen.n; i++) {
        const Zahlung *z = &db.zahlungen.v[i];
        if (z->treffen < 0 || z->status == ZAHLUNG_BESTAETIGT || (z->von != ich && z->an != ich)) continue;
        if (!nz++) ui_karte(S_EURO " Zahlungen", C_GELB);
        if (z->von == ich) ui_zeile(tmp("Du zahlst an %s%s", pname(z->an), z->status == ZAHLUNG_GEMELDET ? " (gemeldet)" : ""), euro_text(z->betrag), C_ROT);
        else ui_zeile(tmp("%s zahlt an dich%s", pname(z->von), z->status == ZAHLUNG_GEMELDET ? " (gemeldet)" : ""), euro_text(z->betrag), C_GRUEN);
    }
    if (nz) { if (ui_knopf("Zur Kasse")) { reiter = R_KASSE; ui_seite_neu(); } ui_karte_ende(); }
    karte_offenes();
}

/* ======================================================================= Plan */
static void seite_plan(void) {
    int t = laufendes_treffen();
    if (t < 0) t = ruhm_treffen();
    if (t < 0) { ui_text("Noch kein Treffen mit Bad-Besuchen.", C_TEXT2); return; }
    const Treffen *tr = &db.treffen.v[t];
    if (plan_tag < tr->von || plan_tag > tr->bis) plan_tag = tag_von(jetzt) >= tr->von && tag_von(jetzt) <= tr->bis ? tag_von(jetzt) : tr->von;
    ui_text(tmp("%s · nur Anzeige: wer wann drin war", treffen_titel(t)), C_TEXT2);
    if (ui_knopf(S_LINKS)) { if (plan_tag > tr->von) plan_tag--; nochmal_zeichnen = 1; }
    if (ui_knopf_neben(tmp("%s", datum_lang(plan_tag)))) {}
    if (ui_knopf_neben(S_RECHTS)) { if (plan_tag < tr->bis) plan_tag++; nochmal_zeichnen = 1; }
    if (ui_taste() & T_C_LINKS) { if (plan_tag > tr->von) plan_tag--; nochmal_zeichnen = 1; }
    if (ui_taste() & T_C_RECHTS) { if (plan_tag < tr->bis) plan_tag++; nochmal_zeichnen = 1; }
    int bs[16], nb = baeder(t, bs, 16);
    zeit_t tag0 = (zeit_t)plan_tag * 86400;
    for (int i = 0; i < nb; i++) {
        ui_karte(tmp(S_KLO " %s", db.raeume.v[bs[i]].name), C_FLAECHE2);
        /* Zeitleiste 6–24 Uhr */
        int y = ui_y(), x0 = RAND_X + 14, w = INHALT_B - 28;
        gfx_rechteck(x0, y + 14, w, 20, C_HG);
        for (int hh = 6; hh <= 24; hh += 3) {
            int x = x0 + (hh - 6) * w / 18;
            gfx_linie_v(x, y + 10, 28, C_LINIE);
            gfx_text(x - 8, y - 4, tmp("%d", hh), C_TEXT2, 0, 1);
        }
        int n = 0;
        for (int b = 0; b < db.buchungen.n; b++) {
            const Buchung *x = &db.buchungen.v[b];
            if (x->geloescht || x->raum != bs[i] || x->treffen != t || x->start < tag0 - 3600 || x->start >= tag0 + 86400) continue;
            zeit_t e = buchung_ende(b);
            int a = (int)((x->start - tag0) / 60) - 360, z = (int)((e - tag0) / 60) - 360;
            if (z <= a) z = a + 1;
            if (a < 0) a = 0;
            if (z > 1080) z = 1080;
            if (z > 0 && a < 1080) gfx_rechteck(x0 + a * w / 1080, y + 16, (z - a) * w / 1080 < 2 ? 2 : (z - a) * w / 1080, 16, pfarbe(x->profil));
            n++;
        }
        if (tag_von(jetzt) == plan_tag) { int m = minute_von(jetzt) - 360; if (m >= 0 && m < 1080) gfx_rechteck(x0 + m * w / 1080, y + 8, 2, 32, C_ROT); }
        ui_abstand(46);
        for (int b = 0; b < db.buchungen.n; b++) {
            const Buchung *x = &db.buchungen.v[b];
            if (x->geloescht || x->raum != bs[i] || x->treffen != t || tag_von(x->start) != plan_tag) continue;
            zeit_t e = buchung_ende(b);
            const char *wer = x->partner >= 0 ? tmp("%s & %s", pname(x->profil), pname(x->partner)) : pname(x->profil);
            ui_zeile(tmp("%s–%s  %s", uhr_text(x->start), uhr_laeuft(b) ? "jetzt" : uhr_text(e), wer), uhr_laeuft(b) ? "läuft" : tmp("%d Min", (int)((e - x->start + 59) / 60)), uhr_laeuft(b) ? C_GELB : C_TEXT);
        }
        if (!n) ui_text("An diesem Tag war niemand drin.", C_TEXT2);
        ui_karte_ende();
    }
}

/* ======================================================================= Kasse */
static struct { int schritt, art; int32_t betrag; char titel[64]; uint32_t fuer; int zahler; } neu_a;
static const char *art_namen[16];

static void ausgabe_flow(int t) {
    int w = auswahl_ergebnis(ID_ART);
    if (w >= 0) { neu_a.art = w; zahl(ID_BETRAG, tmp("%s: Betrag", art_text(w)), 0); }
    int32_t c;
    if (zahl_ergebnis(ID_BETRAG, &c)) {
        if (c <= 0) meldung("Wie viel hat es gekostet?");
        else { neu_a.betrag = c; tastatur(ID_TITEL, tmp("%s: Wofür? (z. B. Laden oder Gericht)", art_text(neu_a.art)), "", 60); }
    }
    const char *tt = tastatur_ergebnis(ID_TITEL);
    if (tt) {
        strncpy(neu_a.titel, tt, sizeof neu_a.titel - 1); neu_a.titel[sizeof neu_a.titel - 1] = 0;
        uint32_t m = 0; int tn[MAX_PROFILE], n = treffen_teilnehmer(t, tn);
        for (int i = 0; i < n; i++) m |= 1u << tn[i];
        personen(ID_FUER, "Für wen war es?", m);
    }
    uint32_t m;
    if (personen_ergebnis(ID_FUER, &m)) {
        neu_a.fuer = m;
        static const char *namen[MAX_PROFILE];
        for (int p = 0; p < db.profile.n; p++) namen[p] = p == ich ? tmp("%s (ich)", pname(p)) : pname(p);
        auswahl(ID_ZAHLER, "Wer hat bezahlt?", namen, db.profile.n, ich);
    }
    w = auswahl_ergebnis(ID_ZAHLER);
    if (w >= 0) {
        Aktion *a = aktion_neu(A_AUSGABE);
        a->a = (int16_t)t; a->b = (int16_t)neu_a.art; a->c = (int16_t)w; a->betrag = neu_a.betrag; a->maske = neu_a.fuer;
        strncpy(a->text, neu_a.titel, AKTION_TEXT_MAX);
        if (!fehler(tu(a))) meldung(tmp("%s eingetragen: %s", art_text(neu_a.art), euro_text(neu_a.betrag)));
    }
}

static void seite_kasse(void) {
    int t = kasse_treffen();
    if (t < 0) { ui_text("Keine Kasse offen.", C_TEXT2); return; }
    const Treffen *tr = &db.treffen.v[t];
    int32_t stand[MAX_PROFILE]; uint32_t bet;
    int32_t summe = kassen_stand(t, stand, &bet);
    ui_karte(tmp(S_EURO " Kasse · %s", treffen_titel(t)), pfarbe(tr->gastgeber >= 0 ? tr->gastgeber : ich));
    ui_zeile("Ausgaben insgesamt", euro_text(summe), C_TEXT);
    if (tr->kasse_zu_um) ui_zeile(tmp("Kasse geschlossen am %s", datum_text(tag_von(tr->kasse_zu_um))), NULL, C_GELB);
    if (bet & (1u << ich)) { int32_t c = stand[ich]; ui_zeile(c > 0 ? "Du bekommst" : c < 0 ? "Du zahlst" : "Du bist quitt", c ? euro_text(c < 0 ? -c : c) : "", c > 0 ? C_GRUEN : c < 0 ? C_ROT : C_TEXT); }
    if (!tr->kasse_zu_um && ui_knopf_farbe("+ Ausgabe eintragen", C_GRUEN, C_WEISS)) {
        for (int i = 0; i < AUSGABE_ARTEN_N && i < 16; i++) art_namen[i] = tmp("%s %s", sym(AUSGABE_ARTEN[i].symbol), AUSGABE_ARTEN[i].text);
        auswahl(ID_ART, "Was hast du bezahlt?", art_namen, AUSGABE_ARTEN_N < 16 ? AUSGABE_ARTEN_N : 16, 0);
    }
    int darf = tr->gastgeber >= 0 ? ist_gastgeber(t, ich) : hat_komme(t, ich);
    if (darf && !tr->beendet_um) {
        if (!tr->kasse_zu_um ? ui_knopf_neben("Kasse schließen") : ui_knopf("Kasse wieder öffnen"))
            frage(ID_KASSE_ZU, tr->kasse_zu_um ? "Kasse wieder öffnen? Die Zahlungen werden zurückgesetzt." : "Kasse schließen? Danach zahlt jeder direkt an den, der bezahlt hat.", tr->kasse_zu_um ? "Öffnen" : "Schließen", "Abbrechen");
    }
    if (frage_ergebnis(ID_KASSE_ZU) == 0) { Aktion *a = aktion_neu(tr->kasse_zu_um ? A_KASSE_AUF : A_KASSE_ZU); a->a = (int16_t)t; fehler(tu(a)); }
    ui_karte_ende();
    ausgabe_flow(t);

    /* Zahlungen */
    if (tr->kasse_zu_um) {
        ui_karte("Zahlungen", C_GELB);
        int n = 0;
        for (int i = 0; i < db.zahlungen.n; i++) {
            const Zahlung *z = &db.zahlungen.v[i];
            if (z->treffen != t) continue;
            n++;
            const char *st = z->status == ZAHLUNG_BESTAETIGT ? S_HAKEN " bestätigt" : z->status == ZAHLUNG_GEMELDET ? "gemeldet" : "offen";
            if (ui_eintrag(z->von, tmp("%s " S_RECHTS " %s", pname(z->von), pname(z->an)), tmp("%s%s", st, db.profile.v[z->an].paypal[0] ? tmp(" · PayPal: %s", db.profile.v[z->an].paypal) : ""), euro_text(z->betrag))) {
                if (z->von == ich && z->status == ZAHLUNG_OFFEN) { Aktion *a = aktion_neu(A_ZAHLUNG); a->a = (int16_t)i; a->b = ZAHLUNG_GEMELDET; if (!fehler(tu(a))) meldung("Gemeldet. Jetzt muss es nur noch ankommen."); }
                else if (z->an == ich && z->status != ZAHLUNG_BESTAETIGT) { Aktion *a = aktion_neu(A_ZAHLUNG); a->a = (int16_t)i; a->b = ZAHLUNG_BESTAETIGT; if (!fehler(tu(a))) meldung("Bestätigt. Danke!"); }
                else meldung(z->von == ich || z->an == ich ? "Hier ist nichts mehr zu tun." : "Das ist nicht deine Zahlung.");
            }
        }
        if (!n) ui_text("Alle sind quitt.", C_TEXT2);
        ui_text("A auf eine Zahlung: „Hab bezahlt“ (Zahler) bzw. „Ist angekommen“ (Empfänger).", C_TEXT2);
        ui_karte_ende();
    } else {
        DirektPaar d[64]; int n = direkt_paare(t, d, 64);
        if (n) {
            ui_karte("So wird gezahlt (direkt an den Zahler)", C_FLAECHE2);
            for (int i = 0; i < n; i++)
                if (d[i].betrag > 0) ui_person(d[i].von, tmp("%s " S_RECHTS " %s", pname(d[i].von), pname(d[i].an)), db.profile.v[d[i].an].paypal[0] ? tmp("PayPal: %s", db.profile.v[d[i].an].paypal) : "", euro_text(d[i].betrag));
                else ui_zeile(tmp("%s und %s: geht genau auf", pname(d[i].von), pname(d[i].an)), NULL, C_TEXT2);
            ui_karte_ende();
        }
    }
    /* Ausgaben */
    ui_karte("Ausgaben", C_FLAECHE2);
    int n = 0;
    for (int i = db.ausgaben.n - 1; i >= 0; i--) {
        const Ausgabe *a = &db.ausgaben.v[i];
        if (a->geloescht || a->treffen != t) continue;
        n++;
        int8_t ids[64]; int32_t c[64]; int k = ausgabe_teile(i, ids, c, 64);
        uint32_t pm = 0; for (int j = 0; j < k; j++) pm |= 1u << ids[j];
        int np = 0; for (int p = 0; p < 32; p++) if (pm & (1u << p)) np++;
        if (ui_eintrag(a->profil, tmp("%s %s%s%s", sym(AUSGABE_ARTEN[a->art].symbol), art_text(a->art), a->titel && a->titel[0] ? " · " : "", a->titel ? a->titel : ""),
                       tmp("%s hat bezahlt · für %d %s", pname(a->profil), np, np == 1 ? "Person" : "Personen"), euro_text(a->betrag))) {
            if (!tr->kasse_zu_um) frage(ID_AUSGABE_WEG * 1000 + i, tmp("„%s“ über %s löschen?", a->titel && a->titel[0] ? a->titel : art_text(a->art), euro_text(a->betrag)), "Löschen", "Behalten");
        }
        if (frage_ergebnis(ID_AUSGABE_WEG * 1000 + i) == 0) { Aktion *x = aktion_neu(A_AUSGABE_WEG); x->a = (int16_t)i; fehler(tu(x)); }
    }
    if (!n) ui_text("Noch keine Ausgaben.", C_TEXT2);
    ui_karte_ende();
    /* Stände */
    if (summe) {
        ui_karte("Stand je Person", C_FLAECHE2);
        for (int p = 0; p < db.profile.n; p++) {
            if (!(bet & (1u << p))) continue;
            int32_t bz, an; kassen_konto(t, p, &bz, &an);
            ui_person(p, pname(p), tmp("bezahlt %s · Anteil %s", euro_text(bz), euro_text(an)), stand[p] ? tmp("%s%s", stand[p] > 0 ? "+" : "-", euro_text(stand[p] < 0 ? -stand[p] : stand[p])) : "quitt");
        }
        ui_karte_ende();
    }
    /* Trips */
    for (int i = 0; i < db.trips.n; i++) {
        if (db.trips.v[i].treffen != t) continue;
        const Trip *x = &db.trips.v[i];
        ui_karte(tmp(S_AUTO " Trip: %s", x->titel), C_FLAECHE2);
        if (x->datum) ui_text(tmp("%s%s", datum_text(x->datum), x->uhrzeit >= 0 ? tmp(" %02d:%02d", x->uhrzeit / 60, x->uhrzeit % 60) : ""), C_TEXT2);
        else if (x->wann && x->wann[0]) ui_text(x->wann, C_TEXT2);
        for (int f = 0; f < db.fahrten.n; f++) {
            if (db.fahrten.v[f].geloescht || db.fahrten.v[f].trip != i) continue;
            int ins[12], ni = fahrt_insassen(f, ins, 12);
            char mit[160] = "";
            for (int k = 1; k < ni; k++) { strncat(mit, pname(ins[k]), sizeof mit - strlen(mit) - 3); if (k + 1 < ni) strncat(mit, ", ", sizeof mit - strlen(mit) - 1); }
            ui_person(db.fahrten.v[f].profil, tmp("%s fährt (%s)", pname(db.fahrten.v[f].profil), db.fahrten.v[f].richtung ? "zurück" : "hin"), ni > 1 ? tmp("mit %s", mit) : "noch allein", tmp("%d frei", db.fahrten.v[f].plaetze - (ni - 1)));
        }
        ui_karte_ende();
    }
}

/* ======================================================================= Treffen (Liste) */
static void seite_treffenliste(void) {
    int l = laufendes_treffen();
    if (l >= 0) {
        ui_karte(S_HAUS " Läuft gerade", C_GRUEN);
        if (ui_eintrag(db.treffen.v[l].gastgeber, treffen_titel(l), zeitraum_text(db.treffen.v[l].von, db.treffen.v[l].bis), S_RECHTS)) seite_auf(SE_TREFFEN, l);
        ui_karte_ende();
    }
    ui_karte("Geplant", C_FLAECHE2);
    int ord[64], n = 0;
    for (int i = 0; i < db.treffen.n && n < 64; i++) {
        int st = treffen_status(i);
        if (st != T_KOMMEND && st != T_UEBERFAELLIG) continue;
        int k = n++;
        while (k > 0 && db.treffen.v[ord[k - 1]].von > db.treffen.v[i].von) { ord[k] = ord[k - 1]; k--; }
        ord[k] = i;
    }
    for (int i = 0; i < n; i++) {
        int t = ord[i], z = zusage_von(t, ich);
        const char *unter = tmp("%s · %s · %s", zeitraum_text(db.treffen.v[t].von, db.treffen.v[t].bis), tage_bis(db.treffen.v[t].von), ist_gastgeber(t, ich) ? "du bist Gastgeber" : zusage_text(z));
        if (ui_eintrag(db.treffen.v[t].gastgeber, treffen_titel(t), unter, S_RECHTS)) seite_auf(SE_TREFFEN, t);
        for (int k = 0; k < db.kommentare.n; k++)
            if (!db.kommentare.v[k].geloescht && db.kommentare.v[k].treffen == t && db.kommentare.v[k].angepinnt) ui_text(tmp(S_NADEL " %s: %s", pname(db.kommentare.v[k].profil), db.kommentare.v[k].text), C_TEXT2);
    }
    if (!n) ui_text("Nichts geplant.", C_TEXT2);
    ui_karte_ende();
    ui_karte("Vorbei", C_FLAECHE2);
    for (int i = db.treffen.n - 1; i >= 0; i--)
        if (treffen_status(i) == T_VORBEI && ui_eintrag(db.treffen.v[i].gastgeber, treffen_titel(i), tmp("%s · beendet", zeitraum_text(db.treffen.v[i].von, db.treffen.v[i].bis)), S_RECHTS)) seite_auf(SE_TREFFEN, i);
    if (ui_knopf(tmp(S_POKAL " Frühere Treffen (%d)", db.frueher.n))) seite_auf(SE_FRUEHER, 0);
    ui_karte_ende();
}

/* ======================================================================= Treffen (Detail) */
static int schlaf_nacht;
static void seite_treffen(int t) {
    const Treffen *tr = &db.treffen.v[t];
    int st = treffen_status(t);
    static const char *const STATUS[] = { "geplant", "läuft", "vorbei, bitte beenden", "beendet", "gelöscht" };
    ui_karte(treffen_titel(t), pfarbe(tr->gastgeber >= 0 ? tr->gastgeber : ich));
    ui_text(tmp("%s · %s · %s", zeitraum_text(tr->von, tr->bis), naechte_text(treffen_naechte(t)), STATUS[st]), C_TEXT2);
    int g[2], ng = treffen_gastgeber(t, g);
    if (ng) ui_text(tmp(S_HAUS " Gastgeber: %s%s%s", pname(g[0]), ng > 1 ? " & " : "", ng > 1 ? pname(g[1]) : ""), C_TEXT);
    else ui_text(S_HAUS " Auswärts – Gastgeber-Rechte hat jeder mit „Komme“.", C_TEXT);
    const char *adr = treffen_adresse(t);
    if (adr[0]) ui_text(tmp(S_NADEL " %s", adr), C_TEXT);
    if (tr->gastgeber >= 0 && db.profile.v[raum_besitzer(tr->gastgeber)].parkplaetze) ui_text(tmp(S_AUTO " %d Parkplätze %s", db.profile.v[raum_besitzer(tr->gastgeber)].parkplaetze, db.profile.v[raum_besitzer(tr->gastgeber)].park_hinweis), C_TEXT2);
    if (tr->info && tr->info[0]) ui_text(tr->info, C_TEXT2);
    if (st == T_KOMMEND || st == T_LAUFEND) karte_zusage(t);
    if (st == T_LAUFEND && ist_anwesend(t, ich) && tr->gastgeber != ich && ui_knopf("Treffen verlassen")) frage(ID_VERLASSEN, "Treffen verlassen? Deine Uhr endet, Schlafplätze werden frei.", "Verlassen", "Bleiben");
    if (frage_ergebnis(ID_VERLASSEN) == 0) { Aktion *a = aktion_neu(A_VERLASSEN); a->a = (int16_t)t; if (!fehler(tu(a))) meldung("Gute Heimfahrt!"); }
    if ((st == T_LAUFEND || st == T_UEBERFAELLIG) && hat_komme(t, ich) && ui_knopf_neben("Treffen beenden")) frage(ID_BEENDEN, "Treffen beenden? Es kommt unter „Frühere Treffen“ und lässt sich danach nicht mehr ändern.", "Beenden", "Abbrechen");
    if (frage_ergebnis(ID_BEENDEN) == 0) { Aktion *a = aktion_neu(A_TREFFEN_BEENDEN); a->a = (int16_t)t; if (!fehler(tu(a))) meldung("Treffen beendet. Ab in die Statistik!"); }
    ui_karte_ende();

    /* Zusagen */
    ui_karte("Wer kommt?", C_FLAECHE2);
    for (int s = 0; s < 3; s++) {
        int pr[MAX_PROFILE], n = 0;
        for (int p = 0; p < db.profile.n; p++) {
            int z = tr->gastgeber == p || tr->gastgeber2 == p ? ZUSAGE_KOMME : zusage_von(t, p);
            if (z == s) pr[n++] = p;
        }
        if (!n) continue;
        ui_text(tmp("%s (%d)", zusage_text(s), n), s == 0 ? C_GRUEN : C_TEXT2);
        ui_avatare(pr, n);
    }
    ui_karte_ende();

    /* Agenda */
    ui_karte(S_UHR " Agenda", C_FLAECHE2);
    int na = 0;
    for (tag_t d = tr->von; d <= tr->bis; d++)
        for (int i = 0; i < db.agenda.n; i++) {
            const Agenda *a = &db.agenda.v[i];
            if (a->geloescht || a->treffen != t || a->datum != d) continue;
            na++;
            if (ui_eintrag(a->profil, tmp("%s %s · %s", datum_text(d), a->uhrzeit >= 0 ? tmp("%02d:%02d", a->uhrzeit / 60, a->uhrzeit % 60) : "", a->name), a->notiz, NULL) && hat_komme(t, ich))
                frage(ID_AGENDA_NAME * 1000 + i, tmp("„%s“ aus der Agenda löschen?", a->name), "Löschen", "Behalten");
            if (frage_ergebnis(ID_AGENDA_NAME * 1000 + i) == 0) { Aktion *x = aktion_neu(A_AGENDA_WEG); x->a = (int16_t)i; fehler(tu(x)); }
        }
    if (!na) ui_text("Noch nichts geplant.", C_TEXT2);
    static tag_t ag_tag; static int ag_zeit;
    if (!tr->beendet_um && hat_komme(t, ich) && ui_knopf("+ Punkt")) {
        static const char *tage[31]; int n = 0;
        for (tag_t d = tr->von; d <= tr->bis && n < 31; d++) tage[n++] = datum_lang(d);
        auswahl(ID_AGENDA_TAG, "Welcher Tag?", tage, n, 0);
    }
    int w = auswahl_ergebnis(ID_AGENDA_TAG);
    if (w >= 0) { ag_tag = (tag_t)(tr->von + w); uhr_dialog(ID_AGENDA_ZEIT, (int32_t)ag_tag * 86400 + 14 * 3600); }
    int32_t s;
    if (uhr_ergebnis(ID_AGENDA_ZEIT, &s)) { ag_zeit = minute_von(s); ag_tag = tag_von(s); tastatur(ID_AGENDA_TAG + 500, "Was steht an?", "", 60); }
    const char *an = tastatur_ergebnis(ID_AGENDA_TAG + 500);
    if (an) { Aktion *a = aktion_neu(A_AGENDA); a->a = (int16_t)t; a->b = ag_tag; a->c = (int16_t)ag_zeit; strncpy(a->text, an, AKTION_TEXT_MAX); fehler(tu(a)); }
    ui_karte_ende();

    /* Kommentare */
    int nk = 0;
    for (int i = 0; i < db.kommentare.n; i++) if (!db.kommentare.v[i].geloescht && db.kommentare.v[i].treffen == t) nk++;
    if (nk || st == T_KOMMEND) {
        ui_karte("Kommentare", C_FLAECHE2);
        for (int i = 0; i < db.kommentare.n; i++) {
            const Kommentar *k = &db.kommentare.v[i];
            if (k->geloescht || k->treffen != t) continue;
            if (ui_eintrag(k->profil, tmp("%s%s", k->angepinnt ? S_NADEL " " : "", pname(k->profil)), k->text, NULL) && k->profil == ich)
                frage(ID_KOMMENTAR * 1000 + i, "Deinen Kommentar löschen?", "Löschen", "Behalten");
            if (frage_ergebnis(ID_KOMMENTAR * 1000 + i) == 0) { Aktion *x = aktion_neu(A_KOMMENTAR_WEG); x->a = (int16_t)i; fehler(tu(x)); }
        }
        if (st == T_KOMMEND && ui_knopf("+ Kommentar")) tastatur(ID_KOMMENTAR, "Dein Kommentar", "", 300);
        const char *kt = tastatur_ergebnis(ID_KOMMENTAR);
        if (kt) { Aktion *a = aktion_neu(A_KOMMENTAR); a->a = (int16_t)t; strncpy(a->text, kt, AKTION_TEXT_MAX); fehler(tu(a)); }
        ui_karte_ende();
    }

    /* Schlafplätze je Nacht */
    int nn = treffen_naechte(t);
    if (nn > 0) {
        if (schlaf_nacht < tr->von || schlaf_nacht >= tr->bis) schlaf_nacht = tag_von(jetzt) >= tr->von && tag_von(jetzt) < tr->bis ? tag_von(jetzt) : tr->von;
        ui_karte(S_BETT " Schlafplätze", C_FLAECHE2);
        if (ui_knopf(S_LINKS)) { if (schlaf_nacht > tr->von) schlaf_nacht--; nochmal_zeichnen = 1; }
        ui_knopf_neben(tmp("Nacht %s " S_RECHTS " %s", datum_text(schlaf_nacht), datum_text(schlaf_nacht + 1)));
        if (ui_knopf_neben(S_RECHTS)) { if (schlaf_nacht < tr->bis - 1) schlaf_nacht++; nochmal_zeichnen = 1; }
        int platz_idx[64], np = 0;
        for (int r = 0; r < db.raeume.n; r++) {
            if (db.raeume.v[r].art != RAUM_SCHLAF || !raum_im_treffen(r, t)) continue;
            int erste = 1;
            for (int p = 0; p < db.plaetze.n; p++) {
                if (db.plaetze.v[p].raum != r) continue;
                if (erste) { ui_text(tmp("%s%s", db.raeume.v[r].name, db.raeume.v[r].info && db.raeume.v[r].info[0] ? tmp(" (i) %s", db.raeume.v[r].info) : ""), C_TEXT2); erste = 0; }
                int auf[2], n = liegen_auf(t, p, schlaf_nacht, auf);
                const char *wer = n == 0 ? "frei" : n == 1 ? pname(auf[0]) : tmp("%s & %s", pname(auf[0]), pname(auf[1]));
                if (ui_eintrag(n ? auf[0] : -1, tmp("%s%s", db.plaetze.v[p].name, db.plaetze.v[p].fuer_zwei ? " (für zwei)" : ""), wer, schlaf_von(t, ich, schlaf_nacht) >= 0 && db.schlaf.v[schlaf_von(t, ich, schlaf_nacht)].platz == p ? S_HAKEN : NULL)) {
                    int mein = schlaf_von(t, ich, schlaf_nacht);
                    Aktion *a = aktion_neu(mein >= 0 && db.schlaf.v[mein].platz == p ? A_SCHLAF_WEG : A_SCHLAF);
                    a->a = (int16_t)t; a->b = (int16_t)p; a->c = schlaf_nacht;
                    if (!fehler(tu(a))) meldung(a->typ == A_SCHLAF ? tmp("Gute Nacht auf „%s“!", db.plaetze.v[p].name) : "Platz freigegeben.");
                }
                if (np < 64) platz_idx[np++] = p;
            }
        }
        if (!np) ui_text("Keine Schlafplätze eingetragen.", C_TEXT2);
        else ui_text("A auf einen Platz: nehmen bzw. wieder freigeben.", C_TEXT2);
        ui_karte_ende();
    }

    /* Utensilien */
    int besitzer = tr->gastgeber >= 0 ? raum_besitzer(tr->gastgeber) : -1, nu = 0;
    for (int u = 0; u < db.utensilien.n; u++) {
        if (besitzer < 0 || db.utensilien.v[u].profil != besitzer) continue;
        if (!nu++) ui_karte("Utensilien zum Ausleihen", C_FLAECHE2);
        const Utensil *x = &db.utensilien.v[u];
        int weg = ausgeliehen(t, u), mein = 0, i = ausleihe_von(t, u, ich);
        if (i >= 0) mein = db.ausleihen.v[i].anzahl;
        if (ui_eintrag(-1, tmp("%s%s", x->name, mein ? tmp(" · du hast %d", mein) : ""), tmp("%d von %d frei", x->anzahl - weg, x->anzahl), mein ? "zurück" : "leihen")) {
            Aktion *a = aktion_neu(A_AUSLEIHE); a->a = (int16_t)t; a->b = (int16_t)u; a->c = (int16_t)(mein ? 0 : 1);
            if (!fehler(tu(a))) meldung(mein ? "Zurückgegeben. Danke!" : tmp("%s ausgeliehen.", x->name));
        }
    }
    if (nu) ui_karte_ende();

    /* Räume */
    int bs[16], nb = baeder(t, bs, 16);
    if (nb) {
        ui_karte(S_KLO " Bäder", C_FLAECHE2);
        for (int i = 0; i < nb; i++) ui_zeile(db.raeume.v[bs[i]].name, db.raeume.v[bs[i]].info && db.raeume.v[bs[i]].info[0] ? "(i)" : NULL, C_TEXT);
        for (int i = 0; i < nb; i++) if (db.raeume.v[bs[i]].info && db.raeume.v[bs[i]].info[0]) ui_text(tmp("%s: %s", db.raeume.v[bs[i]].name, db.raeume.v[bs[i]].info), C_TEXT2);
        ui_karte_ende();
    }
    /* Rekorde des Treffens */
    if (st != T_KOMMEND) {
        Rekord r[12]; int n = rekorde(t, r, 12);
        if (n) {
            ui_karte(S_POKAL " Gold dieses Treffens", C_GELB);
            for (int i = 0; i < n; i++) ui_person(r[i].profil, tmp("%s %s", sym(r[i].symbol), r[i].titel), tmp("%s · %s", pname(r[i].profil), r[i].text), NULL);
            ui_karte_ende();
        }
    }
}

/* ======================================================================= Ideen */
static void seite_ideen(void) {
    static const char *const TITEL[5] = { "YouTube", "Spotify", "Links", "Ideen", "Bucketliste" };
    if (ESHOP_LINK[0]) { ui_karte("Heini-eShop", C_FLAECHE2); ui_text(ESHOP_LINK, C_TEXT2); ui_karte_ende(); }
    if (ui_knopf_farbe("+ Idee oder Bucketliste", C_GRUEN, C_WEISS)) {
        static const char *const ARTEN[2] = { "Idee (bei mir als Gastgeber)", "Bucketliste (für alle)" };
        auswahl(ID_IDEE_ART, "Wohin damit?", ARTEN, 2, 0);
    }
    static int idee_art;
    int w = auswahl_ergebnis(ID_IDEE_ART);
    if (w >= 0) { idee_art = w ? MED_BUCKET : MED_IDEE; tastatur(ID_IDEE, w ? "Was wollt ihr unbedingt mal machen?" : "Deine Idee", "", 80); }
    const char *it = tastatur_ergebnis(ID_IDEE);
    if (it) { Aktion *a = aktion_neu(A_IDEE); a->b = (int16_t)idee_art; strncpy(a->text, it, AKTION_TEXT_MAX); fehler(tu(a)); }
    static const int REIHE[5] = { MED_IDEE, MED_BUCKET, MED_YOUTUBE, MED_SPOTIFY, MED_LINK };
    for (int k = 0; k < 5; k++) {
        int art = REIHE[k], n = 0;
        for (int erledigt = 0; erledigt < 2; erledigt++)
            for (int i = 0; i < db.medien.n; i++) {
                const Medium *m = &db.medien.v[i];
                if (m->geloescht || m->art != art || (m->erledigt_um != 0) != erledigt) continue;
                if (!n++) ui_karte(TITEL[art], C_FLAECHE2);
                int hoch = 0, runter = 0, mein = 0;
                for (int d = 0; d < db.daumen.n; d++) if (db.daumen.v[d].medium == i) { if (db.daumen.v[d].wert > 0) hoch++; else if (db.daumen.v[d].wert < 0) runter++; if (db.daumen.v[d].profil == ich) mein = db.daumen.v[d].wert; }
                const char *unter = art == MED_IDEE ? tmp("bei %s%s", pname(m->gastgeber), m->erledigt_um ? " · erledigt" : "") : m->adresse && m->adresse[0] ? m->adresse : (m->erledigt_um ? "erledigt" : pname(m->profil));
                if (ui_eintrag(m->profil, tmp("%s%s", m->erledigt_um ? S_HAKEN " " : "", m->titel), unter, tmp(S_DAUMEN "%d " S_DAUMEN_R "%d%s", hoch, runter, mein > 0 ? " *" : mein < 0 ? " -" : ""))) {
                    static const char *const OPT[4] = { S_DAUMEN " Daumen hoch", S_DAUMEN_R " Daumen runter", "Daumen zurücknehmen", "Erledigt an/aus" };
                    auswahl(ID_MEDIUM * 1000 + i, m->titel, OPT, 4, 0);
                }
                int o = auswahl_ergebnis(ID_MEDIUM * 1000 + i);
                if (o >= 0 && o < 3) { Aktion *a = aktion_neu(A_DAUMEN); a->a = (int16_t)i; a->b = (int16_t)(o == 0 ? 1 : o == 1 ? -1 : 0); fehler(tu(a)); }
                if (o == 3) { Aktion *a = aktion_neu(A_ERLEDIGT); a->a = (int16_t)i; a->b = !m->erledigt_um; fehler(tu(a)); }
            }
        if (n) ui_karte_ende();
    }
    ui_text("Links lassen sich auf der N64 nicht öffnen (kein Internet) – Titel und Adresse stehen hier zum Abschreiben.", C_TEXT2);
}

/* ======================================================================= Chat */
static int chat_auswahl_n = -1;
static void seite_chat(int sprung) {
    gfx_fuellen(C_HG);
    kopf(S_HERZ " Chat", 0, 0);
    ui_bereich(64, FUSS_Y);
    /* angeheftete oben */
    for (int i = 0; i < db.nachrichten.n; i++) {
        const Nachricht *m = &db.nachrichten.v[i];
        if (!m->geloescht && m->angeheftet) ui_text(tmp(S_NADEL " %s: %.60s", pname(m->profil), m->text), C_GELB);
    }
    tag_t letzter = -1;
    int ab = db.nachrichten.n > 120 ? db.nachrichten.n - 120 : 0;
    if (ab > 0) ui_text("Ältere Nachrichten sind gekürzt (die letzten 120).", C_TEXT2);
    for (int i = ab; i < db.nachrichten.n; i++) {
        const Nachricht *m = &db.nachrichten.v[i];
        if (m->geloescht) continue;
        tag_t t = tag_von(m->zeit);
        if (t != letzter) {
            letzter = t;
            const char *tt = t == tag_von(jetzt) ? "Heute" : t == tag_von(jetzt) - 1 ? "Gestern" : datum_text(t);
            ui_abstand(4);
            gfx_text_mitte(BILD_B / 2, ui_y(), tt, C_TEXT2, 1, 1);
            ui_abstand(22);
        }
        if (m->art == NACHRICHT_SYSTEM) {
            extern int ui_systemzeile(const char *text, const char *zeit);
            if (ui_systemzeile(m->text, uhr_text(m->zeit))) chat_auswahl_n = i;
            continue;
        }
        extern int ui_blase(int profil, const char *name, const char *text, const char *zeit, int eigen, const char *extra, const char *antwort);
        char extra[160] = "";
        if (m->medien_anzahl) snprintf(extra, sizeof extra, "%s %d %s (nur in der App)", m->medium == MEDIUM_AUDIO ? S_NOTE : S_KAMERA, m->medien_anzahl,
                                       m->medium == MEDIUM_AUDIO ? "Sprachnachricht" : m->medium == MEDIUM_GIF ? "GIF" : m->medien_anzahl == 1 ? "Foto" : "Fotos");
        int u = m->art == NACHRICHT_UMFRAGE ? umfrage_von_nachricht(i) : -1;
        if (u >= 0) {
            int anz[UMFRAGE_MAX], leute, max = umfrage_zaehlen(u, anz, &leute), s = stimme_von(u, ich);
            int pos = (int)strlen(extra);
            pos += snprintf(extra + pos, sizeof extra - pos, "%sUmfrage%s · %d abgestimmt", pos ? " · " : "", umfrage_zu(u) ? " (zu)" : db.umfragen.v[u].mehrfach ? " (mehrere)" : "", leute);
            (void)max; (void)s;
        }
        char reak[64] = "";
        int e[32], ne = reaktionen_zaehlen(i, e);
        if (ne) { int p = 0; for (int k = 0; k < EMOJIS_N && p < 56; k++) if (e[k]) p += snprintf(reak + p, sizeof reak - p, "%s%d ", sym(EMOJIS[k]), e[k]); }
        const char *antwort = m->antwort_auf >= 0 && m->antwort_auf < db.nachrichten.n ? tmp("%s: %.40s", pname(db.nachrichten.v[m->antwort_auf].profil), db.nachrichten.v[m->antwort_auf].text) : NULL;
        if (ui_blase(m->profil, pname(m->profil), m->text, uhr_text(m->zeit), m->profil == ich, extra[0] ? extra : NULL, antwort)) chat_auswahl_n = i;
        if (u >= 0) {
            int anz[UMFRAGE_MAX], leute, max = umfrage_zaehlen(u, anz, &leute), s = stimme_von(u, ich);
            for (int k = 0; k < db.umfragen.v[u].n; k++) {
                int gewaehlt = s >= 0 && (db.stimmen.v[s].wahl & (1 << k));
                const char *z = tmp("%s %s  (%d)%s", gewaehlt ? S_BOX_JA : S_BOX, db.umfragen.v[u].antworten[k], anz[k], umfrage_zu(u) && anz[k] == max && max ? " " S_POKAL : "");
                if (!umfrage_zu(u) ? ui_knopf(z) : (ui_text(z, C_TEXT2), 0)) { Aktion *a = aktion_neu(A_STIMME); a->a = (int16_t)u; a->b = (int16_t)k; fehler(tu(a)); }
            }
        }
        if (reak[0]) ui_text(reak, C_TEXT2);
    }
    ui_abstand(6);
    if (ui_knopf_farbe("Nachricht schreiben", C_GRUEN, C_WEISS) || (ui_taste() & T_Z)) tastatur(ID_NACHRICHT, "Nachricht an alle", "", 1000 > AKTION_TEXT_MAX ? AKTION_TEXT_MAX : 1000);
    if (ui_knopf_neben("Umfrage starten")) tastatur(ID_UMFRAGE_FRAGE, "Umfrage: Was willst du wissen?", "", 120);
    ui_bereich_ende();
    /* beim Öffnen ans Ende rollen */
    if (chat_ende) { extern void ui_fokus_letztes(void); ui_fokus_letztes(); chat_ende = 0; nochmal_zeichnen = 1; }
    (void)sprung;
    static int antwort_auf = -1;
    const char *nt = tastatur_ergebnis(ID_NACHRICHT);
    if (nt && nt[0]) { Aktion *a = aktion_neu(A_NACHRICHT); a->a = (int16_t)antwort_auf; strncpy(a->text, nt, AKTION_TEXT_MAX); if (!fehler(tu(a))) chat_ende = 1; antwort_auf = -1; }
    static char frage_text[130]; static char antworten[AKTION_TEXT_MAX + 1]; static int n_antw;
    const char *ft = tastatur_ergebnis(ID_UMFRAGE_FRAGE);
    if (ft && ft[0]) { strncpy(frage_text, ft, sizeof frage_text - 1); antworten[0] = 0; n_antw = 0; tastatur(ID_UMFRAGE_ANTW, "Antwort 1 (leer lassen = fertig)", "", 40); }
    const char *at = tastatur_ergebnis(ID_UMFRAGE_ANTW);
    if (erg_tastatur_abgeschlossen(ID_UMFRAGE_ANTW) || at) {
        if (at && at[0] && n_antw < UMFRAGE_MAX) {
            strncat(antworten, "\n", sizeof antworten - strlen(antworten) - 1);
            strncat(antworten, at, sizeof antworten - strlen(antworten) - 1);
            n_antw++;
            if (n_antw < UMFRAGE_MAX) tastatur(ID_UMFRAGE_ANTW, tmp("Antwort %d (leer lassen = fertig)", n_antw + 1), "", 40);
        }
        if ((!at || !at[0] || n_antw >= UMFRAGE_MAX) && frage_text[0]) {
            if (n_antw < 2) meldung("Eine Umfrage braucht 2 bis 6 Antworten.");
            else { Aktion *a = aktion_neu(A_UMFRAGE_NEU); snprintf(a->text, sizeof a->text, "%s%s", frage_text, antworten); a->b = 0; if (!fehler(tu(a))) chat_ende = 1; }
            frage_text[0] = 0;
        }
    }
    /* Aktionen auf einer Nachricht */
    if (chat_auswahl_n >= 0) {
        static const char *opt[8]; static int code[8]; int n = 0;
        const Nachricht *m = &db.nachrichten.v[chat_auswahl_n];
        opt[n] = "Reagieren"; code[n++] = 0;
        if (m->art != NACHRICHT_SYSTEM) { opt[n] = "Antworten"; code[n++] = 1; }
        opt[n] = m->angeheftet ? "Lösen" : "Anheften"; code[n++] = 2;
        int u = umfrage_von_nachricht(chat_auswahl_n);
        if (u >= 0 && m->profil == ich && !umfrage_zu(u)) { opt[n] = "Umfrage beenden"; code[n++] = 3; }
        if (m->profil == ich && m->art != NACHRICHT_SYSTEM) { opt[n] = "Löschen"; code[n++] = 4; }
        auswahl(ID_CHAT_AKTION, tmp("%s: %.40s", pname(m->profil), m->text), opt, n, 0);
        static int merk; merk = chat_auswahl_n;
        extern int chat_merk; chat_merk = merk;
        for (int i = 0; i < 8; i++) { extern int chat_code[8]; chat_code[i] = code[i]; }
        chat_auswahl_n = -1;
    }
    extern int chat_merk; extern int chat_code[8];
    int w = auswahl_ergebnis(ID_CHAT_AKTION);
    if (w >= 0) {
        int c = chat_code[w];
        if (c == 0) {
            static const char *em[32];
            for (int k = 0; k < EMOJIS_N && k < 32; k++) em[k] = sym(EMOJIS[k]);
            auswahl(ID_REAKTION, "Reaktion", em, EMOJIS_N < 32 ? EMOJIS_N : 32, 0);
        } else if (c == 1) { antwort_auf = chat_merk; tastatur(ID_NACHRICHT, tmp("Antwort an %s", pname(db.nachrichten.v[chat_merk].profil)), "", AKTION_TEXT_MAX); }
        else if (c == 2) { Aktion *a = aktion_neu(A_ANHEFTEN); a->a = (int16_t)chat_merk; fehler(tu(a)); }
        else if (c == 3) { Aktion *a = aktion_neu(A_UMFRAGE_ENDE); a->a = (int16_t)umfrage_von_nachricht(chat_merk); fehler(tu(a)); }
        else if (c == 4) { Aktion *a = aktion_neu(A_NACHRICHT_WEG); a->a = (int16_t)chat_merk; fehler(tu(a)); }
    }
    w = auswahl_ergebnis(ID_REAKTION);
    if (w >= 0) { Aktion *a = aktion_neu(A_REAKTION); a->a = (int16_t)chat_merk; a->b = (int16_t)w; fehler(tu(a)); }
    fuss("A: Nachricht wählen   Z: schreiben   B: zurück   C: schnell rollen");
}
int chat_merk; int chat_code[8];

/* ======================================================================= Ruhmeshalle */
static void seite_ruhm(void) {
    gfx_fuellen(C_HG);
    kopf(S_POKAL " Ruhmeshalle", 0, 0);
    ui_bereich(64, FUSS_Y);
    static const char *const R[3] = { "Dieses Treffen", "Ewige Bestenliste", "Trophäenschrank" };
    for (int i = 0; i < 3; i++) {
        const char *s = tmp("%s%s", ruhm_reiter == i ? S_HAKEN " " : "", R[i]);
        if (i == 0 ? ui_knopf_farbe(s, ruhm_reiter == i ? C_BLAU : C_FLAECHE2, C_WEISS) : ui_knopf_neben_farbe(s, ruhm_reiter == i ? C_BLAU : C_FLAECHE2, C_WEISS)) { ruhm_reiter = i; nochmal_zeichnen = 1; }
    }
    if (ruhm_reiter < 2) {
        int t = ruhm_reiter == 0 ? ruhm_treffen() : -1;
        if (ruhm_reiter == 0 && t < 0) ui_text("Noch kein Treffen vorbei.", C_TEXT2);
        else {
            if (t >= 0) ui_text(treffen_titel(t), C_TEXT2);
            Rekord r[12]; int n = rekorde(t, r, 12);
            for (int i = 0; i < n; i++)
                if (ui_eintrag(r[i].profil, tmp("%s %s", sym(r[i].symbol), r[i].titel), tmp("%s · %s", pname(r[i].profil), r[i].text), NULL)) meldung(r[i].erklaerung);
            if (!n) ui_text("Noch keine Rekorde.", C_TEXT2);
        }
    } else {
        if (ruhm_person < 0) ruhm_person = ich;
        if (ui_eintrag(ruhm_person, tmp("Trophäen von %s", pname(ruhm_person)), "A: andere Person", S_RECHTS)) {
            static const char *namen[MAX_PROFILE];
            for (int p = 0; p < db.profile.n; p++) namen[p] = pname(p);
            auswahl(ID_PROFIL_WECHSEL, "Wessen Trophäenschrank?", namen, db.profile.n, ruhm_person);
        }
        int w = auswahl_ergebnis(ID_PROFIL_WECHSEL);
        if (w >= 0) { ruhm_person = w; nochmal_zeichnen = 1; }
        int erreicht = 0, gesamt = 0;
        for (int c = 0; c < ERFOLGE_N; c++) { gesamt++; if (erfolg_hat(ruhm_person, c)) erreicht++; }
        ui_text(tmp("%d von %d erreicht", erreicht, gesamt), C_GELB);
        for (int gruppe = 0; gruppe < 2; gruppe++) {
            ui_karte(gruppe == 0 ? "Klo-Trophäen" : "Heini-Geschichte", C_FLAECHE2);
            for (int c = 0; c < ERFOLGE_N; c++) {
                const ErfolgArt *e = &ERFOLGE[c];
                if ((e->art == 'H') != gruppe) continue;
                int hat = erfolg_hat(ruhm_person, c), anzahl = 0; zeit_t erst = 0;
                for (int i = 0; i < db.erfolge.n; i++) if (db.erfolge.v[i].profil == ruhm_person && db.erfolge.v[i].code == c) { anzahl++; if (!erst || db.erfolge.v[i].erreicht < erst) erst = db.erfolge.v[i].erreicht; }
                int zeigen = hat || !e->geheim;
                const char *titel = tmp("%s %s%s", zeigen ? sym(e->symbol) : "?", zeigen ? e->name : "???", anzahl > 1 ? tmp("  x%d", anzahl) : "");
                const char *unter = zeigen ? e->hinweis : "Geheim. Finde es selbst heraus!";
                if (ui_eintrag(-1, titel, hat ? tmp("%s · seit %s", unter, erst ? datum_voll(tag_von(erst)) : "?") : unter, hat ? S_HAKEN : NULL)) meldung(unter);
            }
            ui_karte_ende();
        }
    }
    ui_bereich_ende();
    fuss("A: Details   B: zurück");
}

/* ======================================================================= Profil */
static void seite_profil(int p) {
    gfx_fuellen(C_HG);
    const Profil *x = &db.profile.v[p];
    kopf(p == ich ? "Mein Profil" : pname(p), 0, 0);
    ui_bereich(64, FUSS_Y);
    ui_karte(NULL, 0);
    int y = ui_y();
    avatar(p, RAND_X + 14, y, 48);
    gfx_text(RAND_X + 80, y + 4, pname(p), C_TEXT, 1, 2);
    gfx_text(RAND_X + 80, y + 38, tmp("%s%s%s", FARBEN[x->farbe].name, x->heini ? " · Heini" : "", x->gastgeber ? " · kann Gastgeber sein" : ""), C_TEXT2, 0, 1);
    ui_abstand(60);
    if (x->status && x->status[0]) ui_text(tmp("„%s“", x->status), C_TEXT);
    if (p == ich && ui_knopf("Status ändern")) tastatur(ID_STATUS, "Dein Status (eine Zeile)", x->status, 139);
    const char *stt = tastatur_ergebnis(ID_STATUS);
    if (stt) { Aktion *a = aktion_neu(A_STATUS); strncpy(a->text, stt, AKTION_TEXT_MAX); fehler(tu(a)); }
    ui_karte_ende();
    int bes = raum_besitzer(p);
    const Profil *b = &db.profile.v[bes];
    if (b->strasse[0] || b->wohnort[0]) {
        ui_karte(S_HAUS " Adresse", C_FLAECHE2);
        ui_text(tmp("%s, %s %s", b->strasse, b->plz, b->wohnort), C_TEXT);
        if (wohnt_mit(p) >= 0) ui_text(tmp("Wohnt zusammen mit %s", pname(wohnt_mit(p))), C_TEXT2);
        if (b->parkplaetze) ui_text(tmp(S_AUTO " %d Parkplätze %s", b->parkplaetze, b->park_hinweis), C_TEXT2);
        ui_karte_ende();
    }
    /* Gaming */
    int nk = 0;
    for (int k = 0; k < db.konsolen.n; k++) {
        if (db.konsolen.v[k].profil != p) continue;
        if (!nk++) ui_karte(S_PAD " Gaming", C_FLAECHE2);
        const Konsole *ko = &db.konsolen.v[k];
        const char *name = ko->art >= 0 && ko->art < KONSOLEN_N ? KONSOLEN[ko->art].name : (ko->name && ko->name[0] ? ko->name : "Andere");
        int spiele = 0, zub = 0;
        for (int d = 0; d < db.dinge.n; d++) if (db.dinge.v[d].konsole == k) { if (db.dinge.v[d].art == DING_SPIEL) spiele++; else zub++; }
        ui_zeile(tmp(S_PAD " %s", name), tmp("%d Spiele · %d Zubehör", spiele, zub), C_TEXT);
        for (int d = 0; d < db.dinge.n; d++) {
            const KonsolenDing *di = &db.dinge.v[d];
            if (di->konsole != k) continue;
            ui_zeile(tmp("   %s %s", di->art == DING_SPIEL ? S_PUNKT : "+", di->name), di->anzahl > 1 ? tmp("x%d", di->anzahl) : NULL, C_TEXT2);
        }
    }
    if (nk) ui_karte_ende();
    ui_karte("Über", C_FLAECHE2);
    if (x->auto_) ui_zeile(tmp(S_AUTO " %s%s", x->auto_name[0] ? x->auto_name : "Auto", x->elektro ? " (elektrisch)" : ""), tmp("%d Plätze · %d,%d %s/100 km", x->auto_plaetze, x->verbrauch / 10, x->verbrauch % 10, x->elektro ? "kWh" : "l"), C_TEXT);
    ui_zeile(S_BIER " Getränke", x->kein_alkohol ? "kein Alkohol" : "alles", C_TEXT);
    if (x->paypal[0]) ui_zeile(S_EURO " PayPal", x->paypal, C_TEXT);
    int q = partner_von(p);
    if (q >= 0) ui_zeile(S_HERZ " Pärchen", pname(q), C_TEXT);
    ui_zeile(S_BETT " Wunschtemperatur", x->wunsch_temp ? tmp("%d " S_GRAD "C", x->wunsch_temp) : "egal", C_TEXT);
    if (x->unvertraeglich[0]) ui_text(tmp("Verträgt nicht: %s", x->unvertraeglich), C_TEXT2);
    if (x->mag_nicht[0]) ui_text(tmp("Mag nicht: %s", x->mag_nicht), C_TEXT2);
    ui_karte_ende();
    int nw = 0;
    for (int w = db.wichtel.n - 1; w >= 0; w--) {
        const Wichtel *wi = &db.wichtel.v[w];
        if (wi->profil != p) continue;
        if (!nw++) ui_karte(S_STERN " Wichtelgeschenke", C_FLAECHE2);
        ui_zeile(tmp("%d: %s", wi->jahr, wi->geschenk), wi->von >= 0 ? tmp("von %s", pname(wi->von)) : NULL, C_TEXT);
    }
    if (nw) ui_karte_ende();
    int ne = 0;
    for (int c = 0; c < ERFOLGE_N; c++) if (erfolg_hat(p, c)) ne++;
    if (ui_knopf(tmp(S_POKAL " Trophäenschrank (%d)", ne))) { ruhm_reiter = 2; ruhm_person = p; seite_auf(SE_RUHM, 0); }
    ui_bereich_ende();
    fuss("B: zurück");
}

/* ======================================================================= Wer ist da? */
static void seite_werda(void) {
    gfx_fuellen(C_HG);
    kopf("Wer ist da?", 0, 0);
    ui_bereich(64, FUSS_Y);
    int l = laufendes_treffen();
    for (int p = 0; p < db.profile.n; p++) {
        if (!db.profile.v[p].aktiv) continue;
        const char *unter = l >= 0 ? (ist_anwesend(l, p) ? "ist da" : hat_komme(l, p) ? "kommt noch" : "nicht dabei") : (db.profile.v[p].status ? db.profile.v[p].status : "");
        if (l >= 0 && ist_anwesend(l, p) && meine_uhr(p) >= 0) unter = tmp("ist im %s", db.raeume.v[db.buchungen.v[meine_uhr(p)].raum].name);
        if (ui_eintrag(p, pname(p), unter, S_RECHTS)) seite_auf(SE_PROFIL, p);
    }
    ui_bereich_ende();
    fuss("A: Profil ansehen   B: zurück");
}

/* ======================================================================= Frühere Treffen */
static void seite_frueher(void) {
    gfx_fuellen(C_HG);
    kopf("Frühere Treffen", 0, 0);
    ui_bereich(64, FUSS_Y);
    HeiniZahl pers[32], gast[32]; int np, ng, tage;
    int n = frueher_statistik(pers, &np, gast, &ng, &tage);
    ui_karte(S_POKAL " Statistik", C_GELB);
    ui_zeile("Treffen", tmp("%d", n), C_TEXT);
    ui_zeile("Treffen-Tage", tmp("%d (Schnitt %d,%d)", tage, n ? tage / n : 0, n ? (tage * 10 / n) % 10 : 0), C_TEXT);
    ui_text("Am häufigsten dabei:", C_TEXT2);
    for (int i = 0; i < np && i < 8; i++) {
        ui_zeile(tmp("%d. %s", i + 1, HEINI_NAMEN[pers[i].name]), tmp("%d Treffen · %d Tage · %d%%", pers[i].n, pers[i].tage, n ? pers[i].n * 100 / n : 0), C_TEXT);
        ui_balken(n ? pers[i].n * 1000 / n : 0, C_GELB);
    }
    ui_text("Am häufigsten Gastgeber:", C_TEXT2);
    for (int i = 0; i < ng && i < 6; i++) ui_zeile(HEINI_NAMEN[gast[i].name], tmp("%d", gast[i].n), C_TEXT);
    ui_karte_ende();
    int jahr = -1;
    for (int i = db.frueher.n - 1; i >= 0; i--) {
        const Frueher *f = &db.frueher.v[i];
        if (f->monat / 100 != jahr) { if (jahr >= 0) ui_karte_ende(); jahr = f->monat / 100; ui_karte(tmp("%d", jahr), C_FLAECHE2); }
        int k = 0; for (int h = 0; h < 32; h++) if (f->teilnehmer & (1u << h)) k++;
        const char *g = f->gastgeber >= 0 ? (f->gastgeber2 >= 0 ? tmp("%s & %s", HEINI_NAMEN[f->gastgeber], HEINI_NAMEN[f->gastgeber2]) : HEINI_NAMEN[f->gastgeber]) : "auswärts";
        if (ui_eintrag(-1, tmp("Nr. %d · %s", f->nr, f->name), tmp("%s %d · %d %s · %d Leute · Gastgeber: %s", MONAT_NAME[(f->monat % 100 + 11) % 12], jahr, f->tage, f->tage == 1 ? "Tag" : "Tage", k, g), NULL)) {
            char tn[300] = ""; int pos = 0;
            for (int h = 0; h < HEINI_NAMEN_N; h++) if (f->teilnehmer & (1u << h)) pos += snprintf(tn + pos, sizeof tn - pos, "%s%s", pos ? ", " : "", HEINI_NAMEN[h]);
            meldung(tmp("Dabei: %s", tn));
        }
    }
    if (jahr >= 0) ui_karte_ende();
    ui_bereich_ende();
    fuss("A: Teilnehmer zeigen   B: zurück");
}

/* ======================================================================= Einstellungen */
static void seite_einst(void) {
    gfx_fuellen(C_HG);
    kopf("Einstellungen", 0, 0);
    ui_bereich(64, FUSS_Y);
    ui_karte(S_UHR " Uhr", C_FLAECHE2);
    ui_text(tmp("%s, %s Uhr", datum_lang(tag_von(jetzt)), uhr_text(jetzt)), C_TEXT);
    ui_text(uhr_gestellt == 2 ? "Die Zeit kommt von der Echtzeituhr des Moduls." : "Ohne Echtzeituhr im Modul läuft die Uhr ab dem Stellen weiter, nach dem Ausschalten bitte neu stellen.", C_TEXT2);
    if (ui_knopf("Uhr stellen")) uhr_dialog(ID_UHR, jetzt);
    int32_t s;
    if (uhr_ergebnis(ID_UHR, &s)) { uhr_setzen(s); if (uhr_gestellt != 2) uhr_gestellt = 1; Aktion *a = aktion_neu(A_UHR); a->profil = -1; a->zeit = s; tu(a); nochmal_zeichnen = 1; }
    ui_karte_ende();
    ui_karte("Regeln", C_FLAECHE2);
    ui_zeile("Vorschlag-Dauer fürs Bad", tmp("%d Minuten", regel_dauer_bad), C_TEXT);
    ui_zeile("Verschleißpauschale (Fahrten)", regel_verschleiss ? tmp("an, %d ct/km", regel_verschleiss_ct) : "aus", C_TEXT);
    if (ich >= 0 && ui_knopf("Bad-Dauer ändern")) {
        static const char *const D[8] = { "5 Minuten", "10 Minuten", "15 Minuten", "20 Minuten", "25 Minuten", "30 Minuten", "45 Minuten", "60 Minuten" };
        auswahl(ID_REGEL, "Vorschlag-Dauer fürs Bad", D, 8, 2);
    }
    int w = auswahl_ergebnis(ID_REGEL);
    if (w >= 0) { static const int M[8] = { 5, 10, 15, 20, 25, 30, 45, 60 }; Aktion *a = aktion_neu(A_REGEL); a->a = (int16_t)M[w]; fehler(tu(a)); }
    ui_karte_ende();
    ui_karte("Speicher auf dem Modul", C_FLAECHE2);
    if (journal_da()) {
        ui_zeile("Änderungen am Fernseher", tmp("%d", journal_eintraege()), C_TEXT);
        ui_zeile("Frei", tmp("%d von %d Byte", journal_frei(), SPEICHER_GROESSE - 24), C_TEXT);
        ui_balken(1000 - journal_frei() * 1000 / (SPEICHER_GROESSE - 24), C_BLAU);
        if (ui_knopf("Alle Änderungen am Fernseher löschen")) frage(ID_JOURNAL_WEG, "Wirklich alles löschen, was am Fernseher eingetragen wurde? Zurück bleibt der Stand aus dem Export.", "Löschen", "Abbrechen");
        if (frage_ergebnis(ID_JOURNAL_WEG) == 0) { journal_loeschen(); journal_laden(); meldung("Gelöscht. Stand wie beim Export."); }
    } else ui_text("Kein SRAM gefunden. Änderungen gelten nur bis zum Ausschalten. Auf Flashcarts den Speichertyp „SRAM 256k“ wählen.", C_GELB);
    ui_karte_ende();
    ui_karte("Über die App", C_FLAECHE2);
    ui_text("Heini-App 64 – die Heini-App 1.61.0 als stationäre App am Fernseher.", C_TEXT);
    ui_text(tmp("Daten aus dem Export vom %s (Datenstand %u).", EXPORT_STAND, (unsigned)EXPORT_KENNUNG), C_TEXT2);
    ui_text("Ohne Internet: kein Abgleich mit dem Pi, keine Push-Nachrichten, keine HeinAI, keine Spielesuche, keine Fotos und Sprachnachrichten.", C_TEXT2);
    if (ONEDRIVE_LINK[0]) ui_text(tmp("Fotos und Videos: %s", ONEDRIVE_LINK), C_TEXT2);
    ui_karte_ende();
    ui_bereich_ende();
    fuss("A: wählen   B: zurück");
}

/* ======================================================================= Klopapier-Alarm (Vollbild) */
static void seite_alarm(int al) {
    const Alarm *a = &db.alarme.v[al];
    gfx_fuellen((plat_ms() / 500) & 1 ? C_ROT : RGB(0x9a, 0x20, 0x24));
    gfx_text_mitte(BILD_B / 2, 60, S_KLOPAPIER " KLOPAPIER-ALARM " S_KLOPAPIER, C_WEISS, 1, 3);
    avatar(a->profil, BILD_B / 2 - 24, 140, 48);
    gfx_text_mitte(BILD_B / 2, 210, tmp("%s braucht Klopapier!", pname(a->profil)), C_WEISS, 1, 2);
    gfx_text_mitte(BILD_B / 2, 250, tmp("im %s · %s", a->raum >= 0 ? db.raeume.v[a->raum].name : "Bad", wann_text(a->ausgeloest, jetzt)), C_WEISS, 0, 1);
    ui_bereich(300, FUSS_Y);
    extern void ui_mitte(int an);
    ui_mitte(1);
    if (ui_knopf_farbe("Ich bring welche", C_GRUEN, C_WEISS)) { Aktion *x = aktion_neu(A_ALARM_BRING); x->a = (int16_t)al; if (!fehler(tu(x))) meldung("Held des Tages!"); alarm_gesehen = al; seite_zurueck(); }
    else if (ui_knopf_neben("Später")) { alarm_gesehen = al; seite_zurueck(); }
    ui_mitte(0);
    ui_bereich_ende();
    nochmal_zeichnen = 1;
}

/* ======================================================================= Rahmen */
static void menue(void) {
    static const char *const M[7] = { S_HERZ " Chat", S_POKAL " Ruhmeshalle", "Wer ist da?", "Mein Profil", S_PAD " Einstellungen", "Profil wechseln", "Zurück" };
    auswahl(ID_MENUE, tmp("Menü · %s", pname(ich)), M, 7, 0);
}
static void menue_ergebnis(void) {
    int w = auswahl_ergebnis(ID_MENUE);
    switch (w) {
    case 0: chat_ende = 1; seite_auf(SE_CHAT, 0); break;
    case 1: ruhm_reiter = 0; seite_auf(SE_RUHM, 0); break;
    case 2: seite_auf(SE_WERDA, 0); break;
    case 3: seite_auf(SE_PROFIL, ich); break;
    case 4: seite_auf(SE_EINST, 0); break;
    case 5: tiefe = 1; seite_ersetzen(SE_PROFILWAHL, 0); break;
    }
}

void seiten_start(void) { tiefe = 0; logo_start = (int)plat_ms(); seite_auf(SE_LOGO, 0); plan_tag = tag_von(jetzt); }

void seiten_zeichnen(void) {
    int s = seite_aktuell();
    uint32_t t = ui_taste();
    /* Klopapier-Alarm springt für Anwesende als Vollbild auf */
    int l = laufendes_treffen();
    if (ich >= 0 && l >= 0 && s != SE_ALARM && s != SE_PROFILWAHL && s != SE_LOGO && !ui_dialog_offen()) {
        int al = offener_alarm(l);
        if (al >= 0 && al != alarm_gesehen && db.alarme.v[al].profil != ich && ist_anwesend(l, ich)) { seite_auf(SE_ALARM, al); return; }
    }
    if (s == SE_ALARM && (param() >= db.alarme.n || db.alarme.v[param()].erledigt_um || db.alarme.v[param()].geloescht)) { seite_zurueck(); return; }
    switch (s) {
    case SE_LOGO: seite_logo(); break;
    case SE_PROFILWAHL: seite_profilwahl(); break;
    case SE_HAUPT: {
        if (t & (T_L | T_R | T_C_LINKS | T_C_RECHTS)) {
            if (!(reiter == R_PLAN && (t & (T_C_LINKS | T_C_RECHTS)))) {
                reiter = (reiter + ((t & (T_R | T_C_RECHTS)) ? 1 : 4)) % 5;
                ui_seite_neu(); nochmal_zeichnen = 1;
            }
        }
        gfx_fuellen(C_HG);
        static const char *const TITEL[5] = { "Heute", "Plan", "Kasse & Trips", "Treffen", "Aktivitäten & Ideen" };
        kopf(TITEL[reiter], 1, reiter);
        ui_bereich(KOPF_H + 2, FUSS_Y);
        switch (reiter) {
        case R_HEUTE: seite_heute(); break;
        case R_PLAN: seite_plan(); break;
        case R_KASSE: seite_kasse(); break;
        case R_TREFFEN: seite_treffenliste(); break;
        case R_IDEEN: seite_ideen(); break;
        }
        ui_bereich_ende();
        fuss("A: wählen   L/R: Reiter   Z: Chat   START: Menü   C" S_HOCH S_RUNTER ": rollen");
        if (t & T_START) menue();
        if (t & T_Z) { chat_ende = 1; seite_auf(SE_CHAT, 0); }
        menue_ergebnis();
        return;
    }
    case SE_CHAT: seite_chat(param()); break;
    case SE_RUHM: seite_ruhm(); break;
    case SE_PROFIL: seite_profil(param()); break;
    case SE_TREFFEN:
        gfx_fuellen(C_HG);
        kopf("Treffen", 0, 0);
        ui_bereich(64, FUSS_Y);
        seite_treffen(param());
        ui_bereich_ende();
        fuss("A: wählen   B: zurück   START: Menü");
        break;
    case SE_FRUEHER: seite_frueher(); break;
    case SE_EINST: seite_einst(); break;
    case SE_WERDA: seite_werda(); break;
    case SE_ALARM: seite_alarm(param()); return;
    }
    if (s != SE_LOGO && s != SE_PROFILWAHL) {
        if ((t & T_B) && tiefe > 1) seite_zurueck();
        if (t & T_START) menue();
        menue_ergebnis();
    }
}
