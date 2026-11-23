/* logik.c – Fachlogik (aus public/logik.js und server.js der Heini-App 1.61.0) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "logik.h"
#include "text.h"
#include "gfx.h"

Datenbank db;
zeit_t jetzt;
int ich = -1;
int regel_dauer_bad = 15, regel_verschleiss = 1, regel_verschleiss_ct = 5;
int design_dunkel = 1;

void *tabelle_platz(void *v, int *cap, int n, size_t gross) {
    if (n <= *cap) return v;
    int neu = *cap ? *cap * 2 : 8;
    while (neu < n) neu *= 2;
    void *w = realloc(v, (size_t)neu * gross);
    memset((char *)w + (size_t)*cap * gross, 0, (size_t)(neu - *cap) * gross);
    *cap = neu;
    return w;
}

static inline tag_t heute(void) { return tag_von(jetzt); }

/* ---------- Profile ---------- */
const char *pname(int p) { return p >= 0 && p < db.profile.n ? db.profile.v[p].name : "Jemand"; }
uint16_t pfarbe(int p) { return p >= 0 && p < db.profile.n ? FARBEN[db.profile.v[p].farbe].farbe : TH->flaeche2; }
int phell(int p) { return p >= 0 && p < db.profile.n ? FARBEN[db.profile.v[p].farbe].hell : 0; }
int profil_aktiv(int p) { return p >= 0 && p < db.profile.n && db.profile.v[p].aktiv; }

static const Paar *bestaetigtes_paar(int p) {
    for (int i = 0; i < db.paare.n; i++) {
        const Paar *x = &db.paare.v[i];
        if (!x->weg && x->status == PAAR_BESTAETIGT && (x->von == p || x->zu == p)) return x;
    }
    return NULL;
}
int partner_von(int p) {
    const Paar *x = bestaetigtes_paar(p);
    return x ? (x->von == p ? x->zu : x->von) : -1;
}
int raum_besitzer(int p) {
    const Paar *x = bestaetigtes_paar(p);
    if (p < 0 || !x || x->wohnen_von < 0 || (x->wohnen_von != x->von && x->wohnen_von != x->zu)) return p;
    return x->wohnen_von;
}
int wohnt_mit(int p) {
    const Paar *x = bestaetigtes_paar(p);
    if (p < 0 || !x || x->wohnen_von < 0 || (x->wohnen_von != x->von && x->wohnen_von != x->zu)) return -1;
    return x->von == p ? x->zu : x->von;
}
int profil_nach_name(const char *name) {
    for (int i = 0; i < db.profile.n; i++) if (!strcmp(db.profile.v[i].name, name)) return i;
    return -1;
}
int heini_name_index(int p) {
    if (p < 0) return -1;
    for (int i = 0; i < HEINI_NAMEN_N; i++) if (!strcmp(HEINI_NAMEN[i], pname(p))) return i;
    return -1;
}

