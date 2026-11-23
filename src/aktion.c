/* aktion.c – Abläufe aus server.js (api/rein, api/fertig, api/alarm, api/ankunft, api/save …) und das Journal im SRAM */
#include <string.h>
#include <stdlib.h>
#include "aktion.h"
#include "logik.h"
#include "text.h"
#include "plat.h"

int letzte_neue_nachricht = -1;
static int journal_wird_geladen;

/* Fehlertexte aus rules.js (TEXTE) */
#define T_FREMD "Finger weg, das gehört dir nicht."
#define T_ERST_ANKOMMEN "Erst ankommen, dann thronen."
#define T_SCHON_DRIN "Du sitzt doch schon! Erst freigeben, dann woanders rein."
#define T_BELEGT "Da sitzt schon jemand. Klopf, klopf!"
#define T_ALLE_BESETZT "Alles besetzt. Kneifen und warten!"
#define T_ALARM_NUR_BAD "Den Klopapier-Alarm gibt es, solange deine Uhr im Bad läuft."
#define T_ALARM_NUR_ERSTELLER "Abbrechen darf nur, wer den Alarm ausgelöst hat."
#define T_GASTGEBER_BETT "Du bist der Gastgeber, du hast doch dein eigenes Bett."
#define T_KEIN_PARTNER "Ihr seid (noch) kein Pärchen."
#define T_NUR_KOMME "Erst „Komme“ sagen, dann mitreden. Bearbeiten, absagen und beenden darf jeder mit „Komme“."
#define T_KASSE_OFFEN_BEENDEN "Achtung: Das Treffen kann nicht beendet werden, die Kasse ist noch offen."
#define T_NICHT_DRIN "Du bist gerade in keinem Bad."
#define T_UMFRAGE_ZU "Die Umfrage ist zu. Demokratie hat Feierabend."
#define T_KASSE_ZU "Kasse ist zu. Zu spät, Geizhals."
#define T_TREFFEN_VORBEI "Das Treffen ist schon vorbei."
#define T_PLATZ_WEG "Zu spät, das Sofa ist weg. Isomatte?"
#define T_AUSGELIEHEN "Alle schon weg. Wer zuerst kommt, kuschelt zuerst."
#define T_OHNE_ZUSAGE "Erst „Komme“ sagen, dann Platz schnappen."
#define T_OHNE_ZUSAGE_KASSE "Erst zusagen, dann zahlen."
#define T_GASTGEBER_KOMMT "Du bist der Gastgeber. Du kommst sowieso. " S_HAUS
#define T_NICHT_LAUFEND "Das Treffen läuft gerade nicht."
#define T_KOMMENTAR_ZU "Das Treffen läuft schon. Ab jetzt wird im Chat geredet."
#define T_KOMMENTARE_VOLL "Höchstens 3 Kommentare je Person. Bearbeite einen alten oder lösch ihn."
#define T_AGENDA_NUR_KOMME "Die Agenda bearbeitet, wer „Komme“ gesagt hat."
#define T_AGENDA_VOLL "Höchstens 40 Punkte je Treffen. Ihr wollt doch auch mal chillen."
#define T_AGENDA_TAG "Der Tag gehört nicht zum Treffen."
#define T_EINCHECKEN_NUR_DA "Andere einchecken darf nur, wer selbst schon da ist."
#define T_ANGEHEFTET "Mehr als 3 angeheftet geht nicht."
#define T_REGELN_NUR "Regeln bearbeiten nur Nico und der Gastgeber des laufenden Treffens (auswärts jeder mit „Komme“)."

static Aktion neu_puffer;
Aktion *aktion_neu(int typ) {
    memset(&neu_puffer, 0, sizeof neu_puffer);
    neu_puffer.typ = (uint8_t)typ;
    neu_puffer.profil = (int8_t)ich;
    neu_puffer.zeit = jetzt;
    neu_puffer.a = neu_puffer.b = neu_puffer.c = neu_puffer.d = -1;
    return &neu_puffer;
}

void system_nachricht(const char *text) {
    Nachricht *n = TAB_NEU(db.nachrichten);
    memset(n, 0, sizeof *n);
    n->profil = -1; n->art = NACHRICHT_SYSTEM; n->antwort_auf = -1; n->zeit = jetzt;
    n->text = text_kopie(text);
    letzte_neue_nachricht = db.nachrichten.n - 1;
}
static int nachricht_neu(int p, const char *text, int antwort, int art) {
    Nachricht *n = TAB_NEU(db.nachrichten);
    memset(n, 0, sizeof *n);
    n->profil = (int8_t)p; n->art = (uint8_t)art; n->antwort_auf = (int16_t)antwort; n->zeit = jetzt;
    n->text = text_kopie(text);
    letzte_neue_nachricht = db.nachrichten.n - 1;
    return db.nachrichten.n - 1;
}

