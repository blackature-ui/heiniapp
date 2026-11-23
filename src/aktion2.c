/* aktion2.c – weitere Abläufe der Heini-App: Treffen anlegen, Profil, Räume, Konsolen, Wichtel, Pärchen,
 * Trips und Fahrtkosten, Essensbestellung, Bad-Besuch bearbeiten, Wunschtemperatur, Design */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "aktion.h"
#include "logik.h"
#include "text.h"

#define T_FREMD "Finger weg, das gehört dir nicht."
#define T_NUR_KOMME "Erst „Komme“ sagen, dann mitreden. Bearbeiten, absagen und beenden darf jeder mit „Komme“."

int text_teile(char *s, char **teile, int max) {
    int n = 0;
    teile[n++] = s;
    for (char *q = s; *q && n < max; q++) if (*q == '\x1f') { *q = 0; teile[n++] = q + 1; }
    for (int i = n; i < max; i++) teile[i] = "";
    return n;
}
static int platz_besitzer(int p) { return raum_besitzer(p); }
static int auto_von(int p) { return p >= 0 && db.profile.v[p].auto_; }

static const char *treffen_pruefen(int selbst, int gg, tag_t von, tag_t bis, int ausserhalb, const char *titel) {
    if (bis < von) return "„Bis“ liegt vor „von“.";
    if (bis - von + 1 > 30) return "Höchstens 30 Tage.";
    if (gg < 0 && !titel[0]) return "Auswärts braucht das Treffen einen Titel.";
    if (gg >= 0) {
        const Profil *g = &db.profile.v[raum_besitzer(gg)];
        if (!db.profile.v[gg].gastgeber) return "Diese Person kann kein Gastgeber sein.";
        if (!ausserhalb && (!g->strasse[0] || strlen(g->plz) != 5 || !g->wohnort[0])) return "Als Gastgeber brauchst du eine Adresse: Straße, PLZ und Wohnort.";
    }
    for (int i = 0; i < db.treffen.n; i++) {
        const Treffen *t = &db.treffen.v[i];
        if (i == selbst || t->geloescht || t->beendet_um) continue;
        if (von <= t->bis && t->von <= bis)
            return tmp("Da ist schon ein Treffen: %s (%s bis %s). Zwei Treffen gleichzeitig gehen nicht.", treffen_titel(i), datum_voll(t->von), datum_voll(t->bis));
    }
    return NULL;
}

/* Fahrtkosten-Teilnehmer eines Fahrers in einem Trip (hin und zurück zusammen) */
static uint32_t trip_insassen(int trip, int fahrer, int *hin, int *rueck) {
    uint32_t m = 0; *hin = *rueck = 0;
    for (int f = 0; f < db.fahrten.n; f++) {
        if (db.fahrten.v[f].geloescht || db.fahrten.v[f].trip != trip || db.fahrten.v[f].profil != fahrer) continue;
        int ins[16], n = fahrt_insassen(f, ins, 16);
        for (int i = 0; i < n; i++) m |= 1u << ins[i];
        if (db.fahrten.v[f].richtung) *rueck = 1; else *hin = 1;
    }
    return m;
}
static int trip_abgerechnet(int trip, int fahrer) {
    for (int a = 0; a < db.ausgaben.n; a++)
        if (!db.ausgaben.v[a].geloescht && db.ausgaben.v[a].trip == trip && db.ausgaben.v[a].profil == fahrer) return a;
    return -1;
}

static int zeichen_n(const char *s, int bytes) { int n = 0; for (int i = 0; i < bytes; i++) if (((unsigned char)s[i] & 0xC0) != 0x80) n++; return n; }