/* ---------- Treffen ---------- */
int treffen_status(int t) {
    if (t < 0 || t >= db.treffen.n || db.treffen.v[t].geloescht) return T_WEG;
    const Treffen *x = &db.treffen.v[t];
    if (x->beendet_um) return T_VORBEI;
    if (heute() > x->bis) return T_UEBERFAELLIG;
    return heute() >= x->von ? T_LAUFEND : T_KOMMEND;
}
int laufendes_treffen(void) {
    int best = -1;
    for (int i = 0; i < db.treffen.n; i++)
        if (treffen_status(i) == T_LAUFEND && (best < 0 || db.treffen.v[i].von < db.treffen.v[best].von)) best = i;
    return best;
}
int naechstes_treffen(void) {
    int best = -1;
    for (int i = 0; i < db.treffen.n; i++) {
        const Treffen *t = &db.treffen.v[i];
        if (t->geloescht || t->beendet_um || t->von < heute()) continue;
        if (best < 0 || t->von < db.treffen.v[best].von) best = i;
    }
    return best;
}
int kasse_treffen(void) {
    int l = laufendes_treffen();
    if (l >= 0) return l;
    int best = -1;
    for (int i = 0; i < db.treffen.n; i++)
        if (treffen_status(i) == T_UEBERFAELLIG && !db.treffen.v[i].kasse_zu_um && (best < 0 || db.treffen.v[i].von > db.treffen.v[best].von)) best = i;
    if (best >= 0) return best;
    for (int i = 0; i < db.treffen.n; i++)
        if (treffen_status(i) == T_KOMMEND && (best < 0 || db.treffen.v[i].von < db.treffen.v[best].von)) best = i;
    if (best >= 0) return best;
    for (int i = 0; i < db.treffen.n; i++)
        if (treffen_status(i) == T_VORBEI && !db.treffen.v[i].kasse_zu_um && (best < 0 || db.treffen.v[i].von > db.treffen.v[best].von)) best = i;
    return best;
}
int anwesenheit_von(int t, int p) {
    for (int i = 0; i < db.anwesenheit.n; i++)
        if (db.anwesenheit.v[i].treffen == t && db.anwesenheit.v[i].profil == p) return i;
    return -1;
}
int ist_anwesend(int t, int p) {
    int a = anwesenheit_von(t, p);
    return a >= 0 && !db.anwesenheit.v[a].abgereist;
}
int zusage_von(int t, int p) {
    for (int i = 0; i < db.zusagen.n; i++)
        if (db.zusagen.v[i].treffen == t && db.zusagen.v[i].profil == p) return db.zusagen.v[i].status;
    return -1;
}
int ist_gastgeber(int t, int p) {
    if (t < 0 || p < 0) return 0;
    const Treffen *x = &db.treffen.v[t];
    return x->gastgeber >= 0 && (x->gastgeber == p || x->gastgeber2 == p || wohnt_mit(x->gastgeber) == p);
}
int hat_komme(int t, int p) {
    if (t < 0 || p < 0) return 0;
    const Treffen *x = &db.treffen.v[t];
    return x->gastgeber == p || (x->gastgeber >= 0 && x->gastgeber2 == p) || zusage_von(t, p) == ZUSAGE_KOMME;
}
int treffen_gastgeber(int t, int out[2]) {
    const Treffen *x = &db.treffen.v[t];
    if (x->gastgeber < 0) return 0;
    int b = x->gastgeber2 >= 0 ? x->gastgeber2 : (!x->beendet_um ? wohnt_mit(x->gastgeber) : -1);
    if (b < 0 || b == x->gastgeber) { out[0] = x->gastgeber; return 1; }
    if (raum_besitzer(x->gastgeber) == b) { out[0] = b; out[1] = x->gastgeber; }
    else { out[0] = x->gastgeber; out[1] = b; }
    return 2;
}
int raum_im_treffen(int r, int t) {
    if (r < 0 || t < 0) return 0;
    const Raum *x = &db.raeume.v[r];
    const Treffen *y = &db.treffen.v[t];
    if (x->weg) return 0;
    if (y->gastgeber >= 0) return x->profil == raum_besitzer(y->gastgeber) && x->treffen < 0;
    return x->treffen == t;
}
int treffen_teilnehmer(int t, int *out) {
    uint32_t m = 0;
    const Treffen *x = &db.treffen.v[t];
    if (x->gastgeber >= 0) m |= 1u << x->gastgeber;
    if (x->gastgeber >= 0 && x->gastgeber2 >= 0) m |= 1u << x->gastgeber2;
    for (int i = 0; i < db.zusagen.n; i++)
        if (db.zusagen.v[i].treffen == t && db.zusagen.v[i].status == ZUSAGE_KOMME && db.zusagen.v[i].profil >= 0) m |= 1u << db.zusagen.v[i].profil;
    for (int i = 0; i < db.anwesenheit.n; i++)
        if (db.anwesenheit.v[i].treffen == t && db.anwesenheit.v[i].profil >= 0) m |= 1u << db.anwesenheit.v[i].profil;
    int n = 0;
    for (int p = 0; p < db.profile.n; p++) if (m & (1u << p)) out[n++] = p;
    return n;
}
const char *treffen_adresse(int t) {
    const Treffen *x = &db.treffen.v[t];
    if (x->ausserhalb || x->gastgeber < 0) return x->adresse ? x->adresse : "";
    const Profil *g = &db.profile.v[raum_besitzer(x->gastgeber)];
    if (g->strasse[0] || g->wohnort[0])
        return tmp("%s%s%s%s%s", g->strasse, g->strasse[0] ? ", " : "", g->plz, g->plz[0] ? " " : "", g->wohnort);
    return x->adresse ? x->adresse : "";
}
const char *treffen_titel(int t) {
    const Treffen *x = &db.treffen.v[t];
    if (x->titel && x->titel[0]) return x->titel;
    return x->gastgeber >= 0 ? tmp("Heini-Treffen bei %s", pname(x->gastgeber)) : "Heini-Treffen";
}
int treffen_naechte(int t) { const Treffen *x = &db.treffen.v[t]; return x->bis > x->von ? x->bis - x->von : 0; }
int nach_treffen(int p) {
    int best = -1, tn[MAX_PROFILE];
    for (int i = 0; i < db.treffen.n; i++) {
        const Treffen *t = &db.treffen.v[i];
        if (t->geloescht || !t->beendet_um || jetzt - t->beendet_um > NACH_TAGE * 86400) continue;
        int n = treffen_teilnehmer(i, tn), dabei = 0;
        for (int k = 0; k < n; k++) if (tn[k] == p) dabei = 1;
        if (dabei && (best < 0 || t->beendet_um > db.treffen.v[best].beendet_um)) best = i;
    }
    return best;
}

