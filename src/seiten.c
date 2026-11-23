/* seiten.c – Rahmen, Profilwahl und die fünf Reiter (Heute, Plan, Kasse & Trips, Treffen, Aktivitäten & Ideen),
 * Treffen-Seite, „Mein Heini-Treffen“ und Klopapier-Alarm im Aussehen der Heini-App */
#include "seiten_intern.h"

int nochmal_zeichnen;
int uhr_gestellt;
zeit_t uhr_basis; uint32_t uhr_basis_ms;
int reiter = R_HEUTE, chat_ende = 1, ruhm_reiter, ruhm_person = -1, kasse_ansicht, ideen_reiter = 1;

typedef struct { int seite, param; } Ebene;
static Ebene stapel[16];
static int tiefe;
static int plan_tag, alarm_gesehen = -1, logo_start;
int treffen_menue_code[4];
int kosten_trip_wahl = -1;

void uhr_setzen(zeit_t z) { uhr_basis = z; uhr_basis_ms = plat_ms(); jetzt = z; }
int seite_aktuell(void) { return tiefe ? stapel[tiefe - 1].seite : SE_LOGO; }
int param(void) { return tiefe ? stapel[tiefe - 1].param : 0; }
void seite_auf(int s, int p) { if (tiefe < 16) stapel[tiefe++] = (Ebene){ s, p }; ui_seite_neu(); nochmal_zeichnen = 1; }
void seite_zurueck(void) { if (tiefe > 1) tiefe--; ui_seite_neu(); nochmal_zeichnen = 1; }
static void seite_ersetzen(int s, int p) { if (tiefe) tiefe--; seite_auf(s, p); }

int fehler(const char *f) {
    nochmal_zeichnen = 1;
    if (!f) return 0;
    meldung(f[0] == '?' ? f + 1 : f);
    return 1;
}
int mach(Aktion *a, const char *ok) { const char *f = tu(a); if (fehler(f)) return 0; if (ok) meldung(ok); return 1; }
const char *art_text(int a) { return a >= 0 && a < AUSGABE_ARTEN_N ? AUSGABE_ARTEN[a].text : "Ausgabe"; }
const char *zusage_text(int z) { return z == ZUSAGE_KOMME ? "Komme" : z == ZUSAGE_VIELLEICHT ? "Vielleicht" : z == ZUSAGE_NEIN ? "Komme nicht" : "Noch offen"; }
const char *tage_bis(tag_t t) {
    int d = t - tag_von(jetzt);
    return d == 0 ? "heute" : d == 1 ? "morgen" : d > 1 ? tmp("in %d Tagen", d) : d == -1 ? "gestern" : tmp("vor %d Tagen", -d);
}
const char *naechte_text(int n) { return n == 1 ? "1 Nacht" : tmp("%d Nächte", n); }
int seiten_kopf(const char *titel, int extra) {
    gfx_fuellen(TH->bg);
    int k = kopf_seite(titel, extra);
    if (k == KOPF_ZURUECK) seite_zurueck();
    ui_bereich(KOPF_SEITE, BILD_H);
    return k;
}
/* ganze Karte als fokussierbare Fläche (nach dem Zeichnen) */
static int karte_als_knopf(int ys) {
    int ye = ui_y() - 12;
    int an = ui_kachel_knopf(RAND_X, ys, INHALT_B, ye - ys);
    if (ui_fokussiert_letztes()) ui_fokus_rahmen(RAND_X, ys, INHALT_B, ye - ys, 16);
    return an;
}

/* ======================================================================= Logo, Profilwahl */
static void seite_logo(void) {
    gfx_fuellen(RGB(0x70, 0x0c, 0x15));
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            uint16_t c = logo128[y * 128 + x];
            int a = c & 15;
            if (!a) continue;
            int r = c >> 12, g = (c >> 8) & 15, b = (c >> 4) & 15;
            uint16_t f = (uint16_t)(((r << 1 | r >> 3) << 11) | ((g << 1 | g >> 3) << 6) | ((b << 1 | b >> 3) << 1) | 1);
            gfx_rechteck(BILD_B / 2 - 96 + x * 3 / 2, 70 + y * 3 / 2, 2, 2, f);
        }
    text_mitte(BILD_B / 2, 290, "Heini-App 64", F_GROSS, C_WEISS);
    text_mitte(BILD_B / 2, 336, "Bad, Klo, Schlafplatz und Chat beim Freundestreffen", F_NORMAL, RGB(0xf5, 0xec, 0xec));
    if ((ui_taste() & (T_A | T_START)) || plat_ms() - (uint32_t)logo_start > 2200) seite_ersetzen(SE_PROFILWAHL, 0);
    nochmal_zeichnen = 1;
}

static void seite_profilwahl(void) {
    ich = -1;
    thema_setzen(4, design_dunkel);
    gfx_fuellen(TH->bg);
    gfx_rechteck(0, 0, BILD_B, KOPF_SEITE, TH->oben);
    text(RAND_X - 6, 13, "Heini-App 64", F_KOPF, TH->onOben);
    text_rechts(BILD_B - RAND_X, 20, tmp("%s · %s", datum_text(tag_von(jetzt)), uhr_text(jetzt)), F_FETT, TH->onOben);
    ui_bereich(KOPF_SEITE, BILD_H);
    text(ui_x(), ui_y(), "Wer bist du?", F_KOPF, TH->text);
    ui_abstand(40);
    int l = laufendes_treffen(), n = naechstes_treffen();
    if (l >= 0) ui_symbolzeile(IC_MAP_PIN, tmp("Läuft gerade: %s · %s", treffen_titel(l), zeitraum_text(db.treffen.v[l].von, db.treffen.v[l].bis)), TH->accentText);
    else if (n >= 0) ui_symbolzeile(IC_CALENDAR, tmp("Nächstes Treffen %s: %s", tage_bis(db.treffen.v[n].von), treffen_titel(n)), TH->muted);
    int sp = 5, kb = INHALT_B / sp, y0 = ui_y(), k = 0, gewaehlt = -1;
    for (int p = 0; p < db.profile.n; p++) {
        if (!db.profile.v[p].aktiv) continue;
        int x = RAND_X + (k % sp) * kb, y = y0 + (k / sp) * 118;
        int an = ui_kachel_knopf(x + 4, y, kb - 8, 110), f = ui_fokussiert_letztes();
        if (f) ui_fokus_rahmen(x + 4, y, kb - 8, 110, 16);
        gfx_runde_box(x + 4, y, kb - 8, 110, 16, f ? TH->flaeche2 : TH->flaeche);
        avatar(p, x + kb / 2 - 20, y + 14, 40);
        text_mitte(x + kb / 2, y + 70, tmp("%.12s", pname(p)), F_FETT, TH->text);
        if (l >= 0 && ist_anwesend(l, p)) text_mitte(x + kb / 2, y + 89, "ist da", F_KLEIN, TH->ok);
        if (an) gewaehlt = p;
        k++;
    }
    ui_weiter(((k + sp - 1) / sp) * 118 + 6);
    if (!uhr_gestellt) ui_symbolzeile(IC_CLOCK, "Diese Konsole hat keine Uhr. Bitte einmal Tag und Uhrzeit stellen.", TH->warn);
    if (ui_knopf_art(tmp("Uhr stellen · %s %s", datum_text(tag_von(jetzt)), uhr_text(jetzt)), KN_TONAL, IC_CLOCK)) uhr_dialog(ID_UHR, jetzt);
    if (ui_knopf_neben_art("Einstellungen", KN_TONAL, IC_SETTINGS)) seite_auf(SE_EINST, 0);
    if (ui_knopf_neben_art("Tour", KN_TONAL, IC_BOOK_OPEN)) seite_auf(SE_TOUR, 0);
    int32_t s;
    if (uhr_ergebnis(ID_UHR, &s)) {
        uhr_setzen(s); uhr_gestellt = 1;
        Aktion *a = aktion_neu(A_UHR); a->profil = -1; a->zeit = s; tu(a);
        meldung(tmp("Uhr gestellt: %s, %s", datum_lang(tag_von(s)), uhr_text(s)));
    }
    ui_text(tmp("Daten aus dem Export vom %s · %d Änderungen am Fernseher", EXPORT_STAND, journal_eintraege()), F_KLEIN, TH->faint);
    ui_bereich_ende();
    if (gewaehlt >= 0) { ich = gewaehlt; reiter = R_HEUTE; alarm_gesehen = -1; seite_auf(SE_HAUPT, 0); meldung(tmp("Hallo %s!", pname(gewaehlt))); }
}

/* ======================================================================= Heute */
static void karte_lauf(int t, int b) {
    const Buchung *x = &db.buchungen.v[b];
    int sek = (int)(jetzt - x->start), plan = regel_dauer_bad * 60;
    ui_karte_art(KA_AKZENT, NULL, -1);
    text(ui_x(), ui_y(), "LÄUFT GERADE", F_FETT, TH->onAccentCont);
    ui_abstand(24);
    text_mitte(BILD_B / 2, ui_y(), tmp("Im %s", db.raeume.v[x->raum].name), F_GROSS, TH->onAccentCont);
    ui_abstand(42);
    text_mitte(BILD_B / 2, ui_y(), x->partner >= 0 && !x->partner_bis ? tmp("seit %s · du und %s · alle Angekommenen sehen eure Uhr", uhr_text(x->start), pname(x->partner))
                                 : tmp("seit %s · alle Angekommenen sehen deine Uhr", uhr_text(x->start)), F_KLEIN, TH->onAccentCont);
    ui_abstand(26);
    int cy = ui_y() + 82, cx = BILD_B / 2;
    int anteil = sek * 1000 / (plan > 0 ? plan : 1);
    ui_ring(cx, cy, 84, 12, anteil > 1000 ? 1000 : anteil, sek > plan ? TH->err : TH->accent, gfx_mischen(TH->accentCont, TH->onAccentCont, 50), 40);
    const char *zt = dauer_text(sek);
    if (text_breite(zt, F_UHR) <= 132) text_mitte(cx, cy - 34, zt, F_UHR, TH->onAccentCont);
    else text_mitte(cx, cy - 20, zt, F_GROSS, TH->onAccentCont);
    text_mitte(cx, cy + 26, sek > plan ? tmp("%d Min drüber", (sek - plan) / 60) : "läuft offen", F_KLEIN, TH->onAccentCont);
    int px = cx + 128, py = cy - 10;
    int an = ui_kachel_knopf(px - 30, py - 30, 60, 60), fo = ui_fokussiert_letztes();
    if (fo) ui_fokus_rahmen(px - 30, py - 30, 60, 60, 30);
    gfx_kreis(px, py, 30, fo ? gfx_mischen(TH->alarm, C_WEISS, 50) : TH->alarm);
    gfx_emoji(emoji_index(0x1F9FB), px - 18, py - 18, 36);
    text_mitte(px, py + 36, "Papier leer?", F_KLEIN, TH->onAccentCont);
    if (an) { Aktion *a = aktion_neu(A_ALARM); a->a = (int16_t)t; mach(a, "Alarm ausgelöst! Alle Anwesenden sehen ihn."); }
    ui_abstand(178);
    if (sek > plan) ui_text(tmp("„%s“", spruch_ueberzogen(b)), F_NORMAL, TH->onAccentCont);
    if (sek >= NOCH_DRIN * 60) ui_text("Bist du noch drin? Spätestens nach 60 Minuten endet die Uhr von selbst.", F_FETT, TH->onAccentCont);
    static const char *const DAUER[2] = { "Dauert nicht lange", "Dauert länger" };
    int w = ui_segment(DAUER, 2, x->lang ? 1 : 0);
    if (w >= 0 && w != x->lang) { Aktion *a = aktion_neu(A_LANG); a->a = (int16_t)b; a->b = (int16_t)w; mach(a, NULL); }
    int q = partner_von(ich);
    if (q >= 0 && x->profil == ich) {
        int mit = x->partner == q && !x->partner_bis;
        if (ui_knopf_art(tmp("Als Pärchen mit %s", pname(q)), mit ? KN_VOLL : KN_RAND, mit ? IC_CHECK : IC_USERS)) { Aktion *a = aktion_neu(A_PARTNER); a->a = (int16_t)b; a->b = !mit; mach(a, NULL); }
    } else if (x->profil != ich && ui_knopf_art("Bin nicht drin", KN_RAND, IC_USER_X)) { Aktion *a = aktion_neu(A_PARTNER); a->a = (int16_t)b; a->b = 0; mach(a, NULL); }
    if (ui_knopf_breit("Bad jetzt freigeben", KN_VOLL, IC_CHECK)) {
        Aktion *a = aktion_neu(A_FERTIG); a->a = (int16_t)b;
        if (mach(a, NULL)) { int d = dran_in(x->raum, t); meldung(d >= 0 ? tmp("Bad ist frei. %s ist dran!", pname(db.warteliste.v[d].profil)) : "Bad ist frei. Danke!"); }
    }
    ui_karte_ende();
}