/* Aus allen Warteschlangen eines Treffens raus (wer reingeht, verlässt die anderen) */
static void wartelisten_raeumen(int p, int t) {
    for (int i = 0; i < db.warteliste.n; i++) {
        Warte *w = &db.warteliste.v[i];
        if (w->geloescht || w->treffen != t) continue;
        if (w->profil == p) w->geloescht = 1;
        else if (w->partner == p) w->partner = -1;
    }
}
static void uhr_beenden(int b) {
    Buchung *x = &db.buchungen.v[b];
    if (x->fertig) return;
    x->fertig = jetzt > x->start ? jetzt : x->start;
    if (x->offen) {
        int d = (int)((x->fertig - x->start + 30) / 60);
        x->dauer = (int16_t)(d < 1 ? 1 : d > BAD_MAX ? BAD_MAX : d);
    }
    if (x->partner >= 0 && !x->partner_bis) x->partner_bis = x->fertig;
}
/* „Treffen verlassen“ und Absage: laufende Uhr endet, Schlafplätze ab heute frei, Warteschlangen raus */
static void abreise_folgen(int t, int p) {
    for (int i = 0; i < db.buchungen.n; i++) {
        Buchung *b = &db.buchungen.v[i];
        if (b->geloescht || b->treffen != t) continue;
        if (b->profil == p && uhr_laeuft(i)) uhr_beenden(i);
        else if (b->partner == p && !b->partner_bis && uhr_laeuft(i)) b->partner_bis = jetzt;
    }
    for (int i = 0; i < db.schlaf.n; i++) {
        Schlaf *s = &db.schlaf.v[i];
        if (!s->geloescht && s->treffen == t && s->profil == p && s->nacht >= tag_von(jetzt)) s->geloescht = 1;
    }
    wartelisten_raeumen(p, t);
}

static const char *laufend_oder(int t) {
    if (t < 0 || t >= db.treffen.n || db.treffen.v[t].geloescht) return "Treffen nicht gefunden.";
    if (treffen_status(t) != T_LAUFEND) return T_NICHT_LAUFEND;
    return NULL;
}