/* ---------- Bad ---------- */
int gehoert(int b, int p) {
    const Buchung *x = &db.buchungen.v[b];
    return p >= 0 && (x->profil == p || (x->partner == p && !x->partner_bis));
}
zeit_t buchung_ende(int b) {
    const Buchung *x = &db.buchungen.v[b];
    if (x->fertig) return x->fertig > x->start ? x->fertig : x->start;
    if (x->offen) { zeit_t e = jetzt; if (e > x->start + BAD_MAX * 60) e = x->start + BAD_MAX * 60; return e < x->start ? x->start : e; }
    return x->start + x->dauer * 60;
}
int uhr_laeuft(int b) {
    const Buchung *x = &db.buchungen.v[b];
    if (x->geloescht || x->fertig || x->start > jetzt) return 0;
    int st = treffen_status(x->treffen);
    if (x->treffen >= 0 && (st == T_VORBEI || st == T_WEG)) return 0;
    zeit_t ende = x->offen ? x->start + BAD_MAX * 60 : x->start + x->dauer * 60;
    return jetzt < ende;
}
int meine_uhr(int p) {
    for (int i = db.buchungen.n - 1; i >= 0; i--) if (gehoert(i, p) && uhr_laeuft(i)) return i;
    return -1;
}
int drin_in(int raum, int t) {
    for (int i = db.buchungen.n - 1; i >= 0; i--)
        if (db.buchungen.v[i].raum == raum && uhr_laeuft(i) && (t < 0 || db.buchungen.v[i].treffen == t)) return i;
    return -1;
}
int schlange(int raum, int t, int *out, int max) {
    int n = 0;
    for (int i = 0; i < db.warteliste.n && n < max; i++) {
        const Warte *w = &db.warteliste.v[i];
        if (w->geloescht || w->raum != raum || (t >= 0 && w->treffen != t)) continue;
        int k = n++;
        while (k > 0 && db.warteliste.v[out[k - 1]].seit > w->seit) { out[k] = out[k - 1]; k--; }
        out[k] = i;
    }
    return n;
}
int dran_in(int raum, int t) {
    if (drin_in(raum, t) >= 0) return -1;
    int s[32];
    return schlange(raum, t, s, 32) ? s[0] : -1;
}
int warte_platz(int p, int raum, int t) {
    for (int i = 0; i < db.warteliste.n; i++) {
        const Warte *w = &db.warteliste.v[i];
        if (!w->geloescht && w->raum == raum && w->treffen == t && (w->profil == p || w->partner == p)) return i;
    }
    return -1;
}
int wartet_in(int p, int t) {
    for (int i = 0; i < db.warteliste.n; i++) {
        const Warte *w = &db.warteliste.v[i];
        if (!w->geloescht && w->treffen == t && (w->profil == p || w->partner == p)) return w->raum;
    }
    return -1;
}
int baeder(int t, int *out, int max) {
    int n = 0;
    if (t < 0) return 0;
    for (int r = 0; r < db.raeume.n && n < max; r++) {
        if (db.raeume.v[r].art != RAUM_BAD || !raum_im_treffen(r, t)) continue;
        int k = n++;
        while (k > 0 && (db.raeume.v[out[k - 1]].reihenfolge > db.raeume.v[r].reihenfolge ||
               (db.raeume.v[out[k - 1]].reihenfolge == db.raeume.v[r].reihenfolge && strcmp(db.raeume.v[out[k - 1]].name, db.raeume.v[r].name) > 0))) { out[k] = out[k - 1]; k--; }
        out[k] = r;
    }
    return n;
}
int offener_alarm(int t) {
    for (int i = 0; i < db.alarme.n; i++) {
        const Alarm *a = &db.alarme.v[i];
        if (!a->geloescht && a->treffen == t && !a->erledigt_um) return i;
    }
    return -1;
}
void uhren_aufraeumen(zeit_t bis) {
    for (int i = 0; i < db.buchungen.n; i++) {
        Buchung *b = &db.buchungen.v[i];
        if (b->geloescht || b->fertig || !b->offen) continue;
        zeit_t grenze = b->start + BAD_MAX * 60;
        if (grenze <= bis) { b->fertig = grenze; b->dauer = BAD_MAX; if (b->partner >= 0 && !b->partner_bis) b->partner_bis = grenze; }
        else {
            const Treffen *t = b->treffen >= 0 ? &db.treffen.v[b->treffen] : NULL;
            if (t && t->beendet_um && t->beendet_um <= bis) {
                zeit_t e = t->beendet_um > b->start ? t->beendet_um : b->start;
                b->fertig = e; b->dauer = (int16_t)((e - b->start + 59) / 60);
            }
        }
    }
}

/* ---------- Schlafen, Ausleihen ---------- */
int liegen_auf(int t, int platz, tag_t nacht, int out[2]) {
    int n = 0;
    for (int i = 0; i < db.schlaf.n && n < 2; i++) {
        const Schlaf *s = &db.schlaf.v[i];
        if (!s->geloescht && s->treffen == t && s->platz == platz && s->nacht == nacht) out[n++] = s->profil;
    }
    return n;
}
int schlaf_von(int t, int p, tag_t nacht) {
    for (int i = 0; i < db.schlaf.n; i++) {
        const Schlaf *s = &db.schlaf.v[i];
        if (!s->geloescht && s->treffen == t && s->profil == p && s->nacht == nacht) return i;
    }
    return -1;
}
int ausgeliehen(int t, int u) {
    int n = 0;
    for (int i = 0; i < db.ausleihen.n; i++)
        if (db.ausleihen.v[i].treffen == t && db.ausleihen.v[i].utensil == u) n += db.ausleihen.v[i].anzahl;
    return n;
}
int ausleihe_von(int t, int u, int p) {
    for (int i = 0; i < db.ausleihen.n; i++)
        if (db.ausleihen.v[i].treffen == t && db.ausleihen.v[i].utensil == u && db.ausleihen.v[i].profil == p && db.ausleihen.v[i].anzahl) return i;
    return -1;
}