static void karte_bad(int t, int raum) {
    const Raum *r = &db.raeume.v[raum];
    int b = drin_in(raum, t), dran = dran_in(raum, t), warte[16], nw = schlange(raum, t, warte, 16);
    int anw = ist_anwesend(t, ich), meine = meine_uhr(ich), wp = warte_platz(ich, raum, t);
    ui_karte_art(b >= 0 ? KA_ERR : KA_OK, NULL, -1);
    uint16_t farbe = b >= 0 ? TH->err : TH->ok;
    text_max(ui_x(), ui_y() - 2, r->name, ui_breite() - 30, F_GROSS, farbe);
    if (r->info && r->info[0]) gfx_symbol(IC_INFO, ui_x() + ui_breite() - 22, ui_y() + 8, 20, farbe);
    ui_abstand(40);
    if (b >= 0) {
        const Buchung *x = &db.buchungen.v[b];
        const char *wer = x->partner >= 0 && !x->partner_bis ? tmp("%s und %s sind drin", pname(x->profil), pname(x->partner)) : tmp("%s ist drin", pname(x->profil));
        ui_text(tmp("%s · %s", wer, x->lang ? "dauert länger" : "dauert nicht lange"), F_FETT, farbe);
        gfx_symbol(IC_CLOCK, ui_x(), ui_y() + 16, 28, farbe);
        text(ui_x() + 38, ui_y(), dauer_text((int)(jetzt - x->start)), F_UHR, farbe);
        ui_abstand(66);
    } else ui_text(dran >= 0 ? tmp("ist frei · %s ist dran", pname(db.warteliste.v[dran].profil)) : "ist frei", F_FETT, farbe);
    if (nw) {
        gfx_symbol(IC_CLOCK, ui_x(), ui_y(), 20, TH->text);
        text(ui_x() + 28, ui_y(), "Danach wollen", F_FETT, TH->text);
        ui_abstand(28);
        for (int i = 0; i < nw; i++) {
            const Warte *w = &db.warteliste.v[warte[i]];
            int y = ui_y();
            text(ui_x(), y + 6, tmp("%d.", i + 1), F_FETT, TH->text);
            avatar(w->profil, ui_x() + 24, y, 28);
            text(ui_x() + 60, y + 6, w->partner >= 0 ? tmp("%s & %s", pname(w->profil), pname(w->partner)) : pname(w->profil), w->profil == ich ? F_FETT : F_NORMAL, TH->text);
            ui_abstand(34);
        }
    }
    if (anw && meine < 0 && b < 0) {
        if (ui_knopf_breit("Rein", KN_VOLL, IC_PLAY)) {
            Aktion *a = aktion_neu(A_REIN); a->a = (int16_t)t; a->b = (int16_t)raum;
            const char *f = tu(a);
            if (f && f[0] == '?') frage(ID_TROTZDEM * 100 + raum, f + 1, "Trotzdem rein", "Lieber warten");
            else if (!fehler(f)) meldung(tmp("Viel Spaß im %s! Die Uhr läuft.", r->name));
        }
    }
    if (anw && meine < 0 && b >= 0 && wp < 0) {
        if (ui_knopf_breit("Ich will danach", KN_ERR, IC_USERS)) { Aktion *a = aktion_neu(A_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; mach(a, "Du stehst an. Wer vorne steht, bleibt vorne."); }
        int q = partner_von(ich);
        if (q >= 0 && ist_anwesend(t, q) && ui_knopf_art(tmp("Mit %s anstellen", pname(q)), KN_RAND, IC_USERS)) { Aktion *a = aktion_neu(A_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; a->c = 1; mach(a, NULL); }
    }
    if (wp >= 0 && ui_knopf_art("Nicht mehr warten", KN_RAND, IC_X)) { Aktion *a = aktion_neu(A_NICHT_WARTEN); a->a = (int16_t)t; a->b = (int16_t)raum; mach(a, NULL); }
    if (r->info && r->info[0]) ui_text(r->info, F_KLEIN, TH->muted);
    if (frage_ergebnis(ID_TROTZDEM * 100 + raum) == 0) { Aktion *a = aktion_neu(A_REIN); a->a = (int16_t)t; a->b = (int16_t)raum; a->c = 1; mach(a, "Vorgedrängelt! Die Uhr läuft."); }
    ui_karte_ende();
}

static void karte_alarm(int t) {
    int al = offener_alarm(t);
    if (al < 0) return;
    const Alarm *a = &db.alarme.v[al];
    ui_karte_art(KA_ALARM, NULL, -1);
    gfx_emoji(emoji_index(0x1F9FB), ui_x(), ui_y(), 36);
    text(ui_x() + 48, ui_y() + 2, "Klopapier-Alarm!", F_TITEL, TH->err);
    text_max(ui_x() + 48, ui_y() + 26, tmp("%s: %s braucht Klopapier · %s", a->raum >= 0 ? db.raeume.v[a->raum].name : "Bad", pname(a->profil), uhr_text(a->ausgeloest)), ui_breite() - 48, F_NORMAL, TH->text);
    ui_abstand(56);
    if (ist_anwesend(t, ich) && ui_knopf_art("Ich bring welche", KN_GEFAHR, -1)) { Aktion *x = aktion_neu(A_ALARM_BRING); x->a = (int16_t)al; mach(x, "Held des Tages!"); }
    if (a->profil == ich && ui_knopf_neben_art("Abbrechen", KN_RAND, -1)) { Aktion *x = aktion_neu(A_ALARM_ABBRUCH); x->a = (int16_t)al; mach(x, NULL); }
    ui_karte_ende();
}

/* Goldkarte „Mein Heini-Treffen“ */
static void karte_mein(int t) {
    int ys = ui_y();
    ui_karte_art(KA_GOLD, "Mein Heini-Treffen", IC_SPARKLES);
    gfx_symbol(IC_CHEVRON_RIGHT, ui_x() + ui_breite() - 16, ui_y() - 36, 20, TH->goldText);
    tag_t h = tag_von(jetzt);
    const Treffen *tr = &db.treffen.v[t];
    tag_t nacht = h >= tr->von && h < tr->bis ? h : tr->von;
    if (ist_gastgeber(t, ich)) ui_symbolzeile(IC_HOUSE, "Du bist Gastgeber", TH->text);
    else if (tr->bis > tr->von) {
        int s = schlaf_von(t, ich, nacht);
        ui_symbolzeile(IC_BED, s >= 0 ? tmp("%s · Nacht auf %s", db.plaetze.v[db.schlaf.v[s].platz].name, WOCHENTAG_KURZ[wochentag(nacht + 1)])
                                       : tmp("Noch kein Platz · Nacht auf %s", WOCHENTAG_KURZ[wochentag(nacht + 1)]), TH->text);
    }
    for (int f = 0; f < db.fahrten.n; f++) {
        if (db.fahrten.v[f].geloescht || db.fahrten.v[f].treffen != t || db.fahrten.v[f].richtung) continue;
        int ins[12], ni = fahrt_insassen(f, ins, 12), drin = 0;
        for (int i = 0; i < ni; i++) if (ins[i] == ich) drin = 1;
        if (!drin) continue;
        int trp = db.fahrten.v[f].trip;
        char mit[120] = "";
        for (int i = 0; i < ni; i++) if (ins[i] != ich) { if (mit[0]) strncat(mit, ", ", sizeof mit - strlen(mit) - 1); strncat(mit, pname(ins[i]), sizeof mit - strlen(mit) - 1); }
        ui_symbolzeile(IC_CAR, tmp("%s · %s%s%s", trp >= 0 ? db.trips.v[trp].titel : "Fahrt", db.fahrten.v[f].profil == ich ? "du fährst" : "", mit[0] ? (db.fahrten.v[f].profil == ich ? " · mit " : "mit ") : "", mit), TH->text);
    }
    int n = 0;
    for (int u = 0; u < db.utensilien.n; u++) { int i = ausleihe_von(t, u, ich); if (i >= 0) { ui_symbolzeile(IC_PACKAGE, tmp("%d× %s geliehen", db.ausleihen.v[i].anzahl, db.utensilien.v[u].name), TH->text); n++; } }
    if (!n && !ist_gastgeber(t, ich)) ui_symbolzeile(IC_PACKAGE, "Nichts geliehen", TH->muted);
    ui_karte_ende();
    if (karte_als_knopf(ys)) seite_auf(SE_MEIN, t);
}

static void karte_zusage(int t) {
    if (ist_gastgeber(t, ich)) { ui_symbolzeile(IC_HOUSE, "Du bist Gastgeber. Du kommst sowieso.", TH->muted); return; }
    static const int ST[3] = { ZUSAGE_KOMME, ZUSAGE_VIELLEICHT, ZUSAGE_NEIN };
    static const char *const TX[3] = { "Komme", "Vielleicht", "Nein" };
    int z = zusage_von(t, ich), aktiv = z == ZUSAGE_KOMME ? 0 : z == ZUSAGE_VIELLEICHT ? 1 : z == ZUSAGE_NEIN ? 2 : -1;
    int w = ui_segment(TX, 3, aktiv);
    if (w >= 0 && w != aktiv) { Aktion *a = aktion_neu(A_ZUSAGE); a->a = (int16_t)t; a->b = (int16_t)ST[w]; mach(a, w == 0 ? "Super! Das Klo freut sich." : w == 1 ? "Vielleicht ist besser als nein." : "Schade! Nächstes Mal."); }
}

static void karte_offenes(void) {
    int n = 0, pz = 0;
    for (int u = db.umfragen.n - 1; u >= 0; u--) {
        int m = db.umfragen.v[u].nachricht;
        if (m < 0 || db.nachrichten.v[m].geloescht || umfrage_zu(u) || stimme_von(u, ich) >= 0) continue;
        n++;
    }
    for (int i = 0; i < db.paare.n; i++) if (!db.paare.v[i].weg && db.paare.v[i].status == PAAR_OFFEN && db.paare.v[i].zu == ich) pz++;
    if (!n && !pz) return;
    ui_karte("Offenes für dich", IC_CLIPBOARD_LIST);
    if (n && ui_eintrag_symbol(IC_BAR_CHART, NULL, n == 1 ? "1 Umfrage wartet auf deine Stimme" : tmp("%d Umfragen warten auf deine Stimme", n), NULL, "›")) { chat_ende = 1; seite_auf(SE_CHAT, 0); }
    if (pz && ui_eintrag_symbol(IC_HEART, NULL, "Jemand möchte mit dir ein Pärchen sein", "Im Profil antworten", "›")) seite_auf(SE_PROFIL, ich + 1000);
    ui_karte_ende();
}

static void seite_heute(void) {
    tag_t h = tag_von(jetzt);
    ui_text(tmp("„%s“", weisheit_des_tages(h)), F_NORMAL, TH->muted);
    int l = laufendes_treffen();
    if (l >= 0) {
        int b = meine_uhr(ich);
        if (b >= 0) karte_lauf(l, b);
        karte_alarm(l);
        if (!ist_anwesend(l, ich)) {
            ui_karte(treffen_titel(l), IC_MAP_PIN);
            int an = anwesenheit_von(l, ich);
            if (an >= 0 && db.anwesenheit.v[an].abgereist) { ui_text("Du bist abgereist", F_GROSS, TH->text); ui_text("Schön war's! Gute Heimfahrt.", F_NORMAL, TH->muted); }
            else { ui_text("Heute geht's los!", F_GROSS, TH->text); ui_text("Sag Bescheid, wenn du da bist. Dann kannst du ins Bad.", F_NORMAL, TH->muted); }
            if (ui_knopf_breit("Bin angekommen", KN_VOLL, -1)) { Aktion *a = aktion_neu(A_ANKUNFT); a->a = (int16_t)l; mach(a, "Willkommen! Der Thron bebt."); }
            ui_karte_ende();
        }
        if (hat_komme(l, ich) || ist_anwesend(l, ich)) karte_mein(l);
        int bs[16], nb = baeder(l, bs, 16);
        for (int i = 0; i < nb; i++) karte_bad(l, bs[i]);
        if (!nb) { ui_karte("Bäder", IC_BATH); ui_text("Für dieses Treffen sind keine Bäder eingetragen. Der Gastgeber trägt sie im Profil unter „Gastgeber“ ein.", F_NORMAL, TH->muted); ui_karte_ende(); }
        int32_t stand[MAX_PROFILE]; uint32_t bet; int32_t summe = kassen_stand(l, stand, &bet);
        if (summe && (bet & (1u << ich))) {
            int32_t c = stand[ich];
            ui_karte("Kasse", IC_EURO);
            ui_zeile(c > 0 ? "Du bekommst" : c < 0 ? "Du zahlst" : "Du bist quitt", c ? euro_text(c < 0 ? -c : c) : "", c > 0 ? TH->ok : c < 0 ? TH->err : TH->text);
            if (ui_eintrag_symbol(IC_EURO, NULL, "Zur Kasse", tmp("Ausgaben insgesamt %s", euro_text(summe)), "›")) { reiter = R_KASSE; ui_seite_neu(); }
            ui_karte_ende();
        }
        if (ist_anwesend(l, ich)) {
            int da[MAX_PROFILE], n = 0;
            for (int p = 0; p < db.profile.n; p++) if (ist_anwesend(l, p)) da[n++] = p;
            ui_karte(tmp("Wer ist da? (%d)", n), IC_USERS);
            ui_avatare(da, n);
            if (ui_knopf_art("Jemanden einchecken", KN_RAND, IC_USER_CHECK)) {
                static const char *namen[MAX_PROFILE]; int k = 0;
                for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv && !ist_anwesend(l, p)) namen[k++] = pname(p);
                if (k) auswahl(ID_EINCHECKEN, "Wen einchecken?", namen, k, 0); else meldung("Alle sind schon da.");
            }
            int w = auswahl_ergebnis(ID_EINCHECKEN);
            if (w >= 0) {
                int k = 0, wer = -1;
                for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv && !ist_anwesend(l, p)) { if (k == w) wer = p; k++; }
                Aktion *a = aktion_neu(A_EINCHECKEN_FUER); a->a = (int16_t)l; a->b = (int16_t)wer; mach(a, NULL);
            }
            ui_karte_ende();
        }
    } else {
        for (int i = 0; i < db.treffen.n; i++) if (treffen_status(i) == T_UEBERFAELLIG) {
            ui_karte_art(KA_WARN, NULL, -1);
            ui_text("Bitte beenden", F_TITEL, TH->warn);
            ui_text(tmp("%s ist vorbei. Erst beenden, dann ab in die Statistik.", treffen_titel(i)), F_NORMAL, TH->text);
            if (ui_knopf_art("Zum Treffen", KN_VOLL, -1)) seite_auf(SE_TREFFEN, i);
            ui_karte_ende();
        }
        int n = naechstes_treffen();
        if (n >= 0) {
            const Treffen *t = &db.treffen.v[n];
            ui_karte(tmp("Nächstes Treffen %s", tage_bis(t->von)), IC_MAP_PIN);
            ui_text(treffen_titel(n), F_GROSS, TH->text);
            ui_symbolzeile(IC_CALENDAR, tmp("%s · %s", zeitraum_text(t->von, t->bis), naechte_text(treffen_naechte(n))), TH->muted);
            ui_symbolzeile(IC_HOUSE, t->gastgeber >= 0 ? tmp("bei %s · %s", pname(t->gastgeber), treffen_adresse(n)) : tmp("auswärts · %s", treffen_adresse(n)), TH->muted);
            karte_zusage(n);
            if (ui_knopf_art("Zum Treffen", KN_TONAL, IC_CHEVRON_RIGHT)) seite_auf(SE_TREFFEN, n);
            ui_karte_ende();
        } else {
            ui_karte("Kein Treffen geplant", IC_CALENDAR);
            ui_text("Gerade ist kein Treffen eingetragen.", F_NORMAL, TH->muted);
            if (ui_knopf_art("Neues Treffen", KN_VOLL, IC_PLUS)) { treffen_form_neu(-1); seite_auf(SE_TREFFEN_FORM, -1); }
            ui_karte_ende();
        }
        int nach = nach_treffen(ich);
        if (nach >= 0) {
            ui_karte(tmp("Nach dem Treffen · %s", treffen_titel(nach)), IC_TROPHY);
            Rekord r[12]; int nr = rekorde(nach, r, 12);
            for (int i = 0; i < nr && i < 4; i++) ui_person(r[i].profil, tmp("%s %s", r[i].emoji, r[i].titel), tmp("%s · %s", pname(r[i].profil), r[i].text), NULL);
            {
                static char ja[200], nein[200]; ja[0] = nein[0] = 0;
                for (int i = 0; i < db.aktivitaeten.n; i++) {
                    const Aktivitaet *a = &db.aktivitaeten.v[i];
                    if (a->treffen != nach) continue;
                    char *z = a->erledigt ? ja : nein;
                    if (z[0]) strncat(z, ", ", sizeof ja - strlen(z) - 1);
                    strncat(z, a->medium >= 0 ? db.medien.v[a->medium].titel : a->titel, sizeof ja - strlen(z) - 1);
                }
                if (ja[0] || nein[0]) ui_symbolzeile(IC_LIGHTBULB, tmp("%s%s%s%s%s", ja, ja[0] ? " erledigt" : "", ja[0] && nein[0] ? " · " : "", nein, nein[0] ? (strchr(nein, ',') ? " bleiben offen" : " bleibt offen") : ""), TH->muted);
            }
            if (ONEDRIVE_LINK[0]) ui_symbolzeile(IC_CAMERA, tmp("Fotos und Videos: %s", ONEDRIVE_LINK), TH->muted);
            if (ui_knopf_art("Zur Ruhmeshalle", KN_TONAL, IC_TROPHY)) { ruhm_reiter = 0; seite_auf(SE_RUHM, 0); }
            ui_karte_ende();
        }
    }
    int nz = 0;
    for (int i = 0; i < db.zahlungen.n; i++) {
        const Zahlung *z = &db.zahlungen.v[i];
        if (z->treffen < 0 || z->status == ZAHLUNG_BESTAETIGT || (z->von != ich && z->an != ich)) continue;
        if (!nz++) ui_karte("Zahlungen", IC_EURO);
        if (z->von == ich) ui_person(z->an, tmp("Du zahlst an %s", pname(z->an)), z->status == ZAHLUNG_GEMELDET ? "gemeldet" : db.profile.v[z->an].paypal[0] ? tmp("PayPal: %s", db.profile.v[z->an].paypal) : "offen", euro_text(z->betrag));
        else ui_person(z->von, tmp("%s zahlt an dich", pname(z->von)), z->status == ZAHLUNG_GEMELDET ? "gemeldet – bitte bestätigen" : "offen", euro_text(z->betrag));
    }
    if (nz) { if (ui_knopf_art("Zur Kasse", KN_TONAL, IC_CHEVRON_RIGHT)) { reiter = R_KASSE; kasse_ansicht = 0; ui_seite_neu(); } ui_karte_ende(); }
    karte_offenes();
}