static const char *ausfuehren(Aktion *a) {
    const char *f;
    int p = a->profil;
    switch (a->typ) {
    case A_UHR:
        return NULL;   /* Zeit ist schon gesetzt */

    case A_ANKUNFT: case A_EINCHECKEN_FUER: {
        int t = a->a, wer = a->typ == A_EINCHECKEN_FUER ? a->b : p;
        if ((f = laufend_oder(t))) return f;
        if (wer != p && !ist_anwesend(t, p) && !ist_gastgeber(t, p)) return T_EINCHECKEN_NUR_DA;
        if (!profil_aktiv(wer)) return "Diese Person gibt es nicht.";
        int i = anwesenheit_von(t, wer);
        if (i >= 0 && !db.anwesenheit.v[i].abgereist) return NULL;
        if (i >= 0) db.anwesenheit.v[i].abgereist = 0;
        else { Anwesenheit *x = TAB_NEU(db.anwesenheit); x->treffen = (int16_t)t; x->profil = (int8_t)wer; x->angekommen = jetzt; x->abgereist = 0; }
        if (wer != db.treffen.v[t].gastgeber) {
            int z = -1;
            for (int k = 0; k < db.zusagen.n; k++) if (db.zusagen.v[k].treffen == t && db.zusagen.v[k].profil == wer) z = k;
            if (z < 0) { Zusage *x = TAB_NEU(db.zusagen); x->treffen = (int16_t)t; x->profil = (int8_t)wer; x->status = ZUSAGE_KOMME; }
            else db.zusagen.v[z].status = ZUSAGE_KOMME;
        }
        system_nachricht(tmp(S_STERN " %s ist angekommen. Der Thron bebt!", pname(wer)));
        return NULL;
    }
    case A_VERLASSEN: {
        int t = a->a;
        if ((f = laufend_oder(t))) return f;
        if (db.treffen.v[t].gastgeber == p) return T_GASTGEBER_BETT;
        int i = anwesenheit_von(t, p);
        if (i < 0 || db.anwesenheit.v[i].abgereist) return T_ERST_ANKOMMEN;
        abreise_folgen(t, p);
        db.anwesenheit.v[i].abgereist = jetzt;
        system_nachricht(tmp("%s ist abgereist. Gute Heimfahrt!", pname(p)));
        return NULL;
    }
    case A_REIN: {
        int t = a->a;
        if ((f = laufend_oder(t))) return f;
        if (!ist_anwesend(t, p)) return T_ERST_ANKOMMEN;
        if (meine_uhr(p) >= 0) return T_SCHON_DRIN;
        int bs[16], nb = baeder(t, bs, 16), raum = -1;
        if (a->b >= 0) {
            int ok = 0;
            for (int i = 0; i < nb; i++) if (bs[i] == a->b) ok = 1;
            if (!ok) return "Diesen Raum gibt es bei diesem Treffen nicht.";
            if (drin_in(a->b, t) >= 0) return T_BELEGT;
            raum = a->b;
        } else {
            for (int i = 0; i < nb && raum < 0; i++) { int w = dran_in(bs[i], t); if (drin_in(bs[i], t) < 0 && w >= 0 && (db.warteliste.v[w].profil == p || db.warteliste.v[w].partner == p)) raum = bs[i]; }
            for (int i = 0; i < nb && raum < 0; i++) if (drin_in(bs[i], t) < 0 && dran_in(bs[i], t) < 0) raum = bs[i];
            for (int i = 0; i < nb && raum < 0; i++) if (drin_in(bs[i], t) < 0) raum = bs[i];
            if (raum < 0) return T_ALLE_BESETZT;
        }
        int vorne = dran_in(raum, t);
        if (vorne >= 0 && db.warteliste.v[vorne].profil != p && db.warteliste.v[vorne].partner != p && a->c != 1)
            return tmp("?%s ist dran. Trotzdem rein?", pname(db.warteliste.v[vorne].profil));
        int mein = warte_platz(p, raum, t), mit = -1;
        if (mein >= 0) {
            const Warte *w = &db.warteliste.v[mein];
            int anderer = w->profil == p ? w->partner : w->profil;
            if (anderer >= 0 && partner_von(p) == anderer && profil_aktiv(anderer) && ist_anwesend(t, anderer) && meine_uhr(anderer) < 0) mit = anderer;
            db.warteliste.v[mein].geloescht = 1;
        }
        Buchung *b = TAB_NEU(db.buchungen);
        memset(b, 0, sizeof *b);
        b->treffen = (int16_t)t; b->raum = (int16_t)raum; b->profil = (int8_t)p; b->partner = (int8_t)mit;
        b->start = jetzt; b->dauer = BAD_MAX; b->offen = 1;
        if (mit >= 0) b->partner_seit = jetzt;
        wartelisten_raeumen(p, t);
        if (mit >= 0) wartelisten_raeumen(mit, t);
        return NULL;
    }
    case A_FERTIG: {
        int b = a->a;
        if (b < 0 || b >= db.buchungen.n || db.buchungen.v[b].geloescht) return "Buchung nicht gefunden.";
        if (!gehoert(b, p)) return T_FREMD;
        if (db.buchungen.v[b].fertig) return NULL;
        uhr_beenden(b);
        return NULL;
    }
    case A_PARTNER: {
        int b = a->a;
        if (b < 0 || b >= db.buchungen.n || !uhr_laeuft(b)) return T_NICHT_DRIN;
        if (!gehoert(b, p)) return T_FREMD;
        Buchung *x = &db.buchungen.v[b];
        if (a->b == 1) {
            if (x->profil != p) return T_FREMD;
            int q = partner_von(p);
            if (q < 0) return T_KEIN_PARTNER;
            if (x->partner == q && !x->partner_bis) return NULL;
            int qu = meine_uhr(q);
            if (!profil_aktiv(q) || !ist_anwesend(x->treffen, q) || (qu >= 0 && qu != b)) return tmp("%s ist gerade nicht da oder schon in einem Bad.", pname(q));
            x->partner = (int8_t)q; x->partner_seit = jetzt; x->partner_bis = 0;
            wartelisten_raeumen(q, x->treffen);
        } else if (x->partner >= 0 && !x->partner_bis) x->partner_bis = jetzt;
        return NULL;
    }
    case A_LANG: {
        int b = a->a;
        if (b < 0 || b >= db.buchungen.n || !uhr_laeuft(b)) return T_NICHT_DRIN;
        if (!gehoert(b, p)) return T_FREMD;
        db.buchungen.v[b].lang = a->b ? 1 : 0;
        return NULL;
    }
    case A_WARTEN: {
        int t = a->a, raum = a->b;
        if ((f = laufend_oder(t))) return f;
        if (!ist_anwesend(t, p)) return T_ERST_ANKOMMEN;
        if (warte_platz(p, raum, t) >= 0) return "Du stehst schon an.";
        int mit = -1;
        if (a->c == 1) {
            mit = partner_von(p);
            if (mit < 0) return T_KEIN_PARTNER;
            if (!ist_anwesend(t, mit)) return tmp("%s ist noch nicht angekommen.", pname(mit));
            if (warte_platz(mit, raum, t) >= 0) return tmp("%s steht schon auf dieser Warteliste.", pname(mit));
        }
        Warte *w = TAB_NEU(db.warteliste);
        w->raum = (int16_t)raum; w->treffen = (int16_t)t; w->profil = (int8_t)p; w->partner = (int8_t)mit; w->seit = jetzt; w->geloescht = 0;
        return NULL;
    }
    case A_NICHT_WARTEN: {
        int w = warte_platz(p, a->b, a->a);
        if (w < 0) return NULL;
        if (db.warteliste.v[w].profil == p) db.warteliste.v[w].geloescht = 1;
        else db.warteliste.v[w].partner = -1;
        return NULL;
    }
    case A_ALARM: {
        int t = a->a;
        if ((f = laufend_oder(t))) return f;
        if (!ist_anwesend(t, p)) return T_ERST_ANKOMMEN;
        int b = meine_uhr(p);
        if (b < 0 || db.buchungen.v[b].treffen != t) return T_ALARM_NUR_BAD;
        if (offener_alarm(t) >= 0) return NULL;
        int raum = db.buchungen.v[b].raum;
        Alarm *x = TAB_NEU(db.alarme);
        x->treffen = (int16_t)t; x->raum = (int16_t)raum; x->profil = (int8_t)p; x->erledigt_von = -1; x->ausgeloest = jetzt; x->erledigt_um = 0; x->geloescht = 0;
        system_nachricht(tmp(S_KLOPAPIER " Klopapier-Alarm im %s! %s braucht Hilfe.", db.raeume.v[raum].name, pname(p)));
        return NULL;
    }
    case A_ALARM_BRING: {
        if (a->a < 0 || a->a >= db.alarme.n || db.alarme.v[a->a].geloescht) return "Alarm nicht gefunden.";
        Alarm *x = &db.alarme.v[a->a];
        if (x->erledigt_um) return tmp("Schon erledigt. %s war schneller.", pname(x->erledigt_von));
        x->erledigt_von = (int8_t)p; x->erledigt_um = jetzt;
        system_nachricht(tmp(S_STERN " %s bringt Klopapier. Held des Tages!", pname(p)));
        return NULL;
    }
    case A_ALARM_ABBRUCH: {
        if (a->a < 0 || a->a >= db.alarme.n || db.alarme.v[a->a].geloescht) return "Alarm nicht gefunden.";
        Alarm *x = &db.alarme.v[a->a];
        if (x->profil != p) return T_ALARM_NUR_ERSTELLER;
        if (x->erledigt_um) return tmp("Schon erledigt. %s war schneller.", pname(x->erledigt_von));
        x->geloescht = 1;
        system_nachricht(tmp("Fehlalarm, %s hat abgebrochen.", pname(p)));
        return NULL;
    }
    case A_ZUSAGE: {
        int t = a->a;
        if (t < 0 || treffen_status(t) == T_WEG) return "Treffen nicht gefunden.";
        if (db.treffen.v[t].beendet_um) return T_TREFFEN_VORBEI;
        if (db.treffen.v[t].gastgeber == p || db.treffen.v[t].gastgeber2 == p) return T_GASTGEBER_KOMMT;
        int z = -1;
        for (int k = 0; k < db.zusagen.n; k++) if (db.zusagen.v[k].treffen == t && db.zusagen.v[k].profil == p) z = k;
        if (z < 0) { Zusage *x = TAB_NEU(db.zusagen); x->treffen = (int16_t)t; x->profil = (int8_t)p; z = db.zusagen.n - 1; }
        db.zusagen.v[z].status = (uint8_t)a->b;
        if (a->b == ZUSAGE_NEIN) {
            abreise_folgen(t, p);
            for (int i = 0; i < db.ausleihen.n; i++) if (db.ausleihen.v[i].treffen == t && db.ausleihen.v[i].profil == p) db.ausleihen.v[i].anzahl = 0;
            for (int i = 0; i < db.schlaf.n; i++) if (db.schlaf.v[i].treffen == t && db.schlaf.v[i].profil == p) db.schlaf.v[i].geloescht = 1;
            int an = anwesenheit_von(t, p);
            if (an >= 0 && !db.anwesenheit.v[an].abgereist) db.anwesenheit.v[an].abgereist = jetzt;
        }
        return NULL;
    }
    case A_SCHLAF: {
        int t = a->a, platz = a->b; tag_t nacht = (tag_t)a->c;
        if (t < 0 || treffen_status(t) == T_WEG || db.treffen.v[t].beendet_um) return T_TREFFEN_VORBEI;
        if (!hat_komme(t, p)) return T_OHNE_ZUSAGE;
        if (ist_gastgeber(t, p)) return T_GASTGEBER_BETT;
        if (nacht < db.treffen.v[t].von || nacht >= db.treffen.v[t].bis) return "Diese Nacht gehört nicht zum Treffen.";
        int auf[2], n = liegen_auf(t, platz, nacht, auf);
        for (int i = 0; i < n; i++) if (auf[i] == p) return NULL;
        int max = db.plaetze.v[platz].fuer_zwei ? 2 : 1;
        if (n >= max) return max == 2 ? tmp("Auf „%s“ liegen schon zwei.", db.plaetze.v[platz].name) : T_PLATZ_WEG;
        int alt = schlaf_von(t, p, nacht);
        if (alt >= 0) db.schlaf.v[alt].geloescht = 1;
        Schlaf *s = TAB_NEU(db.schlaf);
        s->treffen = (int16_t)t; s->platz = (int16_t)platz; s->profil = (int8_t)p; s->nacht = nacht; s->geloescht = 0;
        return NULL;
    }
    case A_SCHLAF_WEG: {
        int s = schlaf_von(a->a, p, (tag_t)a->c);
        if (s >= 0) db.schlaf.v[s].geloescht = 1;
        return NULL;
    }
    case A_AUSLEIHE: {
        int t = a->a, u = a->b, anzahl = a->c;
        if (t < 0 || treffen_status(t) == T_WEG || db.treffen.v[t].beendet_um) return T_TREFFEN_VORBEI;
        if (!hat_komme(t, p)) return T_OHNE_ZUSAGE;
        if (ist_gastgeber(t, p) && anzahl > 0) return "Das sind doch deine eigenen Sachen.";
        int i = ausleihe_von(t, u, p), mein = i >= 0 ? db.ausleihen.v[i].anzahl : 0;
        int frei = db.utensilien.v[u].anzahl - (ausgeliehen(t, u) - mein);
        if (anzahl > frei) return T_AUSGELIEHEN;
        if (i < 0) { Ausleihe *x = TAB_NEU(db.ausleihen); x->treffen = (int16_t)t; x->utensil = (int16_t)u; x->profil = (int8_t)p; i = db.ausleihen.n - 1; }
        db.ausleihen.v[i].anzahl = (uint8_t)(anzahl < 0 ? 0 : anzahl);
        return NULL;
    }
    case A_NACHRICHT: {
        if (!a->text[0]) return "Leere Nachricht.";
        nachricht_neu(p, a->text, a->a, NACHRICHT_PERSON);
        return NULL;
    }
    case A_NACHRICHT_WEG: {
        if (a->a < 0 || a->a >= db.nachrichten.n) return NULL;
        if (db.nachrichten.v[a->a].profil != p) return T_FREMD;
        db.nachrichten.v[a->a].geloescht = 1;
        return NULL;
    }
    case A_ANHEFTEN: {
        if (a->a < 0 || a->a >= db.nachrichten.n) return NULL;
        Nachricht *n = &db.nachrichten.v[a->a];
        if (!n->angeheftet) {
            int k = 0;
            for (int i = 0; i < db.nachrichten.n; i++) if (!db.nachrichten.v[i].geloescht && db.nachrichten.v[i].angeheftet) k++;
            if (k >= 3) return T_ANGEHEFTET;
        }
        n->angeheftet = !n->angeheftet;
        return NULL;
    }
    case A_REAKTION: {
        int n = a->a, e = a->b;
        for (int i = 0; i < db.reaktionen.n; i++) {
            Reaktion *r = &db.reaktionen.v[i];
            if (r->nachricht == n && r->profil == p) {
                if (r->emoji == e) { r->nachricht = -1; return NULL; }   /* zurücknehmen */
                r->emoji = (uint8_t)e; return NULL;
            }
        }
        Reaktion *r = TAB_NEU(db.reaktionen);
        r->nachricht = (int16_t)n; r->profil = (int8_t)p; r->emoji = (uint8_t)e;
        return NULL;
    }
    case A_STIMME: {
        int u = a->a, k = a->b;
        if (u < 0 || u >= db.umfragen.n) return "Die Umfrage gibt es nicht mehr.";
        if (umfrage_zu(u)) return T_UMFRAGE_ZU;
        int s = stimme_von(u, p);
        if (s < 0) { Stimme *x = TAB_NEU(db.stimmen); x->umfrage = (int16_t)u; x->profil = (int8_t)p; x->wahl = 0; s = db.stimmen.n - 1; }
        uint8_t w = db.stimmen.v[s].wahl, bit = (uint8_t)(1 << k);
        if (w & bit) w &= (uint8_t)~bit;
        else w = db.umfragen.v[u].mehrfach ? (uint8_t)(w | bit) : bit;
        db.stimmen.v[s].wahl = w;
        return NULL;
    }
    case A_UMFRAGE_ENDE: {
        int u = a->a;
        if (u < 0 || u >= db.umfragen.n) return "Die Umfrage gibt es nicht mehr.";
        int n = db.umfragen.v[u].nachricht;
        if (db.nachrichten.v[n].profil != p) return tmp("Beenden darf nur %s. Wer fragt, der beendet.", pname(db.nachrichten.v[n].profil));
        if (umfrage_zu(u)) return T_UMFRAGE_ZU;
        db.umfragen.v[u].beendet_um = jetzt;
        return NULL;
    }
    case A_UMFRAGE_NEU: {
        /* text = Frage \n Antwort1 \n Antwort2 … */
        char *teile[UMFRAGE_MAX + 1]; int n = 0;
        char *s = text_kopie(a->text);
        teile[n++] = s;
        for (char *q = s; *q && n < UMFRAGE_MAX + 1; q++) if (*q == '\n') { *q = 0; teile[n++] = q + 1; }
        if (n < 3) return "Eine Umfrage braucht 2 bis 6 Antworten.";
        int m = nachricht_neu(p, teile[0], -1, NACHRICHT_UMFRAGE);
        Umfrage *u = TAB_NEU(db.umfragen);
        memset(u, 0, sizeof *u);
        u->nachricht = (int16_t)m; u->n = (uint8_t)(n - 1); u->mehrfach = a->b == 1;
        for (int i = 1; i < n; i++) u->antworten[i - 1] = teile[i];
        return NULL;
    }
    case A_AUSGABE: {
        int t = a->a;
        if (t < 0 || treffen_status(t) == T_WEG) return "Treffen nicht gefunden.";
        if (db.treffen.v[t].kasse_zu_um) return T_KASSE_ZU;
        if (!hat_komme(t, p)) return T_OHNE_ZUSAGE_KASSE;
        if (a->betrag <= 0) return "Wie viel hat es gekostet?";
        if (!a->maske) return "Für wen war es?";
        Ausgabe *x = TAB_NEU(db.ausgaben);
        memset(x, 0, sizeof *x);
        x->treffen = (int16_t)t; x->trip = -1; x->profil = (int8_t)(a->c >= 0 ? a->c : p); x->eingetragen_von = (int8_t)p;
        x->betrag = a->betrag; x->art = (uint8_t)a->b; x->fuer = a->maske; x->titel = text_kopie(a->text);
        return NULL;
    }
    case A_AUSGABE_WEG: {
        if (a->a < 0 || a->a >= db.ausgaben.n) return NULL;
        Ausgabe *x = &db.ausgaben.v[a->a];
        if (db.treffen.v[x->treffen].kasse_zu_um) return T_KASSE_ZU;
        if (!hat_komme(x->treffen, p)) return T_OHNE_ZUSAGE_KASSE;
        x->geloescht = 1;
        return NULL;
    }
    case A_KASSE_ZU: case A_KASSE_AUF: {
        int t = a->a;
        if (t < 0 || treffen_status(t) == T_WEG) return "Treffen nicht gefunden.";
        Treffen *x = &db.treffen.v[t];
        if (x->beendet_um) return "Das Treffen ist beendet. Die Kasse bleibt zu.";
        int darf = x->gastgeber >= 0 ? ist_gastgeber(t, p) : hat_komme(t, p);
        if (!darf) return tmp("Das darf nur %s. Wer das Klo hat, hat das Sagen.", x->gastgeber >= 0 ? pname(x->gastgeber) : "der Gastgeber");
        if (a->typ == A_KASSE_ZU) {
            if (x->kasse_zu_um) return NULL;
            DirektPaar d[64]; int n = direkt_paare(t, d, 64);
            for (int i = 0; i < n; i++) if (d[i].betrag > 0) {
                Zahlung *z = TAB_NEU(db.zahlungen);
                z->treffen = (int16_t)t; z->von = (int8_t)d[i].von; z->an = (int8_t)d[i].an; z->betrag = d[i].betrag; z->status = ZAHLUNG_OFFEN; z->gemeldet_um = z->bestaetigt_um = 0;
            }
            x->kasse_zu_um = jetzt;
        } else {
            x->kasse_zu_um = 0;
            for (int i = 0; i < db.zahlungen.n; i++) if (db.zahlungen.v[i].treffen == t) db.zahlungen.v[i].treffen = -1;
        }
        return NULL;
    }
    case A_ZAHLUNG: {
        if (a->a < 0 || a->a >= db.zahlungen.n) return NULL;
        Zahlung *z = &db.zahlungen.v[a->a];
        if (a->b == ZAHLUNG_GEMELDET) { if (z->von != p) return T_FREMD; z->status = ZAHLUNG_GEMELDET; z->gemeldet_um = jetzt; }
        else if (a->b == ZAHLUNG_BESTAETIGT) { if (z->an != p) return T_FREMD; z->status = ZAHLUNG_BESTAETIGT; z->bestaetigt_um = jetzt; }
        else { if (z->von != p && z->an != p) return T_FREMD; z->status = ZAHLUNG_OFFEN; }
        return NULL;
    }
    case A_DAUMEN: {
        int m = a->a;
        for (int i = 0; i < db.daumen.n; i++)
            if (db.daumen.v[i].medium == m && db.daumen.v[i].profil == p) { db.daumen.v[i].wert = (int8_t)a->b; return NULL; }
        Daumen *d = TAB_NEU(db.daumen);
        d->medium = (int16_t)m; d->profil = (int8_t)p; d->wert = (int8_t)a->b;
        return NULL;
    }
    case A_ERLEDIGT: {
        if (a->a < 0 || a->a >= db.medien.n) return NULL;
        db.medien.v[a->a].erledigt_um = a->b ? jetzt : 0;
        return NULL;
    }
    case A_IDEE: {
        if (!a->text[0]) return "Titel fehlt.";
        int pos = 0;
        for (int i = 0; i < db.medien.n; i++) if (db.medien.v[i].pos >= pos) pos = db.medien.v[i].pos + 1;
        Medium *m = TAB_NEU(db.medien);
        memset(m, 0, sizeof *m);
        m->art = (uint8_t)a->b; m->profil = (int8_t)p; m->gastgeber = (int8_t)(a->b == MED_IDEE ? (a->c >= 0 ? a->c : p) : -1);
        m->pos = (int16_t)pos; m->titel = text_kopie(a->text); m->adresse = "";
        return NULL;
    }
    case A_KOMMENTAR: {
        int t = a->a, k = 0;
        if (t < 0 || treffen_status(t) != T_KOMMEND) return T_KOMMENTAR_ZU;
        for (int i = 0; i < db.kommentare.n; i++) if (!db.kommentare.v[i].geloescht && db.kommentare.v[i].treffen == t && db.kommentare.v[i].profil == p) k++;
        if (k >= 3) return T_KOMMENTARE_VOLL;
        Kommentar *x = TAB_NEU(db.kommentare);
        memset(x, 0, sizeof *x);
        x->treffen = (int16_t)t; x->profil = (int8_t)p; x->zeit = jetzt; x->text = text_kopie(a->text);
        return NULL;
    }
    case A_KOMMENTAR_WEG: {
        if (a->a < 0 || a->a >= db.kommentare.n) return NULL;
        if (db.kommentare.v[a->a].profil != p) return T_FREMD;
        db.kommentare.v[a->a].geloescht = 1;
        return NULL;
    }
    case A_AGENDA: {
        int t = a->a, k = 0;
        if (t < 0 || treffen_status(t) == T_WEG || db.treffen.v[t].beendet_um) return T_TREFFEN_VORBEI;
        if (!hat_komme(t, p)) return T_AGENDA_NUR_KOMME;
        if (a->b < db.treffen.v[t].von || a->b > db.treffen.v[t].bis) return T_AGENDA_TAG;
        for (int i = 0; i < db.agenda.n; i++) if (!db.agenda.v[i].geloescht && db.agenda.v[i].treffen == t) k++;
        if (k >= 40) return T_AGENDA_VOLL;
        if (!a->text[0]) return "Was steht an?";
        Agenda *x = TAB_NEU(db.agenda);
        memset(x, 0, sizeof *x);
        x->treffen = (int16_t)t; x->profil = (int8_t)p; x->datum = (tag_t)a->b; x->uhrzeit = a->c; x->name = text_kopie(a->text); x->notiz = "";
        return NULL;
    }
    case A_AGENDA_WEG: {
        if (a->a < 0 || a->a >= db.agenda.n) return NULL;
        if (!hat_komme(db.agenda.v[a->a].treffen, p)) return T_AGENDA_NUR_KOMME;
        db.agenda.v[a->a].geloescht = 1;
        return NULL;
    }
    case A_REGEL: {
        int l = laufendes_treffen();
        int darf = l >= 0 && (db.treffen.v[l].gastgeber >= 0 ? ist_gastgeber(l, p) : hat_komme(l, p));
        if (!darf && strcmp(pname(p), "Nogger") != 0) return T_REGELN_NUR;
        if (a->a < 1 || a->a > BAD_MAX) return "Zwischen 1 und 60 Minuten.";
        regel_dauer_bad = a->a;
        return NULL;
    }
    case A_STATUS: {
        db.profile.v[p].status = text_kopie(a->text);
        return NULL;
    }
    case A_TREFFEN_BEENDEN: {
        int t = a->a;
        if (t < 0 || treffen_status(t) == T_WEG) return "Treffen nicht gefunden.";
        Treffen *x = &db.treffen.v[t];
        if (x->beendet_um) return T_TREFFEN_VORBEI;
        if (!hat_komme(t, p)) return T_NUR_KOMME;
        int ausgaben = 0;
        for (int i = 0; i < db.ausgaben.n; i++) if (!db.ausgaben.v[i].geloescht && db.ausgaben.v[i].treffen == t) ausgaben = 1;
        if (ausgaben && !x->kasse_zu_um) return T_KASSE_OFFEN_BEENDEN;
        if (treffen_status(t) == T_KOMMEND) return "Das Treffen hat noch nicht angefangen.";
        x->beendet_um = jetzt;
        uhren_aufraeumen(jetzt);
        /* „Frühere Treffen“: nächste Nr., Teilnehmer mit Heini-Namen */
        int nr = 0, tn[MAX_PROFILE], n = treffen_teilnehmer(t, tn);
        for (int i = 0; i < db.frueher.n; i++) if (db.frueher.v[i].nr > nr) nr = db.frueher.v[i].nr;
        Frueher *fr = TAB_NEU(db.frueher);
        memset(fr, 0, sizeof *fr);
        int j, m, d;
        tag_zu_datum(x->von, &j, &m, &d);
        fr->nr = (int16_t)(nr + 1); fr->tage = (int16_t)(x->bis - x->von + 1); fr->monat = j * 100 + m;
        fr->name = text_kopie(treffen_titel(t));
        int g[2], ng = treffen_gastgeber(t, g);
        fr->gastgeber = (int8_t)(ng > 0 ? heini_name_index(g[0]) : -1);
        fr->gastgeber2 = (int8_t)(ng > 1 ? heini_name_index(g[1]) : -1);
        for (int i = 0; i < n; i++) { int h = heini_name_index(tn[i]); if (h >= 0) fr->teilnehmer |= 1u << h; }
        system_nachricht(tmp(S_POKAL " Treffen Nr. %d ist beendet. Danke fürs Kommen, ihr Heinis!", nr + 1));
        return NULL;
    }
    }
    return ausfuehren2(a);
}