/* ---------- Kasse ---------- */
static int art_index(const char *k) {
    for (int i = 0; i < AUSGABE_ARTEN_N; i++) if (!strcmp(AUSGABE_ARTEN[i].schluessel, k)) return i;
    return -1;
}
/* Gleich verteilen, Rest-Cent der Reihe nach an die ersten */
static int anteile(int32_t betrag, const int8_t *ids, int n, int8_t *oid, int32_t *ocent) {
    if (!n) return 0;
    int32_t je = betrag / n, rest = betrag - je * n;
    for (int i = 0; i < n; i++) { oid[i] = ids[i]; ocent[i] = je + (i < rest ? 1 : 0); }
    return n;
}
int ausgabe_teile(int a, int8_t *ids, int32_t *cent, int max) {
    const Ausgabe *x = &db.ausgaben.v[a];
    if (x->geloescht) return 0;
    if (x->art == art_index("essen")) {
        int8_t pid[64]; int32_t pc[64]; int np = 0;
        for (int i = 0; i < db.posten.n && np < 64; i++)
            if (db.posten.v[i].ausgabe == a && db.posten.v[i].cent > 0) { pid[np] = db.posten.v[i].profil; pc[np] = db.posten.v[i].cent; np++; }
        if (np) {
            int8_t lid[64]; int32_t lc[64];
            int nl = anteile(x->liefer > 0 ? x->liefer : 0, pid, np, lid, lc);
            int n = 0;
            for (int i = 0; i < np && n < max; i++) { ids[n] = pid[i]; cent[n] = pc[i] + (i < nl ? lc[i] : 0); n++; }
            return n;
        }
    }
    int8_t fuer[MAX_PROFILE]; int nf = 0, nr = 0;
    int8_t rest[MAX_PROFILE];
    if (x->fuer_n) { for (int i = 0; i < x->fuer_n && i < MAX_PROFILE; i++) fuer[nf++] = x->fuer_reihe[i]; }
    else for (int p = 0; p < db.profile.n; p++) if (x->fuer & (1u << p)) fuer[nf++] = (int8_t)p;
    int alk = x->alkohol || x->art == art_index("alkohol");
    if (alk) {
        for (int i = 0; i < nf; i++) if (!db.profile.v[fuer[i]].kein_alkohol) rest[nr++] = fuer[i];
        if (nr) return anteile(x->betrag, rest, nr, ids, cent);
    }
    return anteile(x->betrag, fuer, nf, ids, cent);
}
int32_t kassen_stand(int t, int32_t stand[MAX_PROFILE], uint32_t *beteiligt) {
    int32_t summe = 0;
    memset(stand, 0, sizeof(int32_t) * MAX_PROFILE);
    if (beteiligt) *beteiligt = 0;
    for (int a = 0; a < db.ausgaben.n; a++) {
        const Ausgabe *x = &db.ausgaben.v[a];
        if (x->geloescht || x->treffen != t) continue;
        int8_t ids[64]; int32_t c[64];
        int n = ausgabe_teile(a, ids, c, 64);
        if (!n || x->betrag <= 0) continue;
        int32_t betrag = 0;
        for (int i = 0; i < n; i++) betrag += c[i];
        summe += betrag;
        stand[x->profil] += betrag;
        if (beteiligt) *beteiligt |= 1u << x->profil;
        for (int i = 0; i < n; i++) { stand[ids[i]] -= c[i]; if (beteiligt) *beteiligt |= 1u << ids[i]; }
    }
    return summe;
}
int32_t kassen_konto(int t, int p, int32_t *bezahlt, int32_t *anteil) {
    *bezahlt = 0; *anteil = 0;
    for (int a = 0; a < db.ausgaben.n; a++) {
        const Ausgabe *x = &db.ausgaben.v[a];
        if (x->geloescht || x->treffen != t) continue;
        int8_t ids[64]; int32_t c[64];
        int n = ausgabe_teile(a, ids, c, 64);
        if (!n || x->betrag <= 0) continue;
        int32_t betrag = 0, mein = 0;
        for (int i = 0; i < n; i++) { betrag += c[i]; if (ids[i] == p) mein += c[i]; }
        if (x->profil == p) *bezahlt += betrag;
        *anteil += mein;
    }
    return *bezahlt - *anteil;
}
int direkt_paare(int t, DirektPaar *out, int max) {
    static int32_t schuld[MAX_PROFILE][MAX_PROFILE];   /* schuld[x][y] = x schuldet y */
    static uint8_t paar[MAX_PROFILE][MAX_PROFILE];
    memset(schuld, 0, sizeof schuld); memset(paar, 0, sizeof paar);
    for (int a = 0; a < db.ausgaben.n; a++) {
        const Ausgabe *x = &db.ausgaben.v[a];
        if (x->geloescht || x->treffen != t) continue;
        int8_t ids[64]; int32_t c[64];
        int n = ausgabe_teile(a, ids, c, 64);
        if (!n || x->betrag <= 0) continue;
        int32_t je[MAX_PROFILE] = {0};
        for (int i = 0; i < n; i++) je[ids[i]] += c[i];
        for (int p = 0; p < MAX_PROFILE; p++) {
            if (p == x->profil || !je[p]) continue;
            schuld[p][x->profil] += je[p];
            paar[p < x->profil ? p : x->profil][p < x->profil ? x->profil : p] = 1;
        }
    }
    int n = 0;
    for (int a = 0; a < MAX_PROFILE; a++) for (int b = a + 1; b < MAX_PROFILE; b++) {
        if (!paar[a][b] || n >= max) continue;
        int von = schuld[a][b] >= schuld[b][a] ? a : b, an = von == a ? b : a;
        DirektPaar d = { von, an, schuld[von][an] - schuld[an][von] };
        int k = n++;
        while (k > 0 && out[k - 1].betrag < d.betrag) { out[k] = out[k - 1]; k--; }
        out[k] = d;
    }
    return n;
}
int32_t fahrtkosten(int km, int hin_zurueck, int verbrauch, int spritpreis, int verschleiss) {
    if (km <= 0 || verbrauch <= 0 || spritpreis <= 0) return 0;
    int64_t strecke = (int64_t)km * (hin_zurueck ? 2 : 1);
    int64_t z = strecke * verbrauch * spritpreis + strecke * (verschleiss > 0 ? verschleiss : 0) * 1000;
    return (int32_t)((z + 5000) / 10000);
}
int fahrt_insassen(int f, int *out, int max) {
    int n = 0;
    out[n++] = db.fahrten.v[f].profil;
    for (int i = 0; i < db.mitfahrer.n && n < max; i++)
        if (!db.mitfahrer.v[i].geloescht && db.mitfahrer.v[i].fahrt == f && db.mitfahrer.v[i].profil != db.fahrten.v[f].profil)
            out[n++] = db.mitfahrer.v[i].profil;
    return n;
}