/* ======================================================================= Plan */
static int besuch_b = -1, besuch_schritt;
static int32_t besuch_beginn;
static void besuch_bearbeiten(int b) { besuch_b = b; besuch_schritt = 1; uhr_dialog(ID_BESUCH, db.buchungen.v[b].start); }
static void besuch_flow(void) {
    int32_t s;
    if (besuch_b < 0 || !uhr_ergebnis(ID_BESUCH, &s)) return;
    if (besuch_schritt == 1) { besuch_beginn = s; besuch_schritt = 2; uhr_dialog(ID_BESUCH, buchung_ende(besuch_b)); return; }
    Aktion *a = aktion_neu(A_BESUCH); a->a = (int16_t)besuch_b; a->b = (int16_t)minute_von(besuch_beginn); a->c = (int16_t)minute_von(s);
    mach(a, "Besuch korrigiert.");
    besuch_b = -1;
}

static void seite_plan(void) {
    int t = laufendes_treffen();
    if (t < 0) t = ruhm_treffen();
    if (t < 0) { ui_text("Noch kein Treffen mit Bad-Besuchen.", F_NORMAL, TH->muted); return; }
    const Treffen *tr = &db.treffen.v[t];
    if (plan_tag < tr->von || plan_tag > tr->bis) plan_tag = tag_von(jetzt) >= tr->von && tag_von(jetzt) <= tr->bis ? tag_von(jetzt) : tr->von;
    if (ui_knopf_art("", KN_KLEIN, IC_CHEVRON_LEFT)) { if (plan_tag > tr->von) plan_tag--; nochmal_zeichnen = 1; }
    ui_knopf_neben_art(tmp("%s%s", datum_text(plan_tag), plan_tag == tag_von(jetzt) ? " · heute" : ""), KN_KLEIN, -1);
    if (ui_knopf_neben_art("", KN_KLEIN, IC_CHEVRON_RIGHT)) { if (plan_tag < tr->bis) plan_tag++; nochmal_zeichnen = 1; }
    if (ui_taste() & T_C_LINKS) { if (plan_tag > tr->von) plan_tag--; nochmal_zeichnen = 1; }
    if (ui_taste() & T_C_RECHTS) { if (plan_tag < tr->bis) plan_tag++; nochmal_zeichnen = 1; }
    int bs[8], nb = baeder(t, bs, 8);
    if (!nb) { ui_text("Keine Bäder.", F_NORMAL, TH->muted); return; }
    zeit_t tag0 = (zeit_t)plan_tag * 86400;
    int von_h = 6;
    for (int b = 0; b < db.buchungen.n; b++) {
        const Buchung *x = &db.buchungen.v[b];
        if (x->geloescht || x->treffen != t || tag_von(x->start) != plan_tag) continue;
        int hh = minute_von(x->start) / 60; if (hh < von_h) von_h = hh;
    }
    int y0 = ui_y() + 26, sh = 52, spalte_x = RAND_X + 46, spalte_b = (INHALT_B - 46) / nb;
    for (int i = 0; i < nb; i++) text_mitte(spalte_x + i * spalte_b + spalte_b / 2, ui_y(), db.raeume.v[bs[i]].name, F_KLEIN, TH->muted);
    for (int i = 0; i < nb; i++) gfx_runde_box(spalte_x + i * spalte_b + 3, y0, spalte_b - 6, (24 - von_h) * sh, 8, gfx_mischen(TH->bg, TH->flaeche, 140));
    for (int hh = von_h; hh <= 24; hh++) {
        int y = y0 + (hh - von_h) * sh;
        gfx_linie_h(spalte_x, y, INHALT_B - 46, TH->line);
        if (hh < 24) text(RAND_X - 4, y - 8, tmp("%02d:00", hh), F_KLEIN, TH->muted);
    }
    for (int b = 0; b < db.buchungen.n; b++) {
        const Buchung *x = &db.buchungen.v[b];
        if (x->geloescht || x->treffen != t || tag_von(x->start) != plan_tag) continue;
        int sp = -1; for (int i = 0; i < nb; i++) if (bs[i] == x->raum) sp = i;
        if (sp < 0) continue;
        zeit_t e = buchung_ende(b);
        int a = (int)(x->start - tag0) / 60 - von_h * 60, z = (int)(e - tag0) / 60 - von_h * 60;
        if (z - a < 22 * 60 / sh) z = a + 22 * 60 / sh;
        int by = y0 + a * sh / 60, bh = (z - a) * sh / 60, bx = spalte_x + sp * spalte_b + 6;
        int eigen = gehoert(b, ich) || x->profil == ich;
        int an = ui_kachel_knopf(bx, by, spalte_b - 12, bh), f = ui_fokussiert_letztes();
        if (f) ui_fokus_rahmen(bx, by, spalte_b - 12, bh, 6);
        gfx_runde_box(bx, by, spalte_b - 12, bh, 6, uhr_laeuft(b) ? TH->errBg : TH->flaeche2);
        gfx_rechteck(bx, by + 2, 4, bh - 4, pfarbe(x->profil));
        if (eigen) gfx_runder_rahmen(bx, by, spalte_b - 12, bh, 6, 2, TH->accent);
        text_max(bx + 10, by + (bh - 18) / 2, tmp("%s  %s–%s", eigen ? "Du" : pname(x->profil), uhr_text(x->start), uhr_laeuft(b) ? "jetzt" : uhr_text(e)), spalte_b - 26, F_KLEIN, TH->text);
        if (an) {
            if (eigen && !uhr_laeuft(b) && !tr->beendet_um) besuch_bearbeiten(b);
            else meldung(tmp("%s · %s bis %s · %d Min", pname(x->profil), uhr_text(x->start), uhr_laeuft(b) ? "jetzt" : uhr_text(e), (int)((e - x->start + 59) / 60)));
        }
    }
    static int plan_gerollt = -1;
    if (plan_gerollt != plan_tag) {   /* beim Öffnen zur aktuellen Uhrzeit (bzw. zur letzten Buchung) rollen */
        int ziel = tag_von(jetzt) == plan_tag ? minute_von(jetzt) : -1;
        for (int b = 0; b < db.buchungen.n && ziel < 0; b++) if (!db.buchungen.v[b].geloescht && db.buchungen.v[b].treffen == t && tag_von(db.buchungen.v[b].start) == plan_tag) ziel = minute_von(db.buchungen.v[b].start);
        if (ziel > 0) ui_roll_auf(ziel * sh / 60 - von_h * sh - 80 + (y0 - KOPF_HAUPT));
        plan_gerollt = plan_tag;
    }
    if (tag_von(jetzt) == plan_tag) {
        int m = minute_von(jetzt) - von_h * 60;
        if (m >= 0) { int y = y0 + m * sh / 60; gfx_rechteck(spalte_x, y, INHALT_B - 46, 2, TH->err); gfx_runde_box(RAND_X - 8, y - 9, 50, 18, 9, TH->err); text_mitte(RAND_X + 17, y - 8, uhr_text(jetzt), F_KLEIN, TH->errBg); }
    }
    ui_weiter((24 - von_h) * sh + 40);
    ui_text("Nur Anzeige: wer wann drin war. A auf einen eigenen Besuch: Beginn und Ende korrigieren.", F_KLEIN, TH->muted);
}