/* ---------- Journal im SRAM ----------
 * Kopf (16 Byte): "HEI64J02", Export-Kennung (u32), belegte Bytes (u32)
 * Eintrag: typ, textlänge, profil, 0, zeit (i32), a, b, c, d (i16), betrag (i32), maske (u32), Text */
#define J_KOPF 16
#define J_EINTRAG 24
#define J_ENDE (SPEICHER_GROESSE - 8)
static uint32_t j_belegt;
static int j_eintraege, j_da;
static zeit_t j_letzte;

static void w16(uint8_t *p, int v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void w32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static int16_t r16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }
static uint32_t r32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static void kopf_schreiben(void) {
    uint8_t k[J_KOPF];
    memcpy(k, "HEI64J02", 8);
    w32(k + 8, EXPORT_KENNUNG);
    w32(k + 12, j_belegt);
    plat_speicher_schreiben(k, 0, J_KOPF);
}
static int journal_schreiben(const Aktion *a) {
    if (!j_da) return 0;
    int tl = (int)strlen(a->text);
    if (tl > AKTION_TEXT_MAX) tl = AKTION_TEXT_MAX;
    uint32_t n = (J_EINTRAG + (uint32_t)tl + 1) & ~1u;   /* gerade Länge: PI-DMA braucht gerade SRAM-Adressen */
    if (J_KOPF + j_belegt + n > J_ENDE) return -1;
    uint8_t e[J_EINTRAG + AKTION_TEXT_MAX + 2];
    memset(e, 0, sizeof e);
    e[0] = a->typ; e[1] = (uint8_t)tl; e[2] = (uint8_t)a->profil; e[3] = 0;
    w32(e + 4, (uint32_t)a->zeit);
    w16(e + 8, a->a); w16(e + 10, a->b); w16(e + 12, a->c); w16(e + 14, a->d);
    w32(e + 16, (uint32_t)a->betrag); w32(e + 20, a->maske);
    memcpy(e + J_EINTRAG, a->text, (size_t)tl);
    plat_speicher_schreiben(e, J_KOPF + j_belegt, n);
    j_belegt += n;
    j_eintraege++;
    kopf_schreiben();
    return 0;
}