/* ---------- Chat ---------- */
int umfrage_von_nachricht(int n) {
    for (int i = 0; i < db.umfragen.n; i++) if (db.umfragen.v[i].nachricht == n) return i;
    return -1;
}
int umfrage_zu(int u) {
    const Umfrage *x = &db.umfragen.v[u];
    return x->beendet_um || (x->endet_um && jetzt >= x->endet_um);
}
int stimme_von(int u, int p) {
    for (int i = 0; i < db.stimmen.n; i++) if (db.stimmen.v[i].umfrage == u && db.stimmen.v[i].profil == p) return i;
    return -1;
}
int umfrage_zaehlen(int u, int anzahl[UMFRAGE_MAX], int *leute) {
    memset(anzahl, 0, sizeof(int) * UMFRAGE_MAX);
    *leute = 0;
    int max = 0;
    for (int i = 0; i < db.stimmen.n; i++) {
        const Stimme *s = &db.stimmen.v[i];
        if (s->umfrage != u || !s->wahl) continue;
        int w = 0;
        for (int k = 0; k < db.umfragen.v[u].n; k++) if (s->wahl & (1 << k)) { anzahl[k]++; w = 1; }
        *leute += w;
    }
    for (int k = 0; k < db.umfragen.v[u].n; k++) if (anzahl[k] > max) max = anzahl[k];
    return max;
}
int reaktionen_zaehlen(int n, int *e) {
    int alle = 0;
    memset(e, 0, sizeof(int) * EMOJIS_N);
    for (int i = 0; i < db.reaktionen.n; i++)
        if (db.reaktionen.v[i].nachricht == n && db.reaktionen.v[i].emoji < EMOJIS_N) { e[db.reaktionen.v[i].emoji]++; alle++; }
    return alle;
}

/* ---------- Sprüche ---------- */
const char *weisheit_des_tages(tag_t t) {
    int tage = t - datum_zu_tag(2026, 1, 1), n = WEISHEITEN_N;
    return WEISHEITEN[((tage % n) + n) % n];
}
const char *spruch_ueberzogen(int b) {
    uint32_t h = (uint32_t)b * 2654435761u + (uint32_t)db.buchungen.v[b].start;
    return SPRUECHE_UEBERZOGEN[h % SPRUECHE_UEBERZOGEN_N];
}

/* ---------- Ruhmeshalle ---------- */
typedef struct { int b; int profil; zeit_t start, fertig; int dauer; int raum, treffen; } Besuch;

static int besuche(Besuch *out, int max, int treffen) {
    int n = 0;
    for (int i = 0; i < db.buchungen.n && n < max; i++) {
        const Buchung *x = &db.buchungen.v[i];
        if (x->geloescht || x->start > jetzt || (treffen >= 0 && x->treffen != treffen)) continue;
        if (treffen >= 0 || x->treffen >= 0 || 1) {
            out[n++] = (Besuch){ i, x->profil, x->start, x->fertig, x->dauer, x->raum, x->treffen };
            if (x->partner >= 0 && n < max) {
                zeit_t s = x->partner_seit && x->partner_seit > x->start ? x->partner_seit : x->start;
                zeit_t e = 0;
                if (x->partner_bis) e = x->partner_bis;
                if (x->fertig && (!e || x->fertig < e)) e = x->fertig;
                if (e && e < s) e = s;
                out[n++] = (Besuch){ i, x->partner, s, e, x->dauer, x->raum, x->treffen };
            }
        }
    }
    return n;
}
static int ist_dauer(const Besuch *b) { return b->fertig ? (int)((b->fertig - b->start + 30) / 60) : -1; }
static int ueberzogen(const Besuch *b) {
    if (!b->fertig) return -100000;
    zeit_t plan = (b->start / 60) * 60 + b->dauer * 60;
    zeit_t d = b->fertig - plan;
    return (int)(d >= 0 ? (d + 30) / 60 : -((-d + 30) / 60));
}
static const char *uhrzeit_hm(zeit_t z) { int m = minute_von(z); return tmp("%02d:%02d", m / 60, m % 60); }