/* ======================================================================= Kasse & Trips */
static struct { int art; int32_t betrag; char titel[64]; uint32_t fuer; } neu_a;
static const char *plus_namen[10];
static struct { int aktiv, i, n; int pid[MAX_PROFILE]; int32_t ct[MAX_PROFILE]; char titel[64]; int32_t liefer; } essen;
static const char *namen_ich[MAX_PROFILE];
static void namen_fuellen(void) { for (int p = 0; p < db.profile.n; p++) namen_ich[p] = p == ich ? tmp("%s (ich)", pname(p)) : pname(p); }

static void ausgabe_flow(int t) {
    int w = auswahl_ergebnis(ID_ART);
    if (w >= 0) {
        if (w == AUSGABE_ARTEN_N) tastatur(ID_TRIP, "Neuer Trip: Wohin?", "", 60);
        else if (!strcmp(AUSGABE_ARTEN[w].schluessel, "essen")) { essen.aktiv = 1; tastatur(ID_ESSEN, "Essensbestellung: Wo bestellt?", "", 60); }
        else if (!strcmp(AUSGABE_ARTEN[w].schluessel, "fahrt")) { kasse_ansicht = 1; meldung("Fahrtkosten rechnet der Fahrer im Trip ab."); }
        else { neu_a.art = w; zahl(ID_BETRAG, tmp("%s: Betrag", art_text(w)), 0); }
    }
    int32_t c;
    if (zahl_ergebnis(ID_BETRAG, &c)) {
        if (c <= 0) meldung("Wie viel hat es gekostet?");
        else { neu_a.betrag = c; tastatur(ID_TITEL, tmp("%s: Wofür?", art_text(neu_a.art)), "", 60); }
    }
    const char *tt = tastatur_ergebnis(ID_TITEL);
    if (tt) {
        strncpy(neu_a.titel, tt, sizeof neu_a.titel - 1); neu_a.titel[sizeof neu_a.titel - 1] = 0;
        uint32_t m = 0; int tn[MAX_PROFILE], n = treffen_teilnehmer(t, tn);
        for (int i = 0; i < n; i++) m |= 1u << tn[i];
        personen(ID_FUER, "Für wen war es?", m);
    }
    uint32_t m;
    if (personen_ergebnis(ID_FUER, &m)) { neu_a.fuer = m; namen_fuellen(); auswahl(ID_ZAHLER, "Wer hat bezahlt?", namen_ich, db.profile.n, ich); }
    w = auswahl_ergebnis(ID_ZAHLER);
    if (w >= 0) {
        Aktion *a = aktion_neu(A_AUSGABE);
        a->a = (int16_t)t; a->b = (int16_t)neu_a.art; a->c = (int16_t)w; a->betrag = neu_a.betrag; a->maske = neu_a.fuer;
        strncpy(a->text, neu_a.titel, AKTION_TEXT_MAX);
        mach(a, tmp("%s eingetragen: %s", art_text(neu_a.art), euro_text(neu_a.betrag)));
    }
    const char *et = tastatur_ergebnis(ID_ESSEN);
    if (et) { strncpy(essen.titel, et, sizeof essen.titel - 1); essen.titel[sizeof essen.titel - 1] = 0; personen(ID_ESSEN + 100, "Wer hat bestellt?", 0); }
    if (personen_ergebnis(ID_ESSEN + 100, &m)) {
        essen.n = 0; essen.i = 0;
        for (int p = 0; p < db.profile.n; p++) if (m & (1u << p)) essen.pid[essen.n++] = p;
        if (essen.n) zahl(ID_ESSEN + 200, tmp("Was hat %s bestellt? Betrag", pname(essen.pid[0])), 0); else essen.aktiv = 0;
    }
    if (zahl_ergebnis(ID_ESSEN + 200, &c)) {
        essen.ct[essen.i++] = c;
        if (essen.i < essen.n) zahl(ID_ESSEN + 200, tmp("Was hat %s bestellt? Betrag", pname(essen.pid[essen.i])), 0);
        else zahl(ID_ESSEN + 300, "Lieferkosten (gleich verteilt, 0 = keine)", 0);
    }
    if (zahl_ergebnis(ID_ESSEN + 300, &c)) { essen.liefer = c; namen_fuellen(); auswahl(ID_ESSEN + 400, "Wer hat bezahlt?", namen_ich, db.profile.n, ich); }
    w = auswahl_ergebnis(ID_ESSEN + 400);
    if (w >= 0) {
        Aktion *a = aktion_neu(A_ESSEN); a->a = (int16_t)t; a->c = (int16_t)w; a->betrag = essen.liefer;
        int pos = snprintf(a->text, sizeof a->text, "%s" TRENN, essen.titel);
        for (int i = 0; i < essen.n && pos < AKTION_TEXT_MAX - 20; i++) pos += snprintf(a->text + pos, sizeof a->text - (size_t)pos, "%d:%ld:;", essen.pid[i], (long)essen.ct[i]);
        mach(a, "Essensbestellung eingetragen. Jeder zahlt, was er bestellt hat.");
        essen.aktiv = 0;
    }
}