const char *ausfuehren2(Aktion *a) {
    int p = a->profil;
    char puf[AKTION_TEXT_MAX + 2]; char *t[8];
    strcpy(puf, a->text);
    text_teile(puf, t, 8);
    switch (a->typ) {
    case A_DESIGN: design_dunkel = a->b ? 1 : 0; return NULL;

    /* ---------- Treffen ---------- */
    case A_TREFFEN_NEU: case A_TREFFEN_AENDERN: {
        int sel = a->typ == A_TREFFEN_AENDERN ? a->d : -1;
        if (sel >= 0) {
            if (sel >= db.treffen.n || db.treffen.v[sel].geloescht) return "Treffen nicht gefunden.";
            if (db.treffen.v[sel].beendet_um) return "Das Treffen ist schon vorbei.";
            if (treffen_status(sel) != T_KOMMEND && !hat_komme(sel, p)) return T_NUR_KOMME;
            if (treffen_status(sel) != T_KOMMEND && db.treffen.v[sel].gastgeber != a->a) return "Den Gastgeber wechselt man nur, bevor das Treffen anfängt.";
        }
        const char *f = treffen_pruefen(sel, a->a, (tag_t)a->b, (tag_t)a->c, (int)(a->maske & 1), t[0]);
        if (f) return f;
        Treffen *x;
        if (sel < 0) { x = TAB_NEU(db.treffen); memset(x, 0, sizeof *x); x->ersteller = (int8_t)p; x->gastgeber2 = -1; sel = db.treffen.n - 1; }
        else x = &db.treffen.v[sel];
        x->titel = text_kopie(t[0]); x->adresse = text_kopie(t[1]); x->info = text_kopie(t[2]);
        x->gastgeber = (int8_t)a->a; x->von = (tag_t)a->b; x->bis = (tag_t)a->c; x->ausserhalb = (uint8_t)(a->maske & 1);
        if (x->gastgeber >= 0) { int w = wohnt_mit(x->gastgeber); x->gastgeber2 = (int8_t)(w >= 0 ? w : -1); }
        if (a->typ == A_TREFFEN_NEU) system_nachricht(tmp(S_HAUS " %s hat ein Treffen angelegt: %s (%s)", pname(p), treffen_titel(sel), zeitraum_text(x->von, x->bis)));
        return NULL;
    }
    case A_TREFFEN_ABSAGEN: {
        int tr = a->a;
        if (tr < 0 || tr >= db.treffen.n || db.treffen.v[tr].geloescht) return "Treffen nicht gefunden.";
        if (db.treffen.v[tr].beendet_um) return "Ein beendetes Treffen bleibt. Es steht unter „Frühere Treffen“.";
        if (treffen_status(tr) != T_KOMMEND && !hat_komme(tr, p)) return T_NUR_KOMME;
        for (int b = 0; b < db.buchungen.n; b++) if (db.buchungen.v[b].treffen == tr && uhr_laeuft(b)) { db.buchungen.v[b].fertig = jetzt; }
        db.treffen.v[tr].geloescht = 1;
        system_nachricht(tmp("%s hat „%s“ abgesagt.", pname(p), treffen_titel(tr)));
        return NULL;
    }

    /* ---------- Profil ---------- */
    case A_PROFIL: {
        Profil *x = &db.profile.v[p];
        int v = (int)a->betrag;
        switch (a->a) {
        case P_FARBE: if (v < 0 || v >= FARBEN_N) return "Diese Farbe gibt es nicht."; x->farbe = (uint8_t)v; break;
        case P_BILD:
            if (v < 0 || v > 48) return "Dieses Bild gibt es nicht.";
            for (int q = 0; q < db.profile.n; q++) if (q != p && v && db.profile.v[q].bild == v) return tmp("Das Bild hat sich gerade %s geschnappt.", pname(q));
            x->bild = (uint8_t)v; break;
        case P_WOHNORT: x->wohnort = text_kopie(t[0]); break;
        case P_STRASSE: x->strasse = text_kopie(t[0]); break;
        case P_PLZ: if (t[0][0] && (strlen(t[0]) != 5 || strspn(t[0], "0123456789") != 5)) return "Die PLZ hat 5 Ziffern."; x->plz = text_kopie(t[0]); break;
        case P_GASTGEBER:
            if (v && (!x->strasse[0] || strlen(x->plz) != 5 || !x->wohnort[0])) return "Als Gastgeber brauchst du eine Adresse: Straße, PLZ und Wohnort.";
            x->gastgeber = v ? 1 : 0; break;
        case P_AUTO: x->auto_ = v ? 1 : 0; break;
        case P_AUTO_NAME: x->auto_name = text_kopie(t[0]); break;
        case P_AUTO_PLAETZE: if (v < 1 || v > 8) return "1 bis 8 Plätze."; x->auto_plaetze = (uint8_t)v; break;
        case P_VERBRAUCH: if (v < 0 || v > 999) return "Verbrauch zwischen 0 und 99,9."; x->verbrauch = (int16_t)v; break;
        case P_ELEKTRO: x->elektro = v ? 1 : 0; break;
        case P_KEIN_ALKOHOL: x->kein_alkohol = v ? 1 : 0; break;
        case P_PAYPAL: x->paypal = text_kopie(t[0]); break;
        case P_WUNSCH_TEMP: if (v && (v < 15 || v > 24)) return "Ganze Grad von 15 bis 24 °C oder egal."; x->wunsch_temp = (int8_t)v; break;
        case P_PARKPLAETZE: if (v < 0 || v > 20) return "0 bis 20 Parkplätze. Mehr passt in keine Einfahrt."; x->parkplaetze = (uint8_t)v; break;
        case P_PARK_HINWEIS: x->park_hinweis = text_kopie(t[0]); break;
        case P_UNVERTRAEGLICH: {
            const char *q = t[0]; int n = 0;
            while (*q) {
                const char *e = strstr(q, " · "); int l = e ? (int)(e - q) : (int)strlen(q);
                n++; if (zeichen_n(q, l) > 40) return "Höchstens 40 Zeichen je Eintrag.";
                q += l; if (e) q += strlen(" · ");
            }
            if (n > 20) return "Höchstens 20 Einträge.";
            x->unvertraeglich = text_kopie(t[0]); break;
        }
        case P_MAG_NICHT: {
            /* wie RULES.essenFehler: höchstens 20 Einträge, je 40 Zeichen, keine doppelten */
            const char *q = t[0]; int n = 0;
            while (*q) {
                const char *e = strstr(q, " · "); int l = e ? (int)(e - q) : (int)strlen(q);
                n++; if (zeichen_n(q, l) > 40) return "Höchstens 40 Zeichen je Eintrag.";
                q += l; if (e) q += strlen(" · ");
            }
            if (n > 20) return "Höchstens 20 Einträge.";
            x->mag_nicht = text_kopie(t[0]); break;
        }
        case P_STARTORT: x->startort = text_kopie(t[0]); break;
        case P_AUTO_EINCHECKEN: x->auto_einchecken = v ? 1 : 0; break;
        default: return "Unbekanntes Feld.";
        }
        return NULL;
    }

    /* ---------- Räume, Plätze, Utensilien (gehören dem Raum-Besitzer) ---------- */
    case A_RAUM_NEU: {
        if (!t[0][0]) return "Der Raum braucht einen Namen.";
        Raum *r = TAB_NEU(db.raeume); memset(r, 0, sizeof *r);
        int reih = 0;
        for (int i = 0; i < db.raeume.n - 1; i++) if (db.raeume.v[i].profil == platz_besitzer(p) && db.raeume.v[i].reihenfolge >= reih) reih = db.raeume.v[i].reihenfolge + 1;
        r->profil = (int8_t)platz_besitzer(p); r->treffen = a->c; r->name = text_kopie(t[0]); r->info = text_kopie(t[1]); r->art = (uint8_t)(a->b ? RAUM_SCHLAF : RAUM_BAD); r->reihenfolge = (int8_t)reih;
        return NULL;
    }
    case A_RAUM_AENDERN: case A_RAUM_WEG: {
        if (a->a < 0 || a->a >= db.raeume.n || db.raeume.v[a->a].weg) return "Raum nicht gefunden.";
        Raum *r = &db.raeume.v[a->a];
        if (r->treffen < 0 && r->profil != platz_besitzer(p)) return T_FREMD;
        if (r->treffen >= 0 && !hat_komme(r->treffen, p)) return T_NUR_KOMME;
        if (a->typ == A_RAUM_WEG) {
            r->weg = 1;
            for (int i = 0; i < db.plaetze.n; i++) if (db.plaetze.v[i].raum == a->a) db.plaetze.v[i].weg = 1;
        } else { if (!t[0][0]) return "Der Raum braucht einen Namen."; r->name = text_kopie(t[0]); r->info = text_kopie(t[1]); }
        return NULL;
    }
    case A_PLATZ_NEU: {
        if (a->a < 0 || a->a >= db.raeume.n || db.raeume.v[a->a].weg) return "Raum nicht gefunden.";
        const Raum *r = &db.raeume.v[a->a];
        if (r->treffen < 0 && r->profil != platz_besitzer(p)) return T_FREMD;
        if (!t[0][0]) return "Der Schlafplatz braucht einen Namen.";
        Platz *x = TAB_NEU(db.plaetze); memset(x, 0, sizeof *x);
        x->raum = a->a; x->name = text_kopie(t[0]); x->fuer_zwei = a->b ? 1 : 0;
        return NULL;
    }
    case A_PLATZ_WEG: {
        if (a->a < 0 || a->a >= db.plaetze.n) return NULL;
        const Raum *r = &db.raeume.v[db.plaetze.v[a->a].raum];
        if (r->treffen < 0 && r->profil != platz_besitzer(p)) return T_FREMD;
        db.plaetze.v[a->a].weg = 1;
        for (int i = 0; i < db.schlaf.n; i++) if (db.schlaf.v[i].platz == a->a) db.schlaf.v[i].geloescht = 1;
        return NULL;
    }
    case A_UTENSIL_NEU: {
        if (!t[0][0]) return "Wie heißt das Ding?";
        Utensil *u = TAB_NEU(db.utensilien); memset(u, 0, sizeof *u);
        u->profil = (int8_t)platz_besitzer(p); u->name = text_kopie(t[0]); u->anzahl = (uint8_t)(a->b < 1 ? 1 : a->b); u->zaehlbar = a->b > 1;
        return NULL;
    }
    case A_UTENSIL_AENDERN: case A_UTENSIL_WEG: {
        if (a->a < 0 || a->a >= db.utensilien.n) return NULL;
        Utensil *u = &db.utensilien.v[a->a];
        if (u->profil != platz_besitzer(p)) return T_FREMD;
        if (a->typ == A_UTENSIL_WEG) u->weg = 1;
        else { if (a->b < 1 || a->b > 99) return "1 bis 99 Stück."; u->anzahl = (uint8_t)a->b; u->zaehlbar = a->b > 1; }
        return NULL;
    }

    /* ---------- Konsolen, Spiele, Wichtel ---------- */
    case A_KONSOLE_NEU: {
        if (a->b < 0 && !t[0][0]) return "Wie heißt die Konsole?";
        for (int k = 0; k < db.konsolen.n; k++) if (!db.konsolen.v[k].weg && db.konsolen.v[k].profil == p && a->b >= 0 && db.konsolen.v[k].art == a->b) return "Die Konsole steht schon in deiner Liste.";
        Konsole *k = TAB_NEU(db.konsolen); memset(k, 0, sizeof *k);
        k->profil = (int8_t)p; k->art = a->b; k->name = text_kopie(t[0]); k->pos = (int16_t)db.konsolen.n;
        return NULL;
    }
    case A_KONSOLE_WEG: {
        if (a->a < 0 || a->a >= db.konsolen.n || db.konsolen.v[a->a].profil != p) return T_FREMD;
        db.konsolen.v[a->a].weg = 1;
        return NULL;
    }
    case A_DING_NEU: {
        if (a->a < 0 || a->a >= db.konsolen.n || db.konsolen.v[a->a].profil != p) return T_FREMD;
        if (!t[0][0]) return "Wie heißt es?";
        KonsolenDing *d = TAB_NEU(db.dinge); memset(d, 0, sizeof *d);
        d->konsole = a->a; d->art = (uint8_t)(a->b ? DING_ZUBEHOER : DING_SPIEL); d->name = text_kopie(t[0]); d->anzahl = (uint8_t)(a->c < 1 ? 1 : a->c);
        return NULL;
    }
    case A_DING_WEG: {
        if (a->a < 0 || a->a >= db.dinge.n) return NULL;
        if (db.konsolen.v[db.dinge.v[a->a].konsole].profil != p) return T_FREMD;
        db.dinge.v[a->a].weg = 1;
        return NULL;
    }
    case A_WICHTEL: {
        int j, m, d; tag_zu_datum(tag_von(jetzt), &j, &m, &d);
        if (a->b < 2000 || a->b > j) return tmp("Jahr von 2000 bis %d.", j);
        if (a->c == p) return "Selbst beschenkt zählt nicht. Wähl einen anderen Heini.";
        if (!t[0][0]) return "Was war das Geschenk?";
        for (int i = 0; i < db.wichtel.n; i++) if (!db.wichtel.v[i].weg && db.wichtel.v[i].profil == p && db.wichtel.v[i].jahr == a->b) return tmp("Für %d hast du schon ein Geschenk eingetragen.", a->b);
        Wichtel *w = TAB_NEU(db.wichtel); memset(w, 0, sizeof *w);
        w->profil = (int8_t)p; w->von = (int8_t)a->c; w->jahr = a->b; w->geschenk = text_kopie(t[0]);
        return NULL;
    }
    case A_WICHTEL_WEG: {
        if (a->a < 0 || a->a >= db.wichtel.n || db.wichtel.v[a->a].profil != p) return T_FREMD;
        db.wichtel.v[a->a].weg = 1;
        return NULL;
    }

    /* ---------- Pärchen ---------- */
    case A_PAAR_ANFRAGE: {
        int zu = a->a;
        if (zu < 0 || zu == p || !profil_aktiv(zu)) return "Diese Person gibt es nicht.";
        if (partner_von(p) >= 0 || partner_von(zu) >= 0) return "Schon vergeben! Erst das alte Pärchen lösen.";
        for (int i = 0; i < db.paare.n; i++) {
            Paar *x = &db.paare.v[i];
            if (!x->weg && x->status == PAAR_OFFEN && ((x->von == p && x->zu == zu) || (x->von == zu && x->zu == p))) return "Die Anfrage läuft schon.";
        }
        Paar *x = TAB_NEU(db.paare); memset(x, 0, sizeof *x);
        x->von = (int8_t)p; x->zu = (int8_t)zu; x->wohnen_von = -1; x->wohnen_durch = -1; x->status = PAAR_OFFEN;
        return NULL;
    }
    case A_PAAR_ANTWORT: {
        if (a->a < 0 || a->a >= db.paare.n || db.paare.v[a->a].weg) return "Anfrage nicht gefunden.";
        Paar *x = &db.paare.v[a->a];
        if (x->zu != p) return T_FREMD;
        if (x->status != PAAR_OFFEN) return "Ihr seid schon ein Pärchen.";
        if (!a->b) { x->weg = 1; return NULL; }
        if (partner_von(x->von) >= 0 || partner_von(x->zu) >= 0) return "Schon vergeben! Erst das alte Pärchen lösen.";
        x->status = PAAR_BESTAETIGT;
        for (int i = 0; i < db.paare.n; i++) {
            Paar *o = &db.paare.v[i];
            if (i != a->a && !o->weg && o->status == PAAR_OFFEN && (o->von == x->von || o->zu == x->von || o->von == x->zu || o->zu == x->zu)) o->weg = 1;
        }
        system_nachricht(tmp(S_HERZ " %s und %s sind jetzt ein Pärchen. Das Bad wird eng!", pname(x->von), pname(x->zu)));
        return NULL;
    }
    case A_PAAR_LOESEN: {
        if (a->a < 0 || a->a >= db.paare.n || db.paare.v[a->a].weg) return NULL;
        Paar *x = &db.paare.v[a->a];
        if (x->von != p && x->zu != p) return T_FREMD;
        x->weg = 1;
        return NULL;
    }
    case A_WOHNEN: {
        if (a->a < 0 || a->a >= db.paare.n || db.paare.v[a->a].weg) return "Pärchen nicht gefunden.";
        Paar *x = &db.paare.v[a->a];
        if (x->von != p && x->zu != p) return T_FREMD;
        if (x->status != PAAR_BESTAETIGT) return "Zusammenwohnen geht nur als bestätigtes Pärchen.";
        if (a->b >= 0 && a->b != x->von && a->b != x->zu) return "Wählt die Räume von einem von euch beiden.";
        x->wohnen_von = (int8_t)a->b; x->wohnen_durch = (int8_t)p; x->wohnen_um = jetzt;
        if (a->b >= 0) {   /* Gastgeber-Schalter gilt für beide */
            int gg = db.profile.v[x->von].gastgeber || db.profile.v[x->zu].gastgeber;
            db.profile.v[x->von].gastgeber = db.profile.v[x->zu].gastgeber = (uint8_t)gg;
        }
        return NULL;
    }

    /* ---------- Trips, Autos, Fahrtkosten ---------- */
    case A_TRIP_NEU: {
        int tr = a->a;
        if (tr < 0 || treffen_status(tr) == T_WEG) return "Treffen nicht gefunden.";
        if (db.treffen.v[tr].kasse_zu_um) return "Die Kasse ist zu, Trips lassen sich nicht mehr bearbeiten.";
        if (!hat_komme(tr, p)) return "Erst „Komme“ sagen, dann Trips planen.";
        if (!t[0][0]) return "Wohin geht der Trip?";
        if (a->b <= 0 || a->c < 0) return "Wann geht der Trip los? Wähl Tag und Uhrzeit.";
        Trip *x = TAB_NEU(db.trips); memset(x, 0, sizeof *x);
        x->treffen = (int16_t)tr; x->profil = (int8_t)p; x->datum = (tag_t)a->b; x->uhrzeit = a->c; x->titel = text_kopie(t[0]); x->wann = "";
        return NULL;
    }
    case A_TRIP_WEG: {
        if (a->a < 0 || a->a >= db.trips.n || db.trips.v[a->a].geloescht) return NULL;
        Trip *x = &db.trips.v[a->a];
        if (db.treffen.v[x->treffen].kasse_zu_um) return "Die Kasse ist zu, Trips lassen sich nicht mehr bearbeiten.";
        for (int f = 0; f < db.fahrten.n; f++) if (!db.fahrten.v[f].geloescht && db.fahrten.v[f].trip == a->a && trip_abgerechnet(a->a, db.fahrten.v[f].profil) >= 0)
            return tmp("%s hat in diesem Trip schon abgerechnet. Erst die Fahrtkosten löschen, dann lässt sich der Trip löschen.", pname(db.fahrten.v[f].profil));
        x->geloescht = 1;
        for (int f = 0; f < db.fahrten.n; f++) if (db.fahrten.v[f].trip == a->a) db.fahrten.v[f].geloescht = 1;
        return NULL;
    }
    case A_FAHRT_NEU: {
        int trip = a->a;
        if (trip < 0 || trip >= db.trips.n || db.trips.v[trip].geloescht) return "Trip nicht gefunden.";
        const Trip *x = &db.trips.v[trip];
        if (db.treffen.v[x->treffen].kasse_zu_um) return "Die Kasse ist zu, Trips lassen sich nicht mehr bearbeiten.";
        if (!auto_von(p)) return "Ohne Auto keine Fahrt. Trag dein Auto erst im Profil ein.";
        if (trip_abgerechnet(trip, p) >= 0) return "Für diesen Trip sind deine Fahrtkosten schon abgerechnet.";
        for (int f = 0; f < db.fahrten.n; f++) {
            if (db.fahrten.v[f].geloescht || db.fahrten.v[f].trip != trip || db.fahrten.v[f].richtung != a->b) continue;
            int ins[16], n = fahrt_insassen(f, ins, 16);
            for (int i = 0; i < n; i++) if (ins[i] == p) return "Du fährst schon mit. Zwei Autos auf einmal schafft keiner.";
        }
        Fahrt *fa = TAB_NEU(db.fahrten); memset(fa, 0, sizeof *fa);
        fa->treffen = x->treffen; fa->trip = (int16_t)trip; fa->profil = (int8_t)p; fa->richtung = (uint8_t)(a->b ? 1 : 0);
        fa->plaetze = (uint8_t)(db.profile.v[p].auto_plaetze > 1 ? db.profile.v[p].auto_plaetze - 1 : 4);
        return NULL;
    }
    case A_FAHRT_WEG: case A_MITFAHREN: case A_AUSSTEIGEN: {
        int f = a->a;
        if (f < 0 || f >= db.fahrten.n || db.fahrten.v[f].geloescht) return "Auto nicht gefunden.";
        Fahrt *fa = &db.fahrten.v[f];
        if (db.treffen.v[fa->treffen].kasse_zu_um) return "Die Kasse ist zu, Trips lassen sich nicht mehr bearbeiten.";
        if (fa->trip >= 0 && trip_abgerechnet(fa->trip, fa->profil) >= 0) return tmp("%s hat dieses Auto schon abgerechnet. Erst die Fahrtkosten löschen, dann lässt es sich bearbeiten.", pname(fa->profil));
        if (a->typ == A_FAHRT_WEG) { if (fa->profil != p) return T_FREMD; fa->geloescht = 1; return NULL; }
        if (a->typ == A_AUSSTEIGEN) {
            for (int i = 0; i < db.mitfahrer.n; i++) if (db.mitfahrer.v[i].fahrt == f && db.mitfahrer.v[i].profil == p) db.mitfahrer.v[i].geloescht = 1;
            return NULL;
        }
        int ins[16], n = fahrt_insassen(f, ins, 16);
        for (int i = 0; i < n; i++) if (ins[i] == p) return NULL;
        if (n - 1 >= fa->plaetze) return "Auto voll. Kofferraum geht nicht.";
        for (int g = 0; g < db.fahrten.n; g++) {
            if (g == f || db.fahrten.v[g].geloescht || db.fahrten.v[g].trip != fa->trip || db.fahrten.v[g].richtung != fa->richtung) continue;
            int i2[16], n2 = fahrt_insassen(g, i2, 16);
            for (int i = 0; i < n2; i++) if (i2[i] == p) return "Du fährst schon mit. Zwei Autos auf einmal schafft keiner.";
        }
        Mitfahrer *m = TAB_NEU(db.mitfahrer); m->fahrt = (int16_t)f; m->profil = (int8_t)p; m->geloescht = 0;
        return NULL;
    }
    case A_FAHRTKOSTEN: {
        int trip = a->a;
        if (trip < 0 || trip >= db.trips.n || db.trips.v[trip].geloescht) return "Trip nicht gefunden.";
        int tr = db.trips.v[trip].treffen;
        if (db.treffen.v[tr].kasse_zu_um) return "Kasse ist zu. Zu spät, Geizhals.";
        if (trip_abgerechnet(trip, p) >= 0) return "Für diesen Trip sind deine Fahrtkosten schon abgerechnet.";
        int hin, rueck; uint32_t m = trip_insassen(trip, p, &hin, &rueck);
        if (!m) return "Fahrtkosten rechnet nur der Fahrer für sein eigenes Auto ab.";
        const Profil *x = &db.profile.v[p];
        int km = a->b, preis = (int)a->betrag, versch = regel_verschleiss ? regel_verschleiss_ct * 10 : 0;
        int32_t c = fahrtkosten(km, hin && rueck, x->verbrauch, preis, versch);
        if (c <= 0) return "Strecke, Verbrauch (im Profil) und Preis müssen über 0 liegen.";
        Ausgabe *au = TAB_NEU(db.ausgaben); memset(au, 0, sizeof *au);
        int art = 0; for (int i = 0; i < AUSGABE_ARTEN_N; i++) if (!strcmp(AUSGABE_ARTEN[i].schluessel, "fahrt")) art = i;
        au->treffen = (int16_t)tr; au->trip = (int16_t)trip; au->profil = (int8_t)p; au->eingetragen_von = (int8_t)p; au->betrag = c; au->art = (uint8_t)art;
        au->fuer = m; au->km = (int16_t)km; au->verbrauch = x->verbrauch; au->spritpreis = (int16_t)preis; au->verschleiss = (int16_t)versch; au->hin_zurueck = (uint8_t)(hin && rueck);
        au->titel = text_kopie(db.trips.v[trip].titel);
        return NULL;
    }

    /* ---------- Essensbestellung: t[0] Titel, t[1] „pid:cent:was;…“ ---------- */
    case A_ESSEN: {
        int tr = a->a;
        if (tr < 0 || treffen_status(tr) == T_WEG) return "Treffen nicht gefunden.";
        if (db.treffen.v[tr].kasse_zu_um) return "Kasse ist zu. Zu spät, Geizhals.";
        if (!hat_komme(tr, p)) return "Erst zusagen, dann zahlen.";
        int art = 0; for (int i = 0; i < AUSGABE_ARTEN_N; i++) if (!strcmp(AUSGABE_ARTEN[i].schluessel, "essen")) art = i;
        Ausgabe *au = TAB_NEU(db.ausgaben); memset(au, 0, sizeof *au);
        int ai = db.ausgaben.n - 1;
        au->treffen = (int16_t)tr; au->trip = -1; au->profil = (int8_t)(a->c >= 0 ? a->c : p); au->eingetragen_von = (int8_t)p; au->art = (uint8_t)art;
        au->liefer = a->betrag; au->titel = text_kopie(t[0]);
        int32_t summe = 0; uint32_t m = 0;
        char *q = t[1];
        while (*q) {
            int pid = (int)strtol(q, &q, 10); if (*q != ':') break; q++;
            int32_t ct = (int32_t)strtol(q, &q, 10); if (*q != ':') break; q++;
            char *was = q; while (*q && *q != ';') q++;
            char sp = *q; *q = 0;
            if (pid >= 0 && pid < db.profile.n && ct > 0) {
                Posten *po = TAB_NEU(db.posten); po->ausgabe = (int16_t)ai; po->profil = (int8_t)pid; po->cent = ct; po->was = text_kopie(was);
                summe += ct; m |= 1u << pid;
            }
            if (sp) q++;
        }
        au = &db.ausgaben.v[ai];
        if (!summe) { au->geloescht = 1; return "Wer hat was bestellt? Mindestens ein Betrag fehlt."; }
        au->betrag = summe + (a->betrag > 0 ? a->betrag : 0); au->fuer = m;
        return NULL;
    }

    /* ---------- Bad-Besuch bearbeiten (besuchKorrektur) ---------- */
    case A_BESUCH: {
        int b = a->a;
        if (b < 0 || b >= db.buchungen.n || db.buchungen.v[b].geloescht) return "Buchung nicht gefunden.";
        Buchung *x = &db.buchungen.v[b];
        if (!gehoert(b, p) && x->profil != p && x->partner != p) return T_FREMD;
        if (x->treffen >= 0 && db.treffen.v[x->treffen].beendet_um) return "Das Treffen ist vorbei. Rückblick und Trophäen stehen jetzt fest.";
        if (x->start > jetzt) return "Bearbeiten geht erst, wenn der Besuch angefangen hat.";
        tag_t d = tag_von(x->start);
        zeit_t s = (zeit_t)d * 86400 + a->b * 60, e = (zeit_t)d * 86400 + a->c * 60;
        if (e <= s) { e += 86400; if (e - s > BAD_MAX * 60) return "Das Ende muss nach dem Beginn liegen."; }
        if (e - s > BAD_MAX * 60) return "Ein Besuch dauert höchstens 60 Minuten.";
        if (e > jetzt) return "Das Ende liegt in der Zukunft.";
        for (int i = 0; i < db.buchungen.n; i++) {
            const Buchung *y = &db.buchungen.v[i];
            if (i == b || y->geloescht || y->raum != x->raum || y->profil == x->profil || y->profil == x->partner) continue;
            zeit_t ys = y->start, ye = buchung_ende(i);
            if (ys < e && s < ye) return tmp("Belegt (%s bis %s)", pname(y->profil), uhr_text(ye));
        }
        x->start = s; x->fertig = e;
        if (x->offen) { int dd = (int)((e - s + 59) / 60); x->dauer = (int16_t)(dd < 1 ? 1 : dd); }
        if (x->partner_seit && x->partner_seit < s) x->partner_seit = s;
        if (x->partner_bis && x->partner_bis > e) x->partner_bis = e;
        return NULL;
    }

    /* ---------- Wunschtemperatur abhaken (Gastgeber) ---------- */
    case A_TEMPERATUR: {
        int tr = a->a;
        if (!ist_gastgeber(tr, p)) return "Abhaken kann nur der Gastgeber. Der dreht ja auch am Thermostat.";
        for (int i = 0; i < db.temperatur.n; i++)
            if (db.temperatur.v[i].treffen == tr && db.temperatur.v[i].raum == a->b && db.temperatur.v[i].nacht == a->c) { db.temperatur.v[i].temp = (int8_t)a->d; return NULL; }
        Temperatur *x = TAB_NEU(db.temperatur); x->treffen = (int16_t)tr; x->raum = a->b; x->nacht = (tag_t)a->c; x->temp = (int8_t)a->d;
        return NULL;
    }
    }
    return "Unbekannte Aktion.";
}