static Besuch besuch_puffer[1024];
int ruhm_treffen(void) {
    int l = laufendes_treffen();
    if (l >= 0) return l;
    int best = -1;
    for (int i = 0; i < db.treffen.n; i++)
        if (treffen_status(i) == T_VORBEI && (best < 0 || db.treffen.v[i].beendet_um > db.treffen.v[best].beendet_um)) best = i;
    return best;
}
int rekorde(int t, Rekord *out, int max) {
    int n = 0, nb = besuche(besuch_puffer, 1024, t);
    Besuch *bs = besuch_puffer;
    #define ADD(TI, ER, SYM, P, W, ...) do { if ((P) >= 0 && n < max) { Rekord *r = &out[n++]; r->titel = TI; r->erklaerung = ER; r->emoji = SYM; r->profil = P; r->wert = W; snprintf(r->text, sizeof r->text, __VA_ARGS__); } } while (0)
    /* Bad-König: meiste Besuche, bei Gleichstand wer die Zahl zuerst erreichte (Besuche sind nach Zeit sortiert genug) */
    {
        int cnt[MAX_PROFILE] = {0}; zeit_t wann[MAX_PROFILE] = {0};
        /* nach Start sortiert durchgehen */
        static int ord[1024];
        for (int i = 0; i < nb; i++) { int k = i; while (k > 0 && bs[ord[k - 1]].start > bs[i].start) { ord[k] = ord[k - 1]; k--; } ord[k] = i; }
        for (int i = 0; i < nb; i++) { int p = bs[ord[i]].profil; if (p < 0) continue; cnt[p]++; wann[p] = bs[ord[i]].start; }
        int best = -1;
        for (int p = 0; p < MAX_PROFILE; p++) if (cnt[p] && (best < 0 || cnt[p] > cnt[best] || (cnt[p] == cnt[best] && wann[p] < wann[best]))) best = p;
        if (best >= 0) ADD("Bad-König", "Die meisten Bad-Besuche.", "👑", best, cnt[best], cnt[best] == 1 ? "1 Bad-Besuch" : "%d Bad-Besuche", (int)cnt[best]);
    }
    int bi = -1, bw = 0;
    for (int i = 0; i < nb; i++) { int m = minute_von(bs[i].start); if (m < 240) continue; if (bi < 0 || m < bw || (m == bw && bs[i].start < bs[bi].start)) { bi = i; bw = m; } }
    if (bi >= 0) ADD("Frühaufsteher", "Der früheste Bad-Besuch ab 4:00 Uhr.", "🌅", bs[bi].profil, bw, "Bad um %s", uhrzeit_hm(bs[bi].start));
    bi = -1;
    for (int i = 0; i < nb; i++) { int m = minute_von(bs[i].start); if (m < 240) m += 1440; if (bi < 0 || m > bw || (m == bw && bs[i].start < bs[bi].start)) { bi = i; bw = m; } }
    if (bi >= 0) ADD("Nachteule", "Der späteste Bad-Besuch, nach Mitternacht bis 4:00 zählt als besonders spät.", "🦉", bs[bi].profil, bw, "Bad um %s", uhrzeit_hm(bs[bi].start));
    bi = -1;
    for (int i = 0; i < nb; i++) { int d = ist_dauer(&bs[i]); if (d < 0) continue; if (bi < 0 || d < bw || (d == bw && bs[i].start < bs[bi].start)) { bi = i; bw = d; } }
    if (bi >= 0) ADD("Schnellster Besuch", "Der kürzeste gemessene Bad-Besuch von „Rein“ bis „Bad jetzt freigeben“.", "⚡", bs[bi].profil, bw, bw == 1 ? "1 Minute" : "%d Minuten", (int)bw);
    bi = -1;
    for (int i = 0; i < nb; i++) { int d = ist_dauer(&bs[i]); if (d < 0) continue; if (bi < 0 || d > bw || (d == bw && bs[i].start < bs[bi].start)) { bi = i; bw = d; } }
    if (bi >= 0) ADD("Längste Sitzung", "Der längste gemessene Bad-Besuch.", "🐌", bs[bi].profil, bw, "%d Minuten im Bad", (int)bw);
    bi = -1;
    for (int i = 0; i < nb; i++) { int u = ueberzogen(&bs[i]); if (u <= 0) continue; if (bi < 0 || u > bw || (u == bw && bs[i].start < bs[bi].start)) { bi = i; bw = u; } }
    if (bi >= 0) ADD("Größter Überzieher", "Am weitesten über das geplante Ende hinaus im Bad.", "⏰", bs[bi].profil, bw, bw == 1 ? "1 Minute drüber" : "%d Minuten drüber", (int)bw);
    {
        int cnt[MAX_PROFILE] = {0}; zeit_t wann[MAX_PROFILE] = {0};
        for (int i = 0; i < db.alarme.n; i++) {
            const Alarm *a = &db.alarme.v[i];
            if (a->geloescht || a->erledigt_von < 0 || (t >= 0 && a->treffen != t)) continue;
            cnt[a->erledigt_von]++; if (a->erledigt_um > wann[a->erledigt_von]) wann[a->erledigt_von] = a->erledigt_um;
        }
        int best = -1;
        for (int p = 0; p < MAX_PROFILE; p++) if (cnt[p] && (best < 0 || cnt[p] > cnt[best] || (cnt[p] == cnt[best] && wann[p] < wann[best]))) best = p;
        if (best >= 0) ADD("Klopapier-Retter", "Die meisten gebrachten Rollen bei Klopapier-Alarmen.", "🧻", best, cnt[best], cnt[best] == 1 ? "1 Rolle gebracht" : "%d Rollen gebracht", (int)cnt[best]);
    }
    {
        int cnt[MAX_PROFILE] = {0}; zeit_t wann[MAX_PROFILE] = {0};
        zeit_t von = 0, bis = 0x7fffffff;
        if (t >= 0) { const Treffen *x = &db.treffen.v[t]; von = (zeit_t)x->von * 86400; bis = x->beendet_um ? x->beendet_um : (zeit_t)(x->bis + 1) * 86400; }
        for (int i = 0; i < db.nachrichten.n; i++) {
            const Nachricht *m = &db.nachrichten.v[i];
            if (m->geloescht || m->art != NACHRICHT_PERSON || m->profil < 0 || m->zeit < von || m->zeit >= bis) continue;
            cnt[m->profil]++; wann[m->profil] = m->zeit;
        }
        int best = -1;
        for (int p = 0; p < MAX_PROFILE; p++) if (cnt[p] && (best < 0 || cnt[p] > cnt[best] || (cnt[p] == cnt[best] && wann[p] < wann[best]))) best = p;
        if (best >= 0) ADD("Chat-Kanone", "Die meisten Nachrichten im Chat.", "💬", best, cnt[best], cnt[best] == 1 ? "1 Nachricht" : "%d Nachrichten", (int)cnt[best]);
    }
    if (t < 0) {
        int cnt[MAX_PROFILE] = {0}; zeit_t wann[MAX_PROFILE] = {0};
        for (int i = 0; i < db.anwesenheit.n; i++) {
            const Anwesenheit *a = &db.anwesenheit.v[i];
            if (a->profil < 0 || a->treffen < 0 || db.treffen.v[a->treffen].geloescht) continue;
            cnt[a->profil]++; if (a->angekommen > wann[a->profil]) wann[a->profil] = a->angekommen;
        }
        int best = -1;
        for (int p = 0; p < MAX_PROFILE; p++) if (cnt[p] && (best < 0 || cnt[p] > cnt[best] || (cnt[p] == cnt[best] && wann[p] < wann[best]))) best = p;
        if (best >= 0) ADD("Meiste Treffen", "Bei den meisten Treffen in der App angekommen.", "🎟", best, cnt[best], cnt[best] == 1 ? "1 Treffen" : "%d Treffen", (int)cnt[best]);
    }
    #undef ADD
    return n;
}