static void trips_ansicht(int t) {
    const Treffen *tr = &db.treffen.v[t];
    int n = 0;
    for (int i = 0; i < db.trips.n; i++) {
        const Trip *x = &db.trips.v[i];
        if (x->geloescht || x->treffen != t) continue;
        n++;
        ui_karte(x->titel, IC_ROUTE);
        ui_text(x->datum ? tmp("%s%s · angelegt von %s", datum_text(x->datum), x->uhrzeit >= 0 ? tmp(" %02d:%02d Uhr", x->uhrzeit / 60, x->uhrzeit % 60) : "", pname(x->profil)) : x->wann, F_KLEIN, TH->muted);
        for (int r = 0; r < 2; r++) {
            int k = 0;
            for (int f = 0; f < db.fahrten.n; f++) {
                const Fahrt *fa = &db.fahrten.v[f];
                if (fa->geloescht || fa->trip != i || fa->richtung != r) continue;
                if (!k++) ui_text(r ? "RÜCKFAHRT" : "HINFAHRT", F_FETT, TH->muted);
                int ins[12], ni = fahrt_insassen(f, ins, 12), drin = 0;
                char mit[160] = "";
                for (int q = 1; q < ni; q++) { if (ins[q] == ich) drin = 1; if (mit[0]) strncat(mit, ", ", sizeof mit - strlen(mit) - 1); strncat(mit, pname(ins[q]), sizeof mit - strlen(mit) - 1); }
                int abger = 0; for (int a = 0; a < db.ausgaben.n; a++) if (!db.ausgaben.v[a].geloescht && db.ausgaben.v[a].trip == i && db.ausgaben.v[a].profil == fa->profil) abger = 1;
                ui_person(fa->profil, tmp("Auto %d · %s", k, pname(fa->profil)), ni > 1 ? tmp("mit %s%s", mit, abger ? " · abgerechnet" : "") : (abger ? "abgerechnet" : "noch allein"), tmp("%d frei", fa->plaetze - (ni - 1)));
                if (!tr->kasse_zu_um && !abger) {
                    if (fa->profil == ich) { if (ui_knopf_art("Auto entfernen", KN_RAND, IC_TRASH_2)) { Aktion *a = aktion_neu(A_FAHRT_WEG); a->a = (int16_t)f; mach(a, NULL); } }
                    else if (drin) { if (ui_knopf_art("Aussteigen", KN_RAND, IC_LOG_OUT)) { Aktion *a = aktion_neu(A_AUSSTEIGEN); a->a = (int16_t)f; mach(a, NULL); } }
                    else if (ui_knopf_art("Dabei", KN_VOLL, IC_CHECK)) { Aktion *a = aktion_neu(A_MITFAHREN); a->a = (int16_t)f; mach(a, "Eingestiegen!"); }
                }
            }
        }
        if (!tr->kasse_zu_um) {
            if (db.profile.v[ich].auto_) {
                if (ui_knopf_art("Auto hin", KN_TONAL, IC_PLUS)) { Aktion *a = aktion_neu(A_FAHRT_NEU); a->a = (int16_t)i; a->b = 0; mach(a, NULL); }
                if (ui_knopf_neben_art("Auto zurück", KN_TONAL, IC_PLUS)) { Aktion *a = aktion_neu(A_FAHRT_NEU); a->a = (int16_t)i; a->b = 1; mach(a, NULL); }
            }
            int fahrer = 0; for (int f = 0; f < db.fahrten.n; f++) if (!db.fahrten.v[f].geloescht && db.fahrten.v[f].trip == i && db.fahrten.v[f].profil == ich) fahrer = 1;
            if (fahrer && ui_knopf_art("Meine Fahrtkosten berechnen", KN_VOLL, IC_CAR)) { kosten_trip_wahl = i; zahl_art(ID_KOSTEN, tmp("%s: gefahrene km (einfache Strecke)", x->titel), 0, 4, 0, "km"); }
            if (ui_knopf_neben_art("Trip löschen", KN_RAND, IC_TRASH_2)) frage(ID_TRIP * 1000 + i, tmp("Trip „%s“ löschen?", x->titel), "Löschen", "Behalten");
            if (frage_ergebnis(ID_TRIP * 1000 + i) == 0) { Aktion *a = aktion_neu(A_TRIP_WEG); a->a = (int16_t)i; mach(a, NULL); }
        }
        ui_karte_ende();
    }
    if (!n) ui_text("Noch keine Trips. Ein Trip ist ein Ausflug, zu dem ihr mit Autos fahrt.", F_NORMAL, TH->muted);
    if (!tr->kasse_zu_um && ui_knopf_art("Neuer Trip", KN_VOLL, IC_PLUS)) tastatur(ID_TRIP, "Neuer Trip: Wohin?", "", 60);
}
static void trip_flow(int t) {
    static char titel[64];
    const char *tt = tastatur_ergebnis(ID_TRIP);
    if (tt && tt[0]) { strncpy(titel, tt, sizeof titel - 1); titel[sizeof titel - 1] = 0; uhr_dialog(ID_TRIP + 1, (int32_t)(tag_von(jetzt) > db.treffen.v[t].von ? tag_von(jetzt) : db.treffen.v[t].von) * 86400 + 14 * 3600); }
    int32_t s;
    if (uhr_ergebnis(ID_TRIP + 1, &s)) { Aktion *a = aktion_neu(A_TRIP_NEU); a->a = (int16_t)t; a->b = tag_von(s); a->c = (int16_t)minute_von(s); strncpy(a->text, titel, AKTION_TEXT_MAX); mach(a, "Trip angelegt. Wer fährt, trägt sein Auto ein."); kasse_ansicht = 1; }
    static int32_t km;
    if (zahl_ergebnis(ID_KOSTEN, &s)) { km = s; zahl_art(ID_KOSTEN + 1, tmp("Preis je %s in Cent", db.profile.v[ich].elektro ? "kWh" : "Liter"), 1799, 4, 1, "ct"); }
    if (zahl_ergebnis(ID_KOSTEN + 1, &s)) {
        Aktion *a = aktion_neu(A_FAHRTKOSTEN); a->a = (int16_t)kosten_trip_wahl; a->b = (int16_t)km; a->betrag = s;
        mach(a, "Fahrtkosten abgerechnet. Sie stehen in der Kasse.");
    }
}

static void seite_kasse(void) {
    int t = kasse_treffen();
    if (t < 0) { ui_text("Keine Kasse offen.", F_NORMAL, TH->muted); return; }
    const Treffen *tr = &db.treffen.v[t];
    static const char *const ANS[2] = { "Kasse", "Trips" };
    ui_text(tmp("%s · %s%s", treffen_titel(t), treffen_status(t) == T_LAUFEND ? "läuft gerade" : treffen_status(t) == T_KOMMEND ? tage_bis(tr->von) : "vorbei", tr->gastgeber >= 0 ? tmp(" · bei %s", pname(tr->gastgeber)) : ""), F_NORMAL, TH->muted);
    int w = ui_segment(ANS, 2, kasse_ansicht);
    if (w >= 0) { kasse_ansicht = w; nochmal_zeichnen = 1; }
    trip_flow(t);
    ausgabe_flow(t);
    if (kasse_ansicht == 1) { trips_ansicht(t); return; }
    int32_t stand[MAX_PROFILE]; uint32_t bet;
    int32_t summe = kassen_stand(t, stand, &bet);
    ui_karte("Dein Stand", IC_SCALE);
    if (bet & (1u << ich)) { int32_t c = stand[ich]; ui_text(c > 0 ? tmp("Du bekommst %s", euro_text(c)) : c < 0 ? tmp("Du zahlst %s", euro_text(-c)) : "Du bist quitt", F_GROSS, c > 0 ? TH->ok : c < 0 ? TH->err : TH->text); }
    else ui_text("Du bist (noch) nicht dabei.", F_NORMAL, TH->muted);
    ui_zeile("Ausgaben insgesamt", euro_text(summe), TH->muted);
    if (tr->kasse_zu_um) ui_symbolzeile(IC_LOCK, tmp("Kasse geschlossen am %s", datum_text(tag_von(tr->kasse_zu_um))), TH->warn);
    if (!tr->kasse_zu_um && ui_knopf_breit("Eintragen", KN_VOLL, IC_PLUS)) {
        int k = 0;
        for (int i = 0; i < AUSGABE_ARTEN_N && k < 9; i++) plus_namen[k++] = tmp("%s  %s", AUSGABE_ARTEN[i].emoji, AUSGABE_ARTEN[i].text);
        plus_namen[k++] = "Neuer Trip";
        auswahl(ID_ART, "Was möchtest du eintragen?", plus_namen, k, 0);
    }
    int darf = tr->gastgeber >= 0 ? ist_gastgeber(t, ich) : hat_komme(t, ich);
    if (darf && !tr->beendet_um) {
        if (ui_knopf_art(tr->kasse_zu_um ? "Kasse wieder öffnen" : "Kasse schließen", KN_RAND, IC_LOCK))
            frage(ID_KASSE_ZU, tr->kasse_zu_um ? "Kasse wieder öffnen? Die Zahlungen werden zurückgesetzt." : "Kasse schließen? Danach zahlt jeder direkt an den, der bezahlt hat.", tr->kasse_zu_um ? "Öffnen" : "Schließen", "Abbrechen");
    }
    if (frage_ergebnis(ID_KASSE_ZU) == 0) { Aktion *a = aktion_neu(tr->kasse_zu_um ? A_KASSE_AUF : A_KASSE_ZU); a->a = (int16_t)t; mach(a, NULL); }
    ui_karte_ende();
    if (tr->kasse_zu_um) {
        ui_ueberschrift("Zahlungen", NULL);
        int n = 0;
        for (int i = 0; i < db.zahlungen.n; i++) {
            const Zahlung *z = &db.zahlungen.v[i];
            if (z->treffen != t) continue;
            n++;
            const char *st = z->status == ZAHLUNG_BESTAETIGT ? "bestätigt" : z->status == ZAHLUNG_GEMELDET ? "gemeldet" : "offen";
            if (ui_eintrag(z->von, tmp("%s → %s", pname(z->von), pname(z->an)), tmp("%s%s", st, db.profile.v[z->an].paypal[0] ? tmp(" · PayPal: %s", db.profile.v[z->an].paypal) : ""), euro_text(z->betrag))) {
                if (z->von == ich && z->status == ZAHLUNG_OFFEN) { Aktion *a = aktion_neu(A_ZAHLUNG); a->a = (int16_t)i; a->b = ZAHLUNG_GEMELDET; mach(a, "Gemeldet. Jetzt muss es nur noch ankommen."); }
                else if (z->an == ich && z->status != ZAHLUNG_BESTAETIGT) { Aktion *a = aktion_neu(A_ZAHLUNG); a->a = (int16_t)i; a->b = ZAHLUNG_BESTAETIGT; mach(a, "Bestätigt. Danke!"); }
                else meldung(z->von == ich || z->an == ich ? "Hier ist nichts mehr zu tun." : "Das ist nicht deine Zahlung.");
            }
        }
        if (!n) ui_text("Alle sind quitt.", F_NORMAL, TH->muted);
        else ui_text("A auf eine Zahlung: „Hab bezahlt“ (Zahler) bzw. „Ist angekommen“ (Empfänger).", F_KLEIN, TH->muted);
    } else {
        DirektPaar d[64]; int n = direkt_paare(t, d, 64);
        if (n) {
            ui_ueberschrift("So wird gezahlt", NULL);
            for (int i = 0; i < n; i++)
                if (d[i].betrag > 0) ui_person(d[i].von, tmp("%s → %s", pname(d[i].von), pname(d[i].an)), db.profile.v[d[i].an].paypal[0] ? tmp("PayPal: %s", db.profile.v[d[i].an].paypal) : "direkt an den Zahler", euro_text(d[i].betrag));
                else ui_zeile(tmp("%s und %s: geht genau auf", pname(d[i].von), pname(d[i].an)), NULL, TH->muted);
        }
    }
    int na = 0;
    for (int i = 0; i < db.ausgaben.n; i++) if (!db.ausgaben.v[i].geloescht && db.ausgaben.v[i].treffen == t) na++;
    ui_ueberschrift("Ausgaben", tmp("%d", na));
    for (int i = db.ausgaben.n - 1; i >= 0; i--) {
        const Ausgabe *a = &db.ausgaben.v[i];
        if (a->geloescht || a->treffen != t) continue;
        int8_t ids[64]; int32_t c[64]; int k = ausgabe_teile(i, ids, c, 64);
        uint32_t pm = 0; for (int j = 0; j < k; j++) pm |= 1u << ids[j];
        int np = 0; for (int p = 0; p < 32; p++) if (pm & (1u << p)) np++;
        if (ui_eintrag_symbol(AUSGABE_ARTEN[a->art].icon, NULL, tmp("%s%s%s", art_text(a->art), a->titel && a->titel[0] ? " · " : "", a->titel ? a->titel : ""),
                              tmp("%s hat bezahlt · für %d %s", pname(a->profil), np, np == 1 ? "Person" : "Personen"), euro_text(a->betrag)))
            if (!tr->kasse_zu_um) frage(ID_AUSGABE_WEG * 1000 + i, tmp("„%s“ über %s löschen?", a->titel && a->titel[0] ? a->titel : art_text(a->art), euro_text(a->betrag)), "Löschen", "Behalten");
        if (frage_ergebnis(ID_AUSGABE_WEG * 1000 + i) == 0) { Aktion *x = aktion_neu(A_AUSGABE_WEG); x->a = (int16_t)i; mach(x, NULL); }
    }
    if (!na) ui_text("Noch keine Ausgaben.", F_NORMAL, TH->muted);
    if (summe) {
        ui_ueberschrift("Stand je Person", NULL);
        for (int p = 0; p < db.profile.n; p++) {
            if (!(bet & (1u << p))) continue;
            int32_t bz, an; kassen_konto(t, p, &bz, &an);
            ui_person(p, pname(p), tmp("bezahlt %s · Anteil %s", euro_text(bz), euro_text(an)), stand[p] ? tmp("%s%s", stand[p] > 0 ? "+" : "−", euro_text(stand[p] < 0 ? -stand[p] : stand[p])) : "quitt");
        }
    }
}