const char *tu(Aktion *a) {
    int alt_ich = ich;
    zeit_t alt_jetzt = jetzt;
    ich = a->profil;
    jetzt = a->zeit;
    uhren_aufraeumen(jetzt);
    const char *f = ausfuehren(a);
    if (!f && a->typ == A_DESIGN) { ich = alt_ich; if (!journal_wird_geladen) jetzt = alt_jetzt; }
    if (!f) {
        const char *v = verlauf_satz(a);
        if (v) { Verlauf *x = TAB_NEU(db.verlauf); x->profil = a->profil; x->typ = a->typ; x->zeit = a->zeit; x->text = text_kopie(v); }
        erfolge_pruefen();
        if (a->zeit > j_letzte) j_letzte = a->zeit;
        if (!journal_wird_geladen && journal_schreiben(a) < 0) f = "Der Speicher auf dem Modul ist voll. Die Änderung gilt nur bis zum Ausschalten.";
    }
    if (journal_wird_geladen) { ich = alt_ich; }
    else { ich = alt_ich; jetzt = alt_jetzt; }
    return f;
}

void journal_laden(void) {
    daten_laden();
    regel_dauer_bad = REGEL_DAUER_BAD;
    regel_verschleiss = REGEL_VERSCHLEISS;
    regel_verschleiss_ct = REGEL_VERSCHLEISS_CT;
    j_letzte = EXPORT_ZEIT;
    j_belegt = 0; j_eintraege = 0;
    j_da = plat_speicher_da();
    if (!j_da) return;
    uint8_t k[J_KOPF];
    plat_speicher_lesen(k, 0, J_KOPF);
    if (memcmp(k, "HEI64J02", 8) != 0 || r32(k + 8) != EXPORT_KENNUNG || r32(k + 12) > J_ENDE - J_KOPF) {
        /* neu oder anderer Datenstand: Journal beginnt leer */
        kopf_schreiben();
        return;
    }
    uint32_t belegt = r32(k + 12), pos = 0;
    journal_wird_geladen = 1;
    static uint8_t puffer[J_ENDE];
    plat_speicher_lesen(puffer, J_KOPF, belegt);
    zeit_t merk_jetzt = jetzt;
    while (pos + J_EINTRAG <= belegt) {
        const uint8_t *e = puffer + pos;
        Aktion a;
        memset(&a, 0, sizeof a);
        a.typ = e[0];
        int tl = e[1];
        a.profil = (int8_t)e[2];
        a.zeit = (zeit_t)r32(e + 4);
        a.a = r16(e + 8); a.b = r16(e + 10); a.c = r16(e + 12); a.d = r16(e + 14);
        a.betrag = (int32_t)r32(e + 16); a.maske = r32(e + 20);
        if (pos + J_EINTRAG + (uint32_t)tl > belegt || a.typ == 0 || a.typ >= A_ANZAHL) break;
        memcpy(a.text, e + J_EINTRAG, (size_t)tl);
        a.text[tl] = 0;
        tu(&a);
        if (a.typ == A_UHR && a.zeit > j_letzte) j_letzte = a.zeit;
        pos += (J_EINTRAG + (uint32_t)tl + 1) & ~1u;
        j_eintraege++;
    }
    j_belegt = pos;
    journal_wird_geladen = 0;
    jetzt = merk_jetzt;
}
int journal_frei(void) { return j_da ? (int)(J_ENDE - J_KOPF - j_belegt) : 0; }
int journal_eintraege(void) { return j_eintraege; }
int journal_da(void) { return j_da; }
zeit_t journal_letzte_zeit(void) { return j_letzte; }
void journal_loeschen(void) { j_belegt = 0; j_eintraege = 0; if (j_da) kopf_schreiben(); }