int erfolg_index(const char *code) {
    for (int i = 0; i < ERFOLGE_N; i++) if (!strcmp(ERFOLGE[i].code, code)) return i;
    return -1;
}
int erfolg_hat(int p, int code) {
    for (int i = 0; i < db.erfolge.n; i++) if (db.erfolge.v[i].profil == p && db.erfolge.v[i].code == code) return 1;
    return 0;
}
static int erfolg_da(int p, int code, int t) {
    for (int i = 0; i < db.erfolge.n; i++)
        if (db.erfolge.v[i].profil == p && db.erfolge.v[i].code == code && db.erfolge.v[i].treffen == t) return 1;
    return 0;
}
extern void system_nachricht(const char *text);
static void gib(int p, const char *code, int t) {
    int c = erfolg_index(code);
    if (p < 0 || c < 0) return;
    if (ERFOLGE[c].art == 'U') t = -1;
    if (erfolg_da(p, c, t)) return;
    Erfolg *e = TAB_NEU(db.erfolge);
    e->profil = (int8_t)p; e->code = (int16_t)c; e->treffen = (int16_t)t; e->erreicht = jetzt; e->neu = 1;
    system_nachricht(tmp(S_POKAL " %s hat „%s“ freigeschaltet!", pname(p), ERFOLGE[c].name));
}
/* Trophäen nach der Rechnung von achievementsBerechnen – gezählt wird nur, was nach dem Export passiert ist
 * (wie der Stichtag meta.ruhm_ab in der Web-App) */