/* ======================================================================= Treffen (Liste) */
static int treffen_kachel(int t, int knopf) {
    const Treffen *tr = &db.treffen.v[t];
    int st = treffen_status(t), ys = ui_y();
    ui_karte_art(st == T_LAUFEND ? KA_AKZENT : KA_NORMAL, NULL, -1);
    int y = ui_y(), x = ui_x(), b = ui_breite();
    uint16_t vg = st == T_LAUFEND ? TH->onAccentCont : TH->text;
    int j, m, d; tag_zu_datum(tr->von, &j, &m, &d);
    static const char *const MON[12] = { "JAN", "FEB", "MÄR", "APR", "MAI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEZ" };
    gfx_runde_box(x + b - 60, y, 60, 62, 10, TH->flaeche2);
    gfx_runde_box(x + b - 60, y, 60, 22, 10, TH->accent); gfx_rechteck(x + b - 60, y + 12, 60, 10, TH->accent);
    text_mitte(x + b - 30, y + 3, MON[m - 1], F_KLEIN, TH->onAccent);
    text_mitte(x + b - 30, y + 26, tmp("%d", d), F_TITEL, TH->text);
    int dy = 0;
    if (st == T_LAUFEND) { gfx_runde_box(x, y, 220, 26, 13, TH->okBg); gfx_kreis(x + 14, y + 13, 5, TH->ok); text(x + 26, y + 4, tmp("Läuft gerade · Tag %d von %d", tag_von(jetzt) - tr->von + 1, tr->bis - tr->von + 1), F_KLEIN, TH->ok); dy = 32; }
    text_max(x, y + dy, treffen_titel(t), b - 76, F_TITEL, vg);
    text_max(x, y + dy + 26, tmp("%s · %s", zeitraum_text(tr->von, tr->bis), st == T_KOMMEND ? tage_bis(tr->von) : st == T_VORBEI ? "beendet" : st == T_UEBERFAELLIG ? "bitte beenden" : "läuft"), b - 76, F_KLEIN, st == T_LAUFEND ? vg : TH->muted);
    if (tr->gastgeber >= 0) { avatar(tr->gastgeber, x, y + dy + 48, 28); text(x + 36, y + dy + 53, tmp("bei %s", pname(tr->gastgeber)), F_NORMAL, vg); }
    else { gfx_symbol(IC_MAP_PIN, x, y + dy + 52, 20, vg); text(x + 28, y + dy + 53, "auswärts", F_NORMAL, vg); }
    if (!ist_gastgeber(t, ich)) { int z = zusage_von(t, ich); text_rechts(x + b - 70, y + dy + 53, zusage_text(z), F_FETT, z == ZUSAGE_KOMME ? TH->ok : TH->muted); }
    ui_abstand(dy + 84);
    for (int k = 0; k < db.kommentare.n; k++)
        if (!db.kommentare.v[k].geloescht && db.kommentare.v[k].treffen == t && db.kommentare.v[k].angepinnt) ui_symbolzeile(IC_PIN, tmp("%s: %s", pname(db.kommentare.v[k].profil), db.kommentare.v[k].text), TH->muted);
    ui_karte_ende();
    return knopf ? karte_als_knopf(ys) : 0;
}
static void seite_treffenliste(void) {
    int ord[64], n = 0;
    for (int i = 0; i < db.treffen.n && n < 64; i++) {
        int st = treffen_status(i);
        if (st != T_KOMMEND && st != T_UEBERFAELLIG && st != T_LAUFEND) continue;
        int k = n++;
        while (k > 0 && db.treffen.v[ord[k - 1]].von > db.treffen.v[i].von) { ord[k] = ord[k - 1]; k--; }
        ord[k] = i;
    }
    for (int i = 0; i < n; i++) if (treffen_kachel(ord[i], 1)) seite_auf(SE_TREFFEN, ord[i]);
    if (!n) ui_text("Nichts geplant.", F_NORMAL, TH->muted);
    if (ui_knopf_art("Neues Treffen", KN_VOLL, IC_PLUS)) { treffen_form_neu(-1); seite_auf(SE_TREFFEN_FORM, -1); }
    ui_ueberschrift("Vorbei", NULL);
    for (int i = db.treffen.n - 1; i >= 0; i--)
        if (treffen_status(i) == T_VORBEI && ui_eintrag(db.treffen.v[i].gastgeber, treffen_titel(i), tmp("%s · beendet", zeitraum_text(db.treffen.v[i].von, db.treffen.v[i].bis)), "›")) seite_auf(SE_TREFFEN, i);
    if (ui_eintrag_symbol(IC_HISTORY, NULL, tmp("Frühere Treffen (%d)", db.frueher.n), "Statistik seit 2013", "›")) seite_auf(SE_FRUEHER, 0);
}

/* ======================================================================= Treffen (Detail) */
static void seite_treffen(int t) {
    const Treffen *tr = &db.treffen.v[t];
    int st = treffen_status(t);
    if (seiten_kopf(treffen_titel(t), IC_ELLIPSIS_VERTICAL) == KOPF_EXTRA) {
        static const char *opt[4]; int n = 0;
        if (!tr->beendet_um) { opt[n] = "Bearbeiten"; treffen_menue_code[n++] = 0; }
        if (!tr->beendet_um && (st == T_KOMMEND || hat_komme(t, ich))) { opt[n] = "Absagen"; treffen_menue_code[n++] = 1; }
        if ((st == T_LAUFEND || st == T_UEBERFAELLIG) && hat_komme(t, ich)) { opt[n] = "Treffen beenden"; treffen_menue_code[n++] = 2; }
        if (n) auswahl(ID_TREFFEN_MENUE, treffen_titel(t), opt, n, 0);
        else meldung("Ein beendetes Treffen bleibt. Es steht unter „Frühere Treffen“.");
    }
    int w = auswahl_ergebnis(ID_TREFFEN_MENUE);
    if (w >= 0) {
        if (treffen_menue_code[w] == 0) { treffen_form_neu(t); seite_auf(SE_TREFFEN_FORM, t); return; }
        if (treffen_menue_code[w] == 1) frage(ID_ABSAGEN, tmp("„%s“ absagen? Es verschwindet für alle.", treffen_titel(t)), "Absagen", "Behalten");
        if (treffen_menue_code[w] == 2) frage(ID_BEENDEN, "Treffen beenden? Danach kann niemand mehr ins Bad, die Kasse bleibt für immer zu und das Treffen kommt unter „Frühere Treffen“.", "Bestätigen", "Verwerfen");
    }
    if (frage_ergebnis(ID_ABSAGEN) == 0) { Aktion *a = aktion_neu(A_TREFFEN_ABSAGEN); a->a = (int16_t)t; if (mach(a, "Abgesagt.")) { seite_zurueck(); return; } }
    if (frage_ergebnis(ID_BEENDEN) == 0) { Aktion *a = aktion_neu(A_TREFFEN_BEENDEN); a->a = (int16_t)t; mach(a, "Treffen beendet. Ab in die Statistik!"); }
    if (tr->info && tr->info[0]) { ui_karte_art(KA_WARN, NULL, -1); ui_symbolzeile(IC_INFO, "Infos des Gastgebers", TH->warn); ui_text(tr->info, F_NORMAL, TH->text); ui_karte_ende(); }
    treffen_kachel(t, 0);
    int g[2], ng = treffen_gastgeber(t, g);
    ui_karte(NULL, -1);
    if (ng) ui_symbolzeile(IC_HOUSE, tmp("Gastgeber: %s%s%s", pname(g[0]), ng > 1 ? " & " : "", ng > 1 ? pname(g[1]) : ""), TH->text);
    else ui_symbolzeile(IC_MAP_PIN, "Auswärts – Gastgeber-Rechte hat jeder mit „Komme“.", TH->text);
    const char *adr = treffen_adresse(t);
    if (adr[0]) ui_symbolzeile(IC_MAP_PIN, adr, TH->accentText);
    if (tr->gastgeber >= 0 && db.profile.v[raum_besitzer(tr->gastgeber)].parkplaetze) ui_symbolzeile(IC_SQUARE_PARKING, tmp("%d Parkplätze · %s", db.profile.v[raum_besitzer(tr->gastgeber)].parkplaetze, db.profile.v[raum_besitzer(tr->gastgeber)].park_hinweis), TH->muted);
    if (st == T_KOMMEND || st == T_LAUFEND) karte_zusage(t);
    if (st == T_LAUFEND && (ist_anwesend(t, ich) || hat_komme(t, ich))) {
        if (ui_knopf_art("Mein Heini-Treffen", KN_TONAL, IC_SPARKLES)) seite_auf(SE_MEIN, t);
        if (ui_knopf_neben_art("Zum Plan", KN_TONAL, IC_CALENDAR)) { seite_zurueck(); reiter = R_PLAN; }
    }
    ui_karte_ende();
    ui_ueberschrift("Wer kommt?", NULL);
    for (int s = 0; s < 3; s++) {
        int pr[MAX_PROFILE], n = 0;
        for (int p = 0; p < db.profile.n; p++) { int z = tr->gastgeber == p || tr->gastgeber2 == p ? ZUSAGE_KOMME : zusage_von(t, p); if (z == s) pr[n++] = p; }
        if (!n) continue;
        ui_text(tmp("%s (%d)", zusage_text(s), n), F_FETT, s == 0 ? TH->ok : TH->muted);
        ui_avatare(pr, n);
    }
    ui_ueberschrift("Agenda", NULL);
    int na = 0;
    for (tag_t d = tr->von; d <= tr->bis; d++)
        for (int i = 0; i < db.agenda.n; i++) {
            const Agenda *a = &db.agenda.v[i];
            if (a->geloescht || a->treffen != t || a->datum != d) continue;
            na++;
            if (ui_eintrag_symbol(IC_CLOCK, NULL, a->name, tmp("%s %s%s%s", datum_text(d), a->uhrzeit >= 0 ? tmp("%02d:%02d", a->uhrzeit / 60, a->uhrzeit % 60) : "", a->notiz[0] ? " · " : "", a->notiz), NULL) && hat_komme(t, ich))
                frage(ID_AGENDA_NAME * 1000 + i, tmp("„%s“ aus der Agenda löschen?", a->name), "Löschen", "Behalten");
            if (frage_ergebnis(ID_AGENDA_NAME * 1000 + i) == 0) { Aktion *x = aktion_neu(A_AGENDA_WEG); x->a = (int16_t)i; mach(x, NULL); }
        }
    if (!na) ui_text("Noch nichts geplant.", F_NORMAL, TH->muted);
    static tag_t ag_tag; static int ag_zeit;
    if (!tr->beendet_um && hat_komme(t, ich) && ui_knopf_art("Punkt", KN_TONAL, IC_PLUS)) uhr_dialog(ID_AGENDA_ZEIT, (int32_t)(tag_von(jetzt) >= tr->von && tag_von(jetzt) <= tr->bis ? tag_von(jetzt) : tr->von) * 86400 + 14 * 3600);
    int32_t s;
    if (uhr_ergebnis(ID_AGENDA_ZEIT, &s)) { ag_zeit = minute_von(s); ag_tag = tag_von(s); tastatur(ID_AGENDA_TAG, "Was steht an?", "", 60); }
    const char *an = tastatur_ergebnis(ID_AGENDA_TAG);
    if (an && an[0]) { Aktion *a = aktion_neu(A_AGENDA); a->a = (int16_t)t; a->b = ag_tag; a->c = (int16_t)ag_zeit; strncpy(a->text, an, AKTION_TEXT_MAX); mach(a, NULL); }
    /* alte Aktivitäten (vor 1.60.0): nur beendete Treffen zeigen sie, ohne Abstimmung und ohne Bearbeiten */
    int nakt = 0;
    for (int i = 0; i < db.aktivitaeten.n; i++) if (db.aktivitaeten.v[i].treffen == t) nakt++;
    if (tr->beendet_um && nakt) {
        ui_ueberschrift("Aktivitäten", tmp("%d", nakt));
        ui_karte_art(KA_NORMAL, NULL, -1);
        for (int i = 0; i < db.aktivitaeten.n; i++) {
            const Aktivitaet *a = &db.aktivitaeten.v[i];
            if (a->treffen != t) continue;
            const Medium *md = a->medium >= 0 ? &db.medien.v[a->medium] : NULL;
            const char *unter = md ? (md->art == MED_IDEE ? (md->gastgeber >= 0 ? tmp("Idee bei %s", pname(md->gastgeber)) : "Idee") : "Bucketliste")
                                   : tmp("Freitext von %s", a->profil == ich ? "dir" : pname(a->profil));
            ui_info_symbol(a->erledigt ? IC_CHECK : IC_X, md ? md->titel : a->titel, tmp("%s · %s%s", unter, a->erledigt ? "erledigt" : "nicht geklappt", a->stimmen ? tmp(" · %d %s", a->stimmen, a->stimmen == 1 ? "Stimme" : "Stimmen") : ""));
        }
        ui_karte_ende();
    }
    int nk = 0;
    for (int i = 0; i < db.kommentare.n; i++) if (!db.kommentare.v[i].geloescht && db.kommentare.v[i].treffen == t) nk++;
    if (nk || st == T_KOMMEND) {
        ui_ueberschrift("Kommentare", NULL);
        for (int i = 0; i < db.kommentare.n; i++) {
            const Kommentar *k = &db.kommentare.v[i];
            if (k->geloescht || k->treffen != t) continue;
            if (ui_eintrag(k->profil, tmp("%s%s", pname(k->profil), k->angepinnt ? " · angeheftet" : ""), k->text, NULL) && k->profil == ich)
                frage(ID_KOMMENTAR * 1000 + i, "Deinen Kommentar löschen?", "Löschen", "Behalten");
            if (frage_ergebnis(ID_KOMMENTAR * 1000 + i) == 0) { Aktion *x = aktion_neu(A_KOMMENTAR_WEG); x->a = (int16_t)i; mach(x, NULL); }
        }
        if (st == T_KOMMEND && ui_knopf_art("Kommentar", KN_TONAL, IC_PLUS)) tastatur(ID_KOMMENTAR, "Dein Kommentar", "", 300);
        const char *kt = tastatur_ergebnis(ID_KOMMENTAR);
        if (kt && kt[0]) { Aktion *a = aktion_neu(A_KOMMENTAR); a->a = (int16_t)t; strncpy(a->text, kt, AKTION_TEXT_MAX); mach(a, NULL); }
    }
    ui_ueberschrift("Räume", NULL);
    int nr = 0;
    for (int r = 0; r < db.raeume.n; r++) {
        if (!raum_im_treffen(r, t)) continue;
        const Raum *x = &db.raeume.v[r];
        int pl = 0; for (int p = 0; p < db.plaetze.n; p++) if (!db.plaetze.v[p].weg && db.plaetze.v[p].raum == r) pl++;
        const char *info = x->info ? x->info : "";
        if (ui_eintrag_symbol(x->art == RAUM_BAD ? IC_BATH : IC_BED, NULL, x->name, x->art == RAUM_BAD ? (info[0] ? info : "Bad") : tmp("%d %s%s%s", pl, pl == 1 ? "Schlafplatz" : "Schlafplätze", info[0] ? " · " : "", info), info[0] ? "(i)" : NULL) && info[0]) meldung(info);
        nr++;
    }
    if (!nr) ui_text(tr->gastgeber >= 0 ? "Der Gastgeber hat noch keine Räume eingetragen." : "Noch keine Räume am Treffen.", F_NORMAL, TH->muted);
    if (tr->gastgeber < 0 && hat_komme(t, ich) && !tr->beendet_um) {
        if (ui_knopf_art("Bad", KN_TONAL, IC_PLUS)) tastatur(ID_RAUM + 50, "Neues Bad am Treffen", "", 30);
        if (ui_knopf_neben_art("Schlafraum", KN_TONAL, IC_PLUS)) tastatur(ID_RAUM + 51, "Neuer Schlafraum am Treffen", "", 30);
        for (int k = 0; k < 2; k++) { const char *rt = tastatur_ergebnis(ID_RAUM + 50 + k); if (rt && rt[0]) { Aktion *a = aktion_neu(A_RAUM_NEU); a->b = (int16_t)k; a->c = (int16_t)t; strncpy(a->text, rt, AKTION_TEXT_MAX); mach(a, NULL); } }
    }
    if (st != T_KOMMEND) {
        Rekord r[12]; int n = rekorde(t, r, 12);
        if (n) { ui_ueberschrift("Gold dieses Treffens", NULL); for (int i = 0; i < n; i++) ui_person(r[i].profil, tmp("%s %s", r[i].emoji, r[i].titel), tmp("%s · %s", pname(r[i].profil), r[i].text), NULL); }
    }
}