/* Zeile für den Änderungsverlauf */
const char *verlauf_satz(const Aktion *a) {
    const char *w = pname(a->profil);
    char puf[AKTION_TEXT_MAX + 2]; char *t[4];
    strcpy(puf, a->text); text_teile(puf, t, 4);
    switch (a->typ) {
    case A_ANKUNFT: return tmp("%s ist angekommen", w);
    case A_EINCHECKEN_FUER: return tmp("%s hat %s eingecheckt", w, pname(a->b));
    case A_VERLASSEN: return tmp("%s hat das Treffen verlassen", w);
    case A_REIN: return tmp("%s ist rein", w);
    case A_FERTIG: return tmp("%s hat das Bad freigegeben", w);
    case A_ALARM: return tmp("%s hat Klopapier-Alarm ausgelöst", w);
    case A_ALARM_BRING: return tmp("%s bringt Klopapier", w);
    case A_ZUSAGE: return tmp("%s: %s", w, a->b == ZUSAGE_KOMME ? "Komme" : a->b == ZUSAGE_VIELLEICHT ? "Vielleicht" : "Komme nicht");
    case A_SCHLAF: return tmp("%s hat einen Schlafplatz genommen", w);
    case A_SCHLAF_WEG: return tmp("%s hat einen Schlafplatz freigegeben", w);
    case A_AUSLEIHE: return tmp("%s hat %s %s", w, a->b >= 0 && a->b < db.utensilien.n ? db.utensilien.v[a->b].name : "etwas", a->c ? "ausgeliehen" : "zurückgegeben");
    case A_AUSGABE: return tmp("%s hat eine Ausgabe eingetragen: %s", w, euro_text(a->betrag));
    case A_AUSGABE_WEG: return tmp("%s hat eine Ausgabe gelöscht", w);
    case A_ESSEN: return tmp("%s hat eine Essensbestellung eingetragen", w);
    case A_KASSE_ZU: return tmp("%s hat die Kasse geschlossen", w);
    case A_KASSE_AUF: return tmp("%s hat die Kasse geöffnet", w);
    case A_ZAHLUNG: return tmp("%s hat eine Zahlung %s", w, a->b == ZAHLUNG_BESTAETIGT ? "bestätigt" : "gemeldet");
    case A_TREFFEN_NEU: return tmp("%s hat das Treffen „%s“ angelegt", w, t[0][0] ? t[0] : "Heini-Treffen");
    case A_TREFFEN_AENDERN: return tmp("%s hat ein Treffen bearbeitet", w);
    case A_TREFFEN_ABSAGEN: return tmp("%s hat ein Treffen abgesagt", w);
    case A_TREFFEN_BEENDEN: return tmp("%s hat ein Treffen beendet", w);
    case A_PROFIL: case A_STATUS: return tmp("%s hat sein Profil bearbeitet", w);
    case A_RAUM_NEU: case A_RAUM_AENDERN: case A_RAUM_WEG: case A_PLATZ_NEU: case A_PLATZ_WEG: return tmp("%s hat Räume bearbeitet", w);
    case A_UTENSIL_NEU: case A_UTENSIL_AENDERN: case A_UTENSIL_WEG: return tmp("%s hat Ausleih-Dinge bearbeitet", w);
    case A_KONSOLE_NEU: case A_KONSOLE_WEG: case A_DING_NEU: case A_DING_WEG: return tmp("%s hat Gaming bearbeitet", w);
    case A_WICHTEL: case A_WICHTEL_WEG: return tmp("%s hat Wichtelgeschenke bearbeitet", w);
    case A_PAAR_ANFRAGE: return tmp("%s hat %s als Pärchen angefragt", w, pname(a->a));
    case A_PAAR_ANTWORT: return tmp("%s hat eine Pärchen-Anfrage %s", w, a->b ? "angenommen" : "abgelehnt");
    case A_PAAR_LOESEN: return tmp("%s hat ein Pärchen gelöst", w);
    case A_WOHNEN: return tmp("%s hat „Wir wohnen zusammen“ geändert", w);
    case A_TRIP_NEU: return tmp("%s hat den Trip „%s“ angelegt", w, t[0]);
    case A_TRIP_WEG: return tmp("%s hat einen Trip gelöscht", w);
    case A_FAHRT_NEU: return tmp("%s fährt mit dem Auto", w);
    case A_MITFAHREN: return tmp("%s fährt mit", w);
    case A_AUSSTEIGEN: return tmp("%s ist ausgestiegen", w);
    case A_FAHRTKOSTEN: return tmp("%s hat Fahrtkosten abgerechnet", w);
    case A_BESUCH: return tmp("%s hat einen Bad-Besuch bearbeitet", w);
    case A_AGENDA: return tmp("%s hat „%s“ in die Agenda eingetragen", w, t[0]);
    case A_KOMMENTAR: return tmp("%s hat einen Kommentar geschrieben", w);
    case A_IDEE: return tmp("%s hat „%s“ eingetragen", w, t[0]);
    case A_REGEL: return tmp("%s hat die Bad-Dauer auf %d Minuten gesetzt", w, a->a);
    case A_TEMPERATUR: return tmp("%s hat die Temperatur abgehakt", w);
    }
    return NULL;
}