void erfolge_pruefen(void) {
    for (int t = 0; t < db.treffen.n; t++) {
        int st = treffen_status(t);
        if (st == T_KOMMEND || st == T_WEG) continue;
        const Treffen *tr = &db.treffen.v[t];
        /* Erster! – nur für Gäste, nur wenn der wirklich Erste nach dem Export kam */
        int erste = -1;
        for (int i = 0; i < db.anwesenheit.n; i++) {
            const Anwesenheit *a = &db.anwesenheit.v[i];
            if (a->treffen != t || a->profil == tr->gastgeber) continue;
            if (erste < 0 || a->angekommen < db.anwesenheit.v[erste].angekommen) erste = i;
        }
        if (erste >= 0 && db.anwesenheit.v[erste].angekommen >= EXPORT_ZEIT) {
            int c = erfolg_index("erster"), schon = 0;
            for (int i = 0; i < db.erfolge.n; i++) if (db.erfolge.v[i].code == c && db.erfolge.v[i].treffen == t) schon = 1;
            if (!schon) gib(db.anwesenheit.v[erste].profil, "erster", t);
        }
        int nb = besuche(besuch_puffer, 1024, t);
        int rein[MAX_PROFILE] = {0};
        for (int i = 0; i < nb; i++) {
            const Besuch *b = &besuch_puffer[i];
            if (b->start < EXPORT_ZEIT || b->profil < 0) continue;
            int m = minute_von(b->start), d = ist_dauer(b);
            if (m >= 240 && m < 390) gib(b->profil, "fruehaufsteher", t);
            if (m < 240) gib(b->profil, "nachteule", t);
            if (m == 180) gib(b->profil, "geisterstunde", t);
            if (b->fertig && b->fertig - b->start < 300) gib(b->profil, "sprinter", t);
            if (d >= 30) gib(b->profil, "marathon", t);
            if (db.buchungen.v[b->b].profil == b->profil) rein[b->profil]++;
        }
        for (int p = 0; p < MAX_PROFILE; p++) if (rein[p] >= 5) gib(p, "spontan", t);
        for (int i = 0; i < db.alarme.n; i++) {
            const Alarm *a = &db.alarme.v[i];
            if (a->geloescht || a->treffen != t) continue;
            if (a->ausgeloest >= EXPORT_ZEIT) gib(a->profil, "ausloeser", t);
            if (a->erledigt_von >= 0 && a->erledigt_um >= EXPORT_ZEIT) {
                gib(a->erledigt_von, "retter", t);
                if (a->erledigt_von == a->profil) gib(a->profil, "selbstheld", t);
            }
        }
        int warte[MAX_PROFILE] = {0};
        for (int i = 0; i < db.warteliste.n; i++) {
            const Warte *w = &db.warteliste.v[i];
            if (w->treffen == t && w->seit >= EXPORT_ZEIT && w->profil >= 0) warte[w->profil]++;
        }
        for (int p = 0; p < MAX_PROFILE; p++) if (warte[p] >= 3) gib(p, "wartelistler", t);
        int chat[MAX_PROFILE] = {0};
        zeit_t von = (zeit_t)tr->von * 86400, bis = tr->beendet_um ? tr->beendet_um : (zeit_t)(tr->bis + 1) * 86400;
        for (int i = 0; i < db.nachrichten.n; i++) {
            const Nachricht *m = &db.nachrichten.v[i];
            if (m->geloescht || m->art != NACHRICHT_PERSON || m->profil < 0 || m->zeit < von || m->zeit >= bis) continue;
            chat[m->profil]++;
        }
        for (int p = 0; p < MAX_PROFILE; p++) if (chat[p] >= 50) gib(p, "chatkanone", t);
        if (st == T_VORBEI && tr->beendet_um >= EXPORT_ZEIT) {
            if (tr->gastgeber >= 0) gib(tr->gastgeber, "gastgeberheld", t);
            if (tr->gastgeber >= 0 && tr->gastgeber2 >= 0) gib(tr->gastgeber2, "gastgeberheld", t);
        }
    }
    /* treffenübergreifend */
    int anz[MAX_PROFILE] = {0};
    uint32_t gast[MAX_PROFILE] = {0};
    for (int i = 0; i < db.anwesenheit.n; i++) {
        const Anwesenheit *a = &db.anwesenheit.v[i];
        if (a->profil < 0 || a->treffen < 0 || treffen_status(a->treffen) == T_WEG) continue;
        anz[a->profil]++;
        const Treffen *tr = &db.treffen.v[a->treffen];
        if (tr->gastgeber >= 0) gast[a->profil] |= 1u << tr->gastgeber;
        if (tr->gastgeber >= 0 && tr->gastgeber2 >= 0) gast[a->profil] |= 1u << tr->gastgeber2;
    }
    for (int p = 0; p < db.profile.n; p++) {
        int g = 0;
        for (int k = 0; k < 32; k++) if (gast[p] & (1u << k)) g++;
        if (anz[p] >= 5) gib(p, "stammgast", -1);
        if (anz[p] >= 10) gib(p, "treueseele", -1);
        if (g >= 3) gib(p, "weltenbummler", -1);
    }
}

/* ---------- Frühere Treffen ---------- */
int frueher_statistik(HeiniZahl *pers, int *np, HeiniZahl *gast, int *ng, int *tage_gesamt) {
    int n = 0;
    *np = 0; *ng = 0; *tage_gesamt = 0;
    for (int h = 0; h < HEINI_NAMEN_N; h++) { pers[h] = (HeiniZahl){ h, 0, 0 }; gast[h] = (HeiniZahl){ h, 0, 0 }; }
    for (int i = 0; i < db.frueher.n; i++) {
        const Frueher *f = &db.frueher.v[i];
        n++;
        *tage_gesamt += f->tage;
        for (int h = 0; h < HEINI_NAMEN_N; h++) if (f->teilnehmer & (1u << h)) { pers[h].n++; pers[h].tage += f->tage; }
        if (f->gastgeber >= 0) gast[f->gastgeber].n++;
        if (f->gastgeber2 >= 0) gast[f->gastgeber2].n++;
    }
    /* sortieren: meiste zuerst, sonst Name */
    for (int pass = 0; pass < 2; pass++) {
        HeiniZahl *l = pass ? gast : pers;
        for (int i = 1; i < HEINI_NAMEN_N; i++) {
            HeiniZahl x = l[i]; int k = i;
            while (k > 0 && (l[k - 1].n < x.n || (l[k - 1].n == x.n && strcmp(HEINI_NAMEN[l[k - 1].name], HEINI_NAMEN[x.name]) > 0))) { l[k] = l[k - 1]; k--; }
            l[k] = x;
        }
    }
    for (int h = 0; h < HEINI_NAMEN_N; h++) { if (pers[h].n) (*np)++; if (gast[h].n) (*ng)++; }
    return n;
}