/* ======================================================================= Mein Heini-Treffen */
static int temperatur_vorschlag(int t, int raum, tag_t nacht, int *wuensche) {
    int summe = 0, n = 0;
    for (int s = 0; s < db.schlaf.n; s++) {
        const Schlaf *x = &db.schlaf.v[s];
        if (x->geloescht || x->treffen != t || x->nacht != nacht || db.plaetze.v[x->platz].raum != raum) continue;
        int w = db.profile.v[x->profil].wunsch_temp;
        if (w) { summe += w; n++; }
    }
    *wuensche = n;
    return n ? (summe * 2 + n) / (2 * n) : 0;
}
static void seite_mein(int t) {
    const Treffen *tr = &db.treffen.v[t];
    seiten_kopf("Mein Heini-Treffen", -1);
    if (treffen_status(t) == T_LAUFEND && ist_anwesend(t, ich) && tr->gastgeber != ich && ui_knopf_art("Treffen verlassen", KN_RAND, IC_LOG_OUT))
        frage(ID_VERLASSEN, "Treffen verlassen? Deine Uhr endet, deine Schlafplätze werden frei und die Warteliste bekommt Bescheid.", "Verlassen", "Bleiben");
    if (frage_ergebnis(ID_VERLASSEN) == 0) { Aktion *a = aktion_neu(A_VERLASSEN); a->a = (int16_t)t; if (mach(a, "Gute Heimfahrt!")) { seite_zurueck(); return; } }
    if (tr->info && tr->info[0]) { ui_ueberschrift("Gut zu wissen", NULL); ui_karte(NULL, -1); ui_symbolzeile(IC_INFO, tr->info, TH->text); ui_karte_ende(); }
    if (tr->bis > tr->von && !ist_gastgeber(t, ich)) {
        ui_ueberschrift("Schlafplatz", NULL);
        for (tag_t nacht = tr->von; nacht < tr->bis; nacht++) {
            ui_text(tmp("Nacht von %s auf %s", datum_text(nacht), datum_text(nacht + 1)), F_FETT, TH->muted);
            int np = 0;
            for (int r = 0; r < db.raeume.n; r++) {
                if (db.raeume.v[r].art != RAUM_SCHLAF || !raum_im_treffen(r, t)) continue;
                for (int p = 0; p < db.plaetze.n; p++) {
                    const Platz *pl = &db.plaetze.v[p];
                    if (pl->weg || pl->raum != r) continue;
                    np++;
                    int auf[2], n = liegen_auf(t, p, nacht, auf), mein = schlaf_von(t, ich, nacht);
                    int ist_meiner = mein >= 0 && db.schlaf.v[mein].platz == p, voll = n >= (pl->fuer_zwei ? 2 : 1);
                    const char *wer = ist_meiner ? (n > 1 ? tmp("deiner · mit %s", pname(auf[0] == ich ? auf[1] : auf[0])) : "deiner") : n ? (n > 1 ? tmp("%s & %s", pname(auf[0]), pname(auf[1])) : pname(auf[0])) : "frei";
                    if (ui_eintrag_symbol(IC_BED, NULL, tmp("%s · %s", pl->name, wer), tmp("%s%s", db.raeume.v[r].name, pl->fuer_zwei ? " · für zwei" : ""), ist_meiner ? "Freigeben" : voll ? "voll" : "Nehmen")) {
                        if (voll && !ist_meiner) meldung("Zu spät, das Sofa ist weg. Isomatte?");
                        else { Aktion *a = aktion_neu(ist_meiner ? A_SCHLAF_WEG : A_SCHLAF); a->a = (int16_t)t; a->b = (int16_t)p; a->c = nacht; mach(a, ist_meiner ? "Platz freigegeben." : tmp("Gute Nacht auf „%s“!", pl->name)); }
                    }
                }
            }
            if (!np) ui_text("Keine Schlafplätze eingetragen.", F_NORMAL, TH->muted);
        }
    }
    int besitzer = tr->gastgeber >= 0 ? raum_besitzer(tr->gastgeber) : -1, nu = 0;
    for (int u = 0; u < db.utensilien.n; u++) {
        const Utensil *x = &db.utensilien.v[u];
        if (besitzer < 0 || x->weg || x->profil != besitzer) continue;
        if (!nu++) ui_ueberschrift("Ausleihen", NULL);
        int weg = ausgeliehen(t, u), i = ausleihe_von(t, u, ich), mein = i >= 0 ? db.ausleihen.v[i].anzahl : 0;
        ui_karte(NULL, -1);
        text(ui_x(), ui_y(), x->name, F_FETT, TH->text);
        text(ui_x(), ui_y() + 22, tmp("%d von %d frei", x->anzahl - weg, x->anzahl), F_KLEIN, TH->muted);
        ui_abstand(48);
        if (ui_knopf_art("", KN_KLEIN, IC_MINUS) && mein > 0) { Aktion *a = aktion_neu(A_AUSLEIHE); a->a = (int16_t)t; a->b = (int16_t)u; a->c = (int16_t)(mein - 1); mach(a, NULL); }
        if (ui_knopf_neben_art(tmp("%d", mein), KN_KLEIN, -1)) {}
        if (ui_knopf_neben_art("", KN_KLEIN, IC_PLUS)) { Aktion *a = aktion_neu(A_AUSLEIHE); a->a = (int16_t)t; a->b = (int16_t)u; a->c = (int16_t)(mein + 1); mach(a, NULL); }
        ui_karte_ende();
    }
    tag_t h = tag_von(jetzt);
    ui_ueberschrift("Heute für dich", NULL);
    int n = 0;
    for (int i = 0; i < db.agenda.n; i++) {
        const Agenda *a = &db.agenda.v[i];
        if (a->geloescht || a->treffen != t || a->datum != h) continue;
        ui_person(a->profil, a->name, a->uhrzeit >= 0 ? tmp("%02d:%02d Uhr", a->uhrzeit / 60, a->uhrzeit % 60) : "heute", NULL); n++;
    }
    for (int b = 0; b < db.buchungen.n; b++)
        if (gehoert(b, ich) && db.buchungen.v[b].treffen == t && tag_von(db.buchungen.v[b].start) == h) { ui_eintrag_symbol(IC_BATH, NULL, db.raeume.v[db.buchungen.v[b].raum].name, tmp("%s–%s", uhr_text(db.buchungen.v[b].start), uhr_laeuft(b) ? "jetzt" : uhr_text(buchung_ende(b))), NULL); n++; }
    if (ist_gastgeber(t, ich) && minute_von(jetzt) >= 18 * 60 && h < tr->bis) {
        for (int r = 0; r < db.raeume.n; r++) {
            if (db.raeume.v[r].art != RAUM_SCHLAF || !raum_im_treffen(r, t)) continue;
            int w, v = temperatur_vorschlag(t, r, h, &w);
            if (!w) continue;
            int erl = -1; for (int i = 0; i < db.temperatur.n; i++) if (db.temperatur.v[i].treffen == t && db.temperatur.v[i].raum == r && db.temperatur.v[i].nacht == h) erl = i;
            if (ui_eintrag_symbol(IC_THERMOMETER, NULL, tmp("%s auf %d °C stellen", db.raeume.v[r].name, v), tmp("%d %s · %s", w, w == 1 ? "Wunsch" : "Wünsche", erl >= 0 ? "erledigt" : "A: abhaken"), erl >= 0 ? "✓" : NULL) && erl < 0) {
                Aktion *a = aktion_neu(A_TEMPERATUR); a->a = (int16_t)t; a->b = (int16_t)r; a->c = h; a->d = (int16_t)v; mach(a, "Abgehakt. Gute Nacht!");
            }
            n++;
        }
    }
    if (!n) ui_text("Heute steht nichts für dich an.", F_NORMAL, TH->muted);
    ui_bereich_ende();
}

/* ======================================================================= Ideen */
static void seite_ideen(void) {
    static const char *const R[3] = { "Medien", "Ideen", "Bucketliste" };
    int w = ui_segment(R, 3, ideen_reiter);
    if (w >= 0) { ideen_reiter = w; nochmal_zeichnen = 1; }
    if (ideen_reiter == 0 && ESHOP_LINK[0]) { ui_karte("Heini-eShop", IC_SHOPPING_BAG); ui_text(ESHOP_LINK, F_NORMAL, TH->accentText); ui_karte_ende(); }
    if (ideen_reiter > 0 && ui_knopf_art(ideen_reiter == 1 ? "Idee" : "Bucketliste", KN_VOLL, IC_PLUS))
        tastatur(ID_IDEE, ideen_reiter == 1 ? "Deine Idee (bei dir als Gastgeber)" : "Was wollt ihr unbedingt mal machen?", "", 80);
    const char *it = tastatur_ergebnis(ID_IDEE);
    if (it && it[0]) { Aktion *a = aktion_neu(A_IDEE); a->b = (int16_t)(ideen_reiter == 1 ? MED_IDEE : MED_BUCKET); strncpy(a->text, it, AKTION_TEXT_MAX); mach(a, NULL); }
    int n = 0;
    static const int ICM[5] = { IC_YOUTUBE, IC_MUSIC, IC_LINK, IC_LIGHTBULB, IC_FLAG };
    for (int erledigt = 0; erledigt < 2; erledigt++)
        for (int i = 0; i < db.medien.n; i++) {
            const Medium *m = &db.medien.v[i];
            int passt = ideen_reiter == 0 ? (m->art == MED_YOUTUBE || m->art == MED_SPOTIFY || m->art == MED_LINK) : ideen_reiter == 1 ? m->art == MED_IDEE : m->art == MED_BUCKET;
            if (m->geloescht || !passt || (m->erledigt_um != 0) != erledigt) continue;
            n++;
            int hoch = 0, runter = 0, mein = 0;
            for (int d = 0; d < db.daumen.n; d++) if (db.daumen.v[d].medium == i) { if (db.daumen.v[d].wert > 0) hoch++; else if (db.daumen.v[d].wert < 0) runter++; if (db.daumen.v[d].profil == ich) mein = db.daumen.v[d].wert; }
            const char *unter = m->art == MED_IDEE ? tmp("bei %s%s", pname(m->gastgeber), m->erledigt_um ? " · erledigt" : "") : m->adresse && m->adresse[0] ? m->adresse : (m->erledigt_um ? "erledigt" : pname(m->profil));
            if (ui_eintrag_symbol(ICM[m->art], m->symbol, tmp("%s%s", m->erledigt_um ? "✓ " : "", m->titel), unter, tmp("👍 %d%s", hoch, runter ? tmp("  👎 %d", runter) : ""))) {
                static const char *const OPT[4] = { "👍 Daumen hoch", "👎 Daumen runter", "Daumen zurücknehmen", "Erledigt an/aus" };
                auswahl(ID_MEDIUM * 1000 + i, m->titel, OPT, 4, mein > 0 ? 2 : 0);
            }
            int o = auswahl_ergebnis(ID_MEDIUM * 1000 + i);
            if (o >= 0 && o < 3) { Aktion *a = aktion_neu(A_DAUMEN); a->a = (int16_t)i; a->b = (int16_t)(o == 0 ? 1 : o == 1 ? -1 : 0); mach(a, NULL); }
            if (o == 3) { Aktion *a = aktion_neu(A_ERLEDIGT); a->a = (int16_t)i; a->b = !m->erledigt_um; mach(a, NULL); }
        }
    if (!n) ui_text("Noch nichts eingetragen.", F_NORMAL, TH->muted);
    if (ideen_reiter == 0) ui_text("Links lassen sich auf der N64 nicht öffnen (kein Internet) – Titel und Adresse stehen zum Abschreiben da.", F_KLEIN, TH->muted);
}

/* ======================================================================= Klopapier-Alarm */
static void seite_alarm(int al) {
    const Alarm *a = &db.alarme.v[al];
    gfx_fuellen(TH->alarm);
    gfx_bild4444(BILD_B / 2 - 48, 50, 96, 96, klopapier96);
    text_mitte(BILD_B / 2, 166, "Klopapier-Alarm!", F_GROSS, C_WEISS);
    text_mitte(BILD_B / 2, 216, tmp("%s: %s braucht Klopapier!", a->raum >= 0 ? db.raeume.v[a->raum].name : "Bad", pname(a->profil)), F_TITEL, C_WEISS);
    ui_bereich(270, BILD_H);
    if (ui_knopf_breit("Ich bring welche", KN_HELL, -1)) { Aktion *x = aktion_neu(A_ALARM_BRING); x->a = (int16_t)al; mach(x, "Held des Tages!"); alarm_gesehen = al; seite_zurueck(); }
    else if (ui_knopf_breit("Später", KN_GEFAHR, IC_VOLUME_X)) { alarm_gesehen = al; seite_zurueck(); }
    ui_bereich_ende();
}

/* ======================================================================= Rahmen */
static void menue(void) {
    static const char *const M[8] = { "Chat", "Ruhmeshalle", "Wer ist da?", "Mein Profil", "Einstellungen", "Änderungsverlauf", "Profil wechseln", "Zurück" };
    auswahl(ID_MENUE, tmp("Menü · %s", pname(ich)), M, 8, 0);
}
static void menue_ergebnis(void) {
    switch (auswahl_ergebnis(ID_MENUE)) {
    case 0: chat_ende = 1; seite_auf(SE_CHAT, 0); break;
    case 1: ruhm_reiter = 0; seite_auf(SE_RUHM, 0); break;
    case 2: seite_auf(SE_WERDA, 0); break;
    case 3: seite_auf(SE_PROFIL, ich); break;
    case 4: seite_auf(SE_EINST, 0); break;
    case 5: seite_auf(SE_VERLAUF, 0); break;
    case 6: tiefe = 1; seite_ersetzen(SE_PROFILWAHL, 0); break;
    }
}

void seiten_start(void) { tiefe = 0; logo_start = (int)plat_ms(); seite_auf(SE_LOGO, 0); plan_tag = tag_von(jetzt); }

void seiten_zeichnen(void) {
    int s = seite_aktuell();
    uint32_t t = ui_taste();
    thema_setzen(ich >= 0 ? db.profile.v[ich].farbe : 4, design_dunkel);
    int l = laufendes_treffen();
    if (ich >= 0 && l >= 0 && s != SE_ALARM && s != SE_PROFILWAHL && s != SE_LOGO && !ui_dialog_offen()) {
        int al = offener_alarm(l);
        if (al >= 0 && al != alarm_gesehen && db.alarme.v[al].profil != ich && ist_anwesend(l, ich)) { seite_auf(SE_ALARM, al); return; }
    }
    if (s == SE_ALARM && (param() >= db.alarme.n || db.alarme.v[param()].erledigt_um || db.alarme.v[param()].geloescht)) { seite_zurueck(); return; }
    besuch_flow();
    switch (s) {
    case SE_LOGO: seite_logo(); return;
    case SE_PROFILWAHL: seite_profilwahl(); return;
    case SE_ALARM: seite_alarm(param()); return;
    case SE_HAUPT: {
        if (t & (T_L | T_R)) { reiter = (reiter + ((t & T_R) ? 1 : 4)) % 5; ui_seite_neu(); nochmal_zeichnen = 1; }
        gfx_fuellen(TH->bg);
        static const char *const TITEL[5] = { "Heute", "Plan", "Kasse & Trips", "Treffen", "Aktivitäten" };
        int k = kopf_haupt(TITEL[reiter]);
        ui_bereich(KOPF_HAUPT, LEISTE_Y);
        switch (reiter) {
        case R_HEUTE: seite_heute(); break;
        case R_PLAN: seite_plan(); break;
        case R_KASSE: seite_kasse(); break;
        case R_TREFFEN: seite_treffenliste(); break;
        case R_IDEEN: seite_ideen(); break;
        }
        ui_bereich_ende();
        int r = reiterleiste(reiter);
        if (r >= 0 && r != reiter) { reiter = r; ui_seite_neu(); nochmal_zeichnen = 1; }
        if (k == KOPF_RUHM) { ruhm_reiter = 0; seite_auf(SE_RUHM, 0); }
        if (k == KOPF_CHAT || (t & T_Z)) { chat_ende = 1; seite_auf(SE_CHAT, 0); }
        if (k == KOPF_ICH) seite_auf(SE_PROFIL, ich);
        if (k == KOPF_WERDA) seite_auf(SE_WERDA, 0);
        if (t & (T_START | T_B)) menue();
        menue_ergebnis();
        return;
    }
    case SE_CHAT: seite_chat(); break;
    case SE_RUHM: seite_ruhm(); break;
    case SE_PROFIL: seite_profil(param()); break;
    case SE_TREFFEN: seite_treffen(param()); ui_bereich_ende(); break;
    case SE_MEIN: seite_mein(param()); break;
    case SE_FRUEHER: seite_frueher(); break;
    case SE_EINST: seite_einst(); break;
    case SE_WERDA: seite_werda(); break;
    case SE_VERLAUF: seite_verlauf(); break;
    case SE_TOUR: seite_tour(); break;
    case SE_BILDWAHL: seite_bildwahl(); break;
    case SE_FARBWAHL: seite_farbwahl(); break;
    case SE_RAEUME: seite_raeume(); break;
    case SE_GAMING: seite_gaming(); break;
    case SE_TREFFEN_FORM: seite_treffen_form(param()); break;
    }
    if (seite_aktuell() == s && s != SE_TOUR) {
        if ((t & T_B) && tiefe > 1) seite_zurueck();
        if ((t & T_START) && ich >= 0) menue();
        menue_ergebnis();
    }
}
