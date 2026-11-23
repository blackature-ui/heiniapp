/* seiten2.c – Chat, Ruhmeshalle, Profil (ansehen und bearbeiten), Bild- und Farbwahl, Räume und Gaming verwalten,
 * Treffen anlegen/bearbeiten, Wer ist da?, Frühere Treffen, Einstellungen, Änderungsverlauf, Tour */
#include "seiten_intern.h"

/* ======================================================================= Chat */
static int chat_merk = -1, chat_code[8], antwort_auf = -1;
void seite_chat(void) {
    seiten_kopf("Chat", -1);
    int chat_auswahl = -1;
    for (int i = 0; i < db.nachrichten.n; i++) {
        const Nachricht *m = &db.nachrichten.v[i];
        if (!m->geloescht && m->angeheftet) ui_symbolzeile(IC_PIN, tmp("%s: %s", pname(m->profil), m->text && m->text[0] ? m->text : m->medium == MEDIUM_AUDIO ? "Sprachnachricht" : m->medium == MEDIUM_GIF ? "GIF" : m->medien_anzahl > 1 ? tmp("%d Fotos", m->medien_anzahl) : "Foto"), TH->accentText);
    }
    tag_t letzter = -1;
    /* ganzer Verlauf: zuerst die letzten 60, darüber „Ältere laden“ in Schritten von 60 bis zur ersten Nachricht */
    static int chat_zeigen = 60;
    int ab = db.nachrichten.n > chat_zeigen ? db.nachrichten.n - chat_zeigen : 0;
    if (ab > 0) {
        if (ui_knopf_breit(tmp("Ältere Nachrichten laden (%d)", ab), KN_RAND, IC_CHEVRON_UP)) { chat_zeigen += 60; ui_roll_auf(0); ui_nochmal(); }
    } else if (db.nachrichten.n) ui_text(tmp("Anfang des Chats · %d Nachrichten", db.nachrichten.n), F_KLEIN, TH->muted);
    for (int i = ab; i < db.nachrichten.n; i++) {
        const Nachricht *m = &db.nachrichten.v[i];
        if (m->geloescht) continue;
        tag_t t = tag_von(m->zeit);
        if (t != letzter) {
            letzter = t;
            const char *tt = t == tag_von(jetzt) ? "Heute" : t == tag_von(jetzt) - 1 ? "Gestern" : datum_text(t);
            int b = text_breite(tt, F_FETT) + 24;
            gfx_runde_box(BILD_B / 2 - b / 2, ui_y() + 2, b, 24, 12, TH->flaeche2);
            text_mitte(BILD_B / 2, ui_y() + 5, tt, F_FETT, TH->muted);
            ui_abstand(34);
        }
        if (m->art == NACHRICHT_SYSTEM) { if (ui_systemzeile(m->text, uhr_text(m->zeit))) chat_auswahl = i; continue; }
        char extra[160] = "";
        if (m->medien_anzahl) snprintf(extra, sizeof extra, "%s %d %s (nur in der App)", m->medium == MEDIUM_AUDIO ? "🎤" : "📷", m->medien_anzahl,
                                       m->medium == MEDIUM_AUDIO ? "Sprachnachricht" : m->medium == MEDIUM_GIF ? "GIF" : m->medien_anzahl == 1 ? "Foto" : "Fotos");
        const char *antwort = m->antwort_auf >= 0 && m->antwort_auf < db.nachrichten.n ? tmp("%s: %s", pname(db.nachrichten.v[m->antwort_auf].profil), db.nachrichten.v[m->antwort_auf].text) : NULL;
        int u = m->art == NACHRICHT_UMFRAGE ? umfrage_von_nachricht(i) : -1;
        if (u >= 0) {
            /* Umfrage als Karte wie in der App */
            int anz[UMFRAGE_MAX], leute, max = umfrage_zaehlen(u, anz, &leute), s = stimme_von(u, ich);
            ui_karte(NULL, -1);
            avatar(m->profil, ui_x(), ui_y(), 28);
            text(ui_x() + 36, ui_y() + 5, pname(m->profil), F_FETT, TH->text);
            ui_abstand(34);
            gfx_symbol(IC_BAR_CHART, ui_x(), ui_y(), 20, TH->text);
            int y0 = ui_y();
            const char *st[4]; int ln[4]; int n = text_umbrechen(m->text, F_TITEL, ui_breite() - 30, st, ln, 4);
            for (int k = 0; k < n; k++) text_n(ui_x() + 28, y0 + k * 24, st[k], ln[k], F_TITEL, TH->text);
            ui_abstand(n * 24 + 8);
            for (int k = 0; k < db.umfragen.v[u].n; k++) {
                int gew = s >= 0 && (db.stimmen.v[s].wahl & (1 << k)), pz = leute ? anz[k] * 100 / leute : 0;
                int y = ui_y(), x = ui_x(), b = ui_breite();
                int an = umfrage_zu(u) ? 0 : ui_kachel_knopf(x, y, b, 40);
                int f = ui_fokussiert_letztes() && !umfrage_zu(u);
                if (f) ui_fokus_rahmen(x, y, b, 40, 12);
                gfx_runde_box(x, y, b, 40, 12, TH->flaeche2);
                if (pz) gfx_runde_box(x, y, b * pz / 100 < 24 ? 24 : b * pz / 100, 40, 12, gew ? TH->accentCont : gfx_mischen(TH->flaeche2, TH->text, 40));
                if (gew) gfx_runder_rahmen(x, y, b, 40, 12, 2, TH->accent);
                text_max(x + (gew ? 36 : 14), y + 10, db.umfragen.v[u].antworten[k], b - 120, F_FETT, TH->text);
                if (gew) gfx_symbol(IC_CHECK, x + 10, y + 10, 20, TH->accentText);
                text_rechts(x + b - 12, y + 10, tmp("%d %%%s", pz, umfrage_zu(u) && anz[k] == max && max ? " 🏆" : ""), F_FETT, TH->text);
                ui_abstand(46);
                if (an) { Aktion *a = aktion_neu(A_STIMME); a->a = (int16_t)u; a->b = (int16_t)k; mach(a, NULL); }
            }
            ui_text(tmp("%d %s abgestimmt%s%s · %s", leute, leute == 1 ? "hat" : "haben", db.umfragen.v[u].mehrfach ? " · mehrere Antworten" : " · eine Antwort", umfrage_zu(u) ? " · beendet" : "", uhr_text(m->zeit)), F_KLEIN, TH->muted);
            if (m->profil == ich && !umfrage_zu(u) && ui_knopf_art("Umfrage beenden", KN_RAND, -1)) { Aktion *a = aktion_neu(A_UMFRAGE_ENDE); a->a = (int16_t)u; mach(a, NULL); }
            ui_karte_ende();
            continue;
        }
        if (ui_blase(m->profil, pname(m->profil), m->text, uhr_text(m->zeit), m->profil == ich, extra[0] ? extra : NULL, antwort)) chat_auswahl = i;
        int e[32], ne = reaktionen_zaehlen(i, e);
        if (ne) {
            char r[96] = ""; int p = 0;
            for (int k = 0; k < EMOJIS_N && p < 80; k++) if (e[k]) p += snprintf(r + p, sizeof r - (size_t)p, "%s %d  ", EMOJIS[k], e[k]);
            int b = text_breite(r, F_NORMAL) + 16, x = m->profil == ich ? ui_x() + ui_breite() - b : ui_x() + 56;
            gfx_runde_box(x, ui_y() - 12, b, 28, 14, TH->flaeche2);
            text(x + 8, ui_y() - 9, r, F_NORMAL, TH->text);
            ui_abstand(22);
        }
    }
    ui_abstand(8);
    if (antwort_auf >= 0) ui_symbolzeile(IC_REPLY, tmp("Antwort an %s", pname(db.nachrichten.v[antwort_auf].profil)), TH->accentText);
    if (ui_knopf_breit("Nachricht", KN_TONAL, IC_SEND) || (ui_taste() & T_Z)) tastatur(ID_NACHRICHT, antwort_auf >= 0 ? tmp("Antwort an %s", pname(db.nachrichten.v[antwort_auf].profil)) : "Nachricht an alle", "", AKTION_TEXT_MAX);
    if (ui_knopf_art("Umfrage", KN_RAND, IC_BAR_CHART)) tastatur(ID_UMFRAGE_FRAGE, "Umfrage: Was willst du wissen?", "", 120);
    ui_bereich_ende();
    if (chat_ende) { ui_fokus_letztes(); chat_ende = 0; nochmal_zeichnen = 1; }
    const char *nt = tastatur_ergebnis(ID_NACHRICHT);
    if (nt && nt[0]) { Aktion *a = aktion_neu(A_NACHRICHT); a->a = (int16_t)antwort_auf; strncpy(a->text, nt, AKTION_TEXT_MAX); if (mach(a, NULL)) chat_ende = 1; antwort_auf = -1; }
    static char frage_text[130], antworten[AKTION_TEXT_MAX + 1]; static int n_antw;
    const char *ft = tastatur_ergebnis(ID_UMFRAGE_FRAGE);
    if (ft && ft[0]) { strncpy(frage_text, ft, sizeof frage_text - 1); antworten[0] = 0; n_antw = 0; tastatur(ID_UMFRAGE_ANTW, "Antwort 1 (leer lassen = fertig)", "", 40); }
    const char *at = tastatur_ergebnis(ID_UMFRAGE_ANTW);
    int abgebrochen = erg_tastatur_abgeschlossen(ID_UMFRAGE_ANTW);
    if (at || abgebrochen) {
        if (at && at[0] && n_antw < UMFRAGE_MAX) {
            strncat(antworten, TRENN, sizeof antworten - strlen(antworten) - 1);
            strncat(antworten, at, sizeof antworten - strlen(antworten) - 1);
            n_antw++;
            if (n_antw < UMFRAGE_MAX) tastatur(ID_UMFRAGE_ANTW, tmp("Antwort %d (leer lassen = fertig)", n_antw + 1), "", 40);
        }
        if ((!at || !at[0] || n_antw >= UMFRAGE_MAX) && frage_text[0]) {
            if (n_antw < 2) meldung("Eine Umfrage braucht 2 bis 6 Antworten.");
            else {
                Aktion *a = aktion_neu(A_UMFRAGE_NEU);
                snprintf(a->text, sizeof a->text, "%s%s", frage_text, antworten);
                for (char *q = a->text; *q; q++) if (*q == '\x1f') *q = '\n';
                frage(ID_UMFRAGE_WAHL, "Darf man mehrere Antworten wählen?", "Mehrere", "Nur eine");
                static Aktion merk; merk = *a;
                extern Aktion *umfrage_merk; umfrage_merk = &merk;
            }
            frage_text[0] = 0;
        }
    }
    extern Aktion *umfrage_merk;
    int mw = frage_ergebnis(ID_UMFRAGE_WAHL);
    if (mw >= 0 && umfrage_merk) { Aktion *a = aktion_neu(A_UMFRAGE_NEU); strcpy(a->text, umfrage_merk->text); a->b = mw == 0; if (mach(a, NULL)) chat_ende = 1; umfrage_merk = NULL; }
    if (chat_auswahl >= 0) {
        static const char *opt[8]; int n = 0;
        const Nachricht *m = &db.nachrichten.v[chat_auswahl];
        opt[n] = "Reagieren"; chat_code[n++] = 0;
        if (m->art != NACHRICHT_SYSTEM) { opt[n] = "Antworten"; chat_code[n++] = 1; }
        opt[n] = m->angeheftet ? "Lösen" : "Anheften"; chat_code[n++] = 2;
        if (m->profil == ich && m->art != NACHRICHT_SYSTEM) { opt[n] = "Löschen"; chat_code[n++] = 4; }
        chat_merk = chat_auswahl;
        auswahl(ID_CHAT_AKTION, tmp("%s: %s", pname(m->profil), m->text), opt, n, 0);
    }
    int w = auswahl_ergebnis(ID_CHAT_AKTION);
    if (w >= 0) {
        int c = chat_code[w];
        if (c == 0) auswahl(ID_REAKTION, "Reaktion", (const char *const *)EMOJIS, EMOJIS_N, 0);
        else if (c == 1) { antwort_auf = chat_merk; tastatur(ID_NACHRICHT, tmp("Antwort an %s", pname(db.nachrichten.v[chat_merk].profil)), "", AKTION_TEXT_MAX); }
        else if (c == 2) { Aktion *a = aktion_neu(A_ANHEFTEN); a->a = (int16_t)chat_merk; mach(a, NULL); }
        else if (c == 4) { Aktion *a = aktion_neu(A_NACHRICHT_WEG); a->a = (int16_t)chat_merk; mach(a, NULL); }
    }
    w = auswahl_ergebnis(ID_REAKTION);
    if (w >= 0) { Aktion *a = aktion_neu(A_REAKTION); a->a = (int16_t)chat_merk; a->b = (int16_t)w; mach(a, NULL); }
}
Aktion *umfrage_merk;

/* ======================================================================= Ruhmeshalle */
void seite_ruhm(void) {
    seiten_kopf("Ruhmeshalle", -1);
    static const char *const R[3] = { "Dieses Treffen", "Ewige Bestenliste", "Trophäen" };
    int w = ui_segment(R, 3, ruhm_reiter);
    if (w >= 0) { ruhm_reiter = w; nochmal_zeichnen = 1; }
    if (ruhm_reiter < 2) {
        int t = ruhm_reiter == 0 ? ruhm_treffen() : -1;
        if (ruhm_reiter == 0 && t < 0) ui_text("Noch kein Treffen vorbei.", F_NORMAL, TH->muted);
        else {
            if (t >= 0) ui_text(treffen_titel(t), F_NORMAL, TH->muted);
            Rekord r[12]; int n = rekorde(t, r, 12);
            for (int i = 0; i < n; i++)
                if (ui_eintrag(r[i].profil, tmp("%s %s", r[i].emoji, r[i].titel), tmp("%s · %s", pname(r[i].profil), r[i].text), NULL)) meldung(r[i].erklaerung);
            if (!n) ui_text("Noch keine Rekorde.", F_NORMAL, TH->muted);
        }
    } else {
        if (ruhm_person < 0) ruhm_person = ich;
        if (ui_eintrag(ruhm_person, tmp("Trophäenschrank von %s", pname(ruhm_person)), "A: andere Person", "›")) {
            static const char *namen[MAX_PROFILE];
            for (int p = 0; p < db.profile.n; p++) namen[p] = pname(p);
            auswahl(ID_PROFIL_WECHSEL, "Wessen Trophäenschrank?", namen, db.profile.n, ruhm_person);
        }
        int pw = auswahl_ergebnis(ID_PROFIL_WECHSEL);
        if (pw >= 0) { ruhm_person = pw; nochmal_zeichnen = 1; }
        for (int gruppe = 0; gruppe < 2; gruppe++) {
            int er = 0, ges = 0;
            for (int c = 0; c < ERFOLGE_N; c++) if ((ERFOLGE[c].art == 'H') == gruppe) { ges++; if (erfolg_hat(ruhm_person, c)) er++; }
            ui_ueberschrift(gruppe ? "Heini-Geschichte" : "Klo-Trophäen", tmp("%d von %d", er, ges));
            int sp = 4, kb = ui_breite() / sp, k = 0, y0 = ui_y();
            for (int c = 0; c < ERFOLGE_N; c++) {
                const ErfolgArt *e = &ERFOLGE[c];
                if ((e->art == 'H') != gruppe) continue;
                int hat = erfolg_hat(ruhm_person, c); zeit_t erst = 0;
                for (int i = 0; i < db.erfolge.n; i++) if (db.erfolge.v[i].profil == ruhm_person && db.erfolge.v[i].code == c && (!erst || db.erfolge.v[i].erreicht < erst)) erst = db.erfolge.v[i].erreicht;
                int zeigen = hat || !e->geheim;
                int x = ui_x() + (k % sp) * kb, y = y0 + (k / sp) * 118;
                int an = ui_kachel_knopf(x + 3, y, kb - 6, 110), f = ui_fokussiert_letztes();
                if (f) ui_fokus_rahmen(x + 3, y, kb - 6, 110, 14);
                gfx_runde_box(x + 3, y, kb - 6, 110, 14, hat ? TH->accentCont : TH->flaeche);
                int i = 0, cp = utf8_naechstes(zeigen ? e->emoji : "❔", &i), ei = emoji_index((uint32_t)cp);
                if (ei >= 0) { gfx_emoji(ei, x + kb / 2 - 18, y + 8, 36); if (!hat) gfx_abdunkeln(x + kb / 2 - 18, y + 8, 36, 36, 150); }
                const char *st[2]; int ln[2];
                int n = text_umbrechen(zeigen ? e->name : "???", F_FETT, kb - 16, st, ln, 2);
                for (int z = 0; z < n; z++) { char puf[64]; int bytes = utf8_bytes_fuer(st[z], ln[z]); if (bytes > 63) bytes = 63; memcpy(puf, st[z], (size_t)bytes); puf[bytes] = 0; text_mitte(x + kb / 2, y + 48 + z * 18, puf, F_FETT, hat ? TH->onAccentCont : TH->text); }
                text_mitte(x + kb / 2, y + 88, hat ? (erst ? datum_voll(tag_von(erst)) : "erreicht") : "offen", F_KLEIN, hat ? TH->onAccentCont : TH->muted);
                if (an) meldung(zeigen ? tmp("%s %s: %s", e->emoji, e->name, e->hinweis) : "Geheim. Finde es selbst heraus!");
                k++;
            }
            ui_weiter(((k + sp - 1) / sp) * 118 + 8);
        }
    }
    ui_bereich_ende();
}

/* ======================================================================= Profil */
static int profil_feld;
static void feld_text(int feld, const char *titel, const char *wert, int max) { profil_feld = feld; tastatur(ID_PROFILFELD, titel, wert, max); }
static void feld_zahl(int feld, const char *titel, int wert, int stellen, int nachkomma, const char *einheit) { profil_feld = feld; zahl_art(ID_PROFILFELD + 1, titel, wert, stellen, nachkomma, einheit); }
static void feld_setzen_text(int feld, const char *s) { Aktion *a = aktion_neu(A_PROFIL); a->a = (int16_t)feld; strncpy(a->text, s, AKTION_TEXT_MAX); mach(a, NULL); }
static void feld_setzen(int feld, int v) { Aktion *a = aktion_neu(A_PROFIL); a->a = (int16_t)feld; a->betrag = v; mach(a, NULL); }
static int schalter(const char *titel, const char *unter, int an) {
    int f = ui_eintrag_symbol(an ? IC_CHECK : IC_SQUARE, NULL, titel, unter, an ? "an" : "aus");
    return f;
}

/* ---- Listen „a · b · c“ (Unverträglichkeiten, Mag ich nicht) ---- */
static int liste_teilen(const char *s, char teile[][48], int max) {
    int n = 0;
    while (s && *s && n < max) {
        const char *e = strstr(s, " · "); int l = e ? (int)(e - s) : (int)strlen(s);
        int k = l < 47 ? l : 47; memcpy(teile[n], s, (size_t)k); teile[n][k] = 0; if (k) n++;
        s += l; if (e) s += strlen(" · ");
    }
    return n;
}
static void liste_bauen(char *ziel, int groesse, char teile[][48], int n, int ohne) {
    ziel[0] = 0;
    for (int i = 0; i < n; i++) { if (i == ohne) continue; if (ziel[0]) strncat(ziel, " · ", (size_t)(groesse - (int)strlen(ziel) - 1)); strncat(ziel, teile[i], (size_t)(groesse - (int)strlen(ziel) - 1)); }
}
/* Chips wie .mchips im Original; fokus: jeder Chip ist wählbar, Rückgabe = gewählter Chip oder -1 */
static int chips(const char *liste, int waehlbar) {
    char teile[20][48]; int n = liste_teilen(liste, teile, 20), gewaehlt = -1;
    int x = ui_x(), y = ui_y(), zeilen = 1;
    for (int i = 0; i < n; i++) {
        int b = text_breite(teile[i], F_NORMAL) + 28 + (waehlbar ? 18 : 0);
        if (b > ui_breite()) b = ui_breite();
        if (x + b > ui_x() + ui_breite()) { x = ui_x(); y += 42; zeilen++; }
        if (waehlbar) { if (ui_kachel_knopf(x, y, b, 34)) gewaehlt = i; if (ui_fokussiert_letztes()) ui_fokus_rahmen(x, y, b, 34, 17); }
        gfx_runde_box(x, y, b, 34, 17, TH->flaeche2);
        text_max(x + 14, y + 8, teile[i], b - 28 - (waehlbar ? 18 : 0), F_NORMAL, TH->text);
        if (waehlbar) text(x + b - 26, y + 8, "×", F_NORMAL, TH->muted);
        x += b + 8;
    }
    if (n) ui_weiter(zeilen * 42 + 4);
    return gewaehlt;
}
static const char *konsole_kurz(const Konsole *ko) { return ko->art >= 0 && ko->art < KONSOLEN_N ? (KONSOLEN[ko->art].kurz && KONSOLEN[ko->art].kurz[0] ? KONSOLEN[ko->art].kurz : KONSOLEN[ko->art].name) : (ko->name && ko->name[0] ? ko->name : "Andere"); }
static int spiele_von(int k, int art) { int n = 0; for (int d = 0; d < db.dinge.n; d++) if (!db.dinge.v[d].weg && db.dinge.v[d].konsole == k && db.dinge.v[d].art == art) n += art == DING_SPIEL ? 1 : db.dinge.v[d].anzahl; return n; }
/* Konsolen als Farbkacheln (wie gamingHtml): bei Zusammenwohnenden die der ganzen Wohnung */
static void konsolen_kacheln(int p) {
    int partner = wohnt_mit(p), sp = 3, kb = ui_breite() / sp, k = 0, y0 = ui_y();
    if (partner >= 0) { ui_text(tmp("Bei %s & %s", pname(raum_besitzer(p)), pname(raum_besitzer(p) == p ? partner : p)), F_FETT, TH->muted); y0 = ui_y(); }
    for (int i = 0; i < db.konsolen.n; i++) {
        const Konsole *ko = &db.konsolen.v[i];
        if (ko->weg || (ko->profil != p && ko->profil != partner)) continue;
        int x = ui_x() + (k % sp) * kb, y = y0 + (k / sp) * 84;
        int an = ui_kachel_knopf(x + 3, y, kb - 6, 76);
        if (ui_fokussiert_letztes()) ui_fokus_rahmen(x + 3, y, kb - 6, 76, 14);
        uint16_t f = ko->art >= 0 && ko->art < KONSOLEN_N ? KONSOLEN[ko->art].farbe : TH->flaeche2;
        gfx_runde_box(x + 3, y, kb - 6, 76, 14, f);
        gfx_symbol(IC_GAMEPAD_2, x + 15, y + 10, 20, C_WEISS);
        text_max(x + 15, y + 34, konsole_kurz(ko), kb - 30, F_FETT, C_WEISS);
        int n = spiele_von(i, DING_SPIEL);
        text(x + 15, y + 54, tmp("%d %s", n, n == 1 ? "Spiel" : "Spiele"), F_KLEIN, C_WEISS);
        if (an) seite_auf(SE_GAMING, i);
        k++;
    }
    if (k) ui_weiter(((k + sp - 1) / sp) * 84 + 4);
}

/* Profil ansehen (profilAnsehen im Original): Kopf, Adresse, Gaming-Equipment, Auto, Pärchen, Getränke, Schlafen, Parken,
   PayPal, Räume, Essen, Verleiht, Wichtelgeschenke; beim eigenen Profil „Mein Profil bearbeiten“ */
static void profil_ansehen(int p) {
    const Profil *x = &db.profile.v[p];
    int eigen = p == ich, bes = raum_besitzer(p), partner = partner_von(p);
    const Profil *b = &db.profile.v[bes];
    seiten_kopf(pname(p), -1);
    /* Kopf in der Profilfarbe */
    const Thema *pt = &THEMEN[x->farbe < 20 ? x->farbe : 0][design_dunkel ? 1 : 0];
    int y = ui_y(), h = 150 + (x->status && x->status[0] ? 26 : 0);
    gfx_runde_box(ui_x(), y, ui_breite(), h, 24, pt->accentCont);
    avatar(p, BILD_B / 2 - 36, y + 14, 72);
    text_mitte(BILD_B / 2, y + 92, eigen ? tmp("%s (du)", pname(p)) : pname(p), F_KOPF, pt->onAccentCont);
    int yy = y + 126;
    if (x->status && x->status[0]) { text_mitte(BILD_B / 2, yy, x->status, F_NORMAL, pt->onAccentCont); yy += 26; }
    text_mitte(BILD_B / 2, yy, x->gastgeber && !x->aktiv ? "Gastgeber · Ehemalig" : x->gastgeber ? "Gastgeber" : !x->aktiv ? "Ehemalig" : "Heini", F_KLEIN, pt->onAccentCont);
    ui_weiter(h + 12);
    /* Adresse */
    if (b->strasse[0] || b->wohnort[0]) {
        ui_karte_art(KA_NORMAL, NULL, -1);
        ui_info_symbol(IC_MAP_PIN, b->strasse[0] ? tmp("%s, %s %s", b->strasse, b->plz, b->wohnort) : b->wohnort, "Adresse");
        ui_karte_ende();
    }
    /* Gaming-Equipment */
    int nk = 0, wm = wohnt_mit(p);
    for (int i = 0; i < db.konsolen.n; i++) if (!db.konsolen.v[i].weg && (db.konsolen.v[i].profil == p || db.konsolen.v[i].profil == wm)) nk++;
    if (nk) { ui_ueberschrift("Gaming-Equipment", tmp("%d", nk)); konsolen_kacheln(p); }
    /* Zeilen */
    int zeilen = x->auto_ || partner >= 0 || x->kein_alkohol || x->wunsch_temp || x->gastgeber || (x->startort && x->startort[0]);
    if (zeilen) {
        ui_karte_art(KA_NORMAL, NULL, -1);
        if (x->auto_) {
            char t[120]; int n = snprintf(t, sizeof t, "%s", x->auto_name[0] ? x->auto_name : "Hat ein Auto");
            if (x->auto_plaetze) n += snprintf(t + n, sizeof t - (size_t)n, " · %d %s frei", x->auto_plaetze, x->auto_plaetze == 1 ? "Platz" : "Plätze");
            n += snprintf(t + n, sizeof t - (size_t)n, " · %s", x->elektro ? "Elektro" : "Verbrenner");
            if (x->verbrauch) snprintf(t + n, sizeof t - (size_t)n, " · %d,%d %s/100 km", x->verbrauch / 10, x->verbrauch % 10, x->elektro ? "kWh" : "l");
            ui_info_symbol(IC_CAR, t, "Auto");
        }
        if (x->startort && x->startort[0]) ui_info_symbol(IC_MAP_PIN, x->startort, "Fährt meistens ab");
        if (partner >= 0) { int f = ui_flaeche(58); ui_person(partner, tmp("mit %s", partner == ich ? "dir" : pname(partner)), "Pärchen", "›"); if (f) seite_auf(SE_PROFIL, partner); }
        if (x->kein_alkohol) ui_info_symbol(IC_BEER, "Trinkt keinen Alkohol", "Getränke");
        if (x->wunsch_temp) ui_info_symbol(IC_THERMOMETER, tmp("Mag es nachts %d °C", x->wunsch_temp), "Schlafen");
        if (x->gastgeber) ui_info_symbol(IC_SQUARE_PARKING, tmp("%s%s%s", !b->parkplaetze ? "Keine Parkplätze" : tmp("%d %s", b->parkplaetze, b->parkplaetze == 1 ? "Parkplatz" : "Parkplätze"), b->park_hinweis[0] ? " · " : "", b->park_hinweis), "Parkplätze");
        ui_karte_ende();
    }
    if (x->paypal[0]) ui_symbolzeile(IC_EURO, tmp("PayPal: %s", x->paypal), TH->accentText);
    if (!(b->strasse[0] || b->wohnort[0]) && !nk && !zeilen) ui_text("Noch nichts eingetragen.", F_NORMAL, TH->muted);
    /* Räume */
    if (x->gastgeber) {
        int nr = 0;
        for (int r = 0; r < db.raeume.n; r++) if (!db.raeume.v[r].weg && db.raeume.v[r].profil == bes && db.raeume.v[r].treffen < 0) nr++;
        if (nr) {
            ui_ueberschrift("Räume", tmp("%d", nr));
            ui_karte_art(KA_NORMAL, NULL, -1);
            for (int r = 0; r < db.raeume.n; r++) {
                const Raum *ra = &db.raeume.v[r];
                if (ra->weg || ra->profil != bes || ra->treffen >= 0) continue;
                int np = 0; for (int k = 0; k < db.plaetze.n; k++) if (!db.plaetze.v[k].weg && db.plaetze.v[k].raum == r) np++;
                ui_info_symbol(ra->art == RAUM_BAD ? IC_BATH : IC_BED_DOUBLE, ra->name, ra->art == RAUM_BAD ? "Bad" : tmp("Schlafraum, %d %s", np, np == 1 ? "Schlafplatz" : "Schlafplätze"));
            }
            ui_karte_ende();
        }
    }
    /* Essen */
    if (x->unvertraeglich[0] || x->mag_nicht[0]) {
        ui_ueberschrift("Essen", NULL);
        ui_karte_art(KA_NORMAL, NULL, -1);
        ui_text("Unverträglichkeiten", F_FETT, TH->muted);
        if (x->unvertraeglich[0]) chips(x->unvertraeglich, 0); else ui_text("Nichts eingetragen.", F_KLEIN, TH->muted);
        ui_abstand(6);
        ui_text("Mag ich nicht", F_FETT, TH->muted);
        if (x->mag_nicht[0]) chips(x->mag_nicht, 0); else ui_text("Nichts eingetragen.", F_KLEIN, TH->muted);
        ui_karte_ende();
    }
    /* Verleiht */
    if (x->gastgeber) {
        int nu = 0;
        for (int u = 0; u < db.utensilien.n; u++) { const Utensil *ut = &db.utensilien.v[u]; if (!ut->weg && ut->profil == bes && (!ut->zaehlbar || ut->anzahl > 0)) nu++; }
        if (nu) {
            ui_ueberschrift("Verleiht", tmp("%d", nu));
            ui_karte_art(KA_NORMAL, NULL, -1);
            for (int u = 0; u < db.utensilien.n; u++) {
                const Utensil *ut = &db.utensilien.v[u];
                if (ut->weg || ut->profil != bes || (ut->zaehlbar && !ut->anzahl)) continue;
                ui_info_symbol(IC_PACKAGE, ut->name, ut->zaehlbar ? tmp("%d zum Ausleihen", ut->anzahl) : "gibt's dort");
            }
            ui_karte_ende();
        }
    }
    /* Wichtelgeschenke */
    int nw = 0;
    for (int i = 0; i < db.wichtel.n; i++) if (!db.wichtel.v[i].weg && db.wichtel.v[i].profil == p) nw++;
    if (nw) {
        ui_ueberschrift("Wichtelgeschenke", tmp("%d", nw));
        ui_karte_art(KA_NORMAL, NULL, -1);
        for (int j = db.wichtel.n - 1; j >= 0; j--) {
            const Wichtel *wi = &db.wichtel.v[j];
            if (wi->weg || wi->profil != p) continue;
            if (wi->von >= 0) { if (ui_flaeche(58)) seite_auf(SE_PROFIL, wi->von); ui_person(wi->von, tmp("%d · %s", wi->jahr, wi->geschenk), tmp("von %s", wi->von == ich ? "dir" : pname(wi->von)), NULL); }
            else ui_info_symbol(IC_GIFT, tmp("%d · %s", wi->jahr, wi->geschenk), NULL);
        }
        ui_karte_ende();
    }
    if (eigen && ui_knopf_breit("Mein Profil bearbeiten", KN_VOLL, IC_PENCIL)) seite_auf(SE_PROFIL, p + 1000);
    ui_bereich_ende();
}

void seite_profil(int p) {
    if (p < 1000) { profil_ansehen(p); return; }
    p -= 1000;
    const Profil *x = &db.profile.v[p];
    int eigen = p == ich;
    seiten_kopf(eigen ? "Profil bearbeiten" : pname(p), eigen ? IC_SETTINGS : -1) == KOPF_EXTRA ? seite_auf(SE_EINST, 0) : (void)0;
    /* Über dich */
    ui_karte("Über dich", IC_SMILE);
    int y = ui_y();
    avatar(p, ui_x() + 8, y + 6, 72);
    text(ui_x() + 100, y + 6, pname(p), F_GROSS, TH->text);
    text(ui_x() + 100, y + 46, tmp("%s%s", FARBEN[x->farbe].name, x->heini ? " · Heini" : ""), F_NORMAL, TH->muted);
    ui_abstand(92);
    if (x->status && x->status[0]) ui_text(tmp("„%s“", x->status), F_NORMAL, TH->text);
    if (eigen) {
        if (ui_knopf_art("Bild wählen", KN_TONAL, IC_SMILE)) seite_auf(SE_BILDWAHL, 0);
        if (ui_knopf_neben_art("Farbe", KN_TONAL, IC_SPARKLES)) seite_auf(SE_FARBWAHL, 0);
        if (ui_knopf_neben_art("Status", KN_TONAL, IC_PENCIL)) tastatur(ID_STATUS, "Was gibt's Neues? (eine Zeile)", x->status, 139);
        ui_text("Heini-Name bleibt für immer – einmal Heini, immer Heini.", F_KLEIN, TH->muted);
    }
    const char *stt = tastatur_ergebnis(ID_STATUS);
    if (stt) { Aktion *a = aktion_neu(A_STATUS); strncpy(a->text, stt, AKTION_TEXT_MAX); mach(a, NULL); }
    ui_karte_ende();
    /* Adresse */
    int bes = raum_besitzer(p);
    const Profil *b = &db.profile.v[bes];
    ui_karte("Adresse", IC_HOUSE);
    if (eigen && bes == p) {
        if (ui_eintrag_symbol(IC_MAP_PIN, NULL, b->strasse[0] ? b->strasse : "Straße fehlt", "Straße und Hausnummer", "›")) feld_text(P_STRASSE, "Straße und Hausnummer", b->strasse, 60);
        if (ui_eintrag_symbol(IC_MAP_PIN, NULL, b->plz[0] ? b->plz : "PLZ fehlt", "Postleitzahl (5 Ziffern)", "›")) feld_text(P_PLZ, "Postleitzahl", b->plz, 5);
        if (ui_eintrag_symbol(IC_MAP_PIN, NULL, b->wohnort[0] ? b->wohnort : "Wohnort fehlt", "Wohnort", "›")) feld_text(P_WOHNORT, "Wohnort", b->wohnort, 40);
    } else ui_text(b->strasse[0] || b->wohnort[0] ? tmp("%s, %s %s", b->strasse, b->plz, b->wohnort) : "Keine Adresse eingetragen.", F_NORMAL, TH->text);
    if (wohnt_mit(p) >= 0) ui_symbolzeile(IC_USERS, tmp("Wohnt zusammen mit %s", pname(wohnt_mit(p))), TH->muted);
    ui_karte_ende();
    /* Gaming */
    ui_karte("Gaming", IC_GAMEPAD_2);
    int nk = 0;
    for (int k = 0; k < db.konsolen.n; k++) {
        const Konsole *ko = &db.konsolen.v[k];
        if (ko->weg || ko->profil != p) continue;
        nk++;
        int sp = 0, zu = 0;
        for (int d = 0; d < db.dinge.n; d++) if (!db.dinge.v[d].weg && db.dinge.v[d].konsole == k) { if (db.dinge.v[d].art == DING_SPIEL) sp++; else zu++; }
        if (ui_eintrag_symbol(IC_GAMEPAD_2, NULL, ko->art >= 0 && ko->art < KONSOLEN_N ? KONSOLEN[ko->art].name : (ko->name && ko->name[0] ? ko->name : "Andere"), tmp("%d Spiele · %d Zubehör", sp, zu), "›")) seite_auf(SE_GAMING, k);
    }
    if (!nk) ui_text("Noch keine Konsolen.", F_NORMAL, TH->muted);
    if (eigen && ui_knopf_art("Konsole", KN_TONAL, IC_PLUS)) {
        static const char *namen[40]; int n = 0;
        for (int i = 0; i < KONSOLEN_N && n < 39; i++) namen[n++] = KONSOLEN[i].name;
        namen[n++] = "Andere …";
        auswahl(ID_KONSOLE, "Welche Konsole?", namen, n, 0);
    }
    int w = auswahl_ergebnis(ID_KONSOLE);
    if (w >= 0) { if (w >= KONSOLEN_N) tastatur(ID_KONSOLE + 500, "Name der Konsole", "", 40); else { Aktion *a = aktion_neu(A_KONSOLE_NEU); a->b = (int16_t)w; mach(a, NULL); } }
    const char *kt = tastatur_ergebnis(ID_KONSOLE + 500);
    if (kt && kt[0]) { Aktion *a = aktion_neu(A_KONSOLE_NEU); a->b = -1; strncpy(a->text, kt, AKTION_TEXT_MAX); mach(a, NULL); }
    ui_karte_ende();
    /* Auto */
    ui_karte("Auto", IC_CAR);
    if (eigen) {
        if (schalter("Ich habe ein Auto", x->auto_ ? (x->auto_name[0] ? x->auto_name : "ohne Namen") : "für Trips und Fahrtkosten", x->auto_)) feld_setzen(P_AUTO, !x->auto_);
        if (x->auto_) {
            if (ui_eintrag_symbol(IC_CAR, NULL, x->auto_name[0] ? x->auto_name : "Name des Autos", "Name", "›")) feld_text(P_AUTO_NAME, "Name des Autos", x->auto_name, 30);
            if (ui_eintrag_symbol(IC_USERS, NULL, tmp("%d Plätze", x->auto_plaetze), "mit Fahrer", "›")) feld_zahl(P_AUTO_PLAETZE, "Plätze mit Fahrer", x->auto_plaetze, 1, 0, "");
            if (ui_eintrag_symbol(IC_ZAP, NULL, tmp("%d,%d %s je 100 km", x->verbrauch / 10, x->verbrauch % 10, x->elektro ? "kWh" : "Liter"), "Verbrauch", "›")) feld_zahl(P_VERBRAUCH, "Verbrauch je 100 km", x->verbrauch, 3, 1, x->elektro ? "kWh" : "l");
            if (schalter("Elektroauto", "Verbrauch in kWh", x->elektro)) feld_setzen(P_ELEKTRO, !x->elektro);
            if (ui_eintrag_symbol(IC_MAP_PIN, NULL, x->startort && x->startort[0] ? x->startort : "Startort fehlt", "Von wo fährst du meistens los? (für Fahrten)", "›")) feld_text(P_STARTORT, "Startort (zum Beispiel Köln)", x->startort ? x->startort : "", 60);
        }
    } else ui_text(x->auto_ ? tmp("%s · %d Plätze · %d,%d %s/100 km", x->auto_name[0] ? x->auto_name : "Auto", x->auto_plaetze, x->verbrauch / 10, x->verbrauch % 10, x->elektro ? "kWh" : "l") : "Kein Auto.", F_NORMAL, TH->text);
    ui_karte_ende();
    /* Getränke, Zahlen, Schlafen, Essen */
    ui_karte("Getränke, Zahlen, Schlafen, Essen", IC_BEER);
    if (eigen) {
        if (schalter("Kein Alkohol", "Bei 🍺 zahlst du nicht mit", x->kein_alkohol)) feld_setzen(P_KEIN_ALKOHOL, !x->kein_alkohol);
        if (ui_eintrag_symbol(IC_EURO, NULL, x->paypal[0] ? x->paypal : "PayPal-Name fehlt", "paypal.me/…", "›")) feld_text(P_PAYPAL, "PayPal-Name (paypal.me/…)", x->paypal, 40);
        if (ui_eintrag_symbol(IC_THERMOMETER, NULL, x->wunsch_temp ? tmp("%d °C", x->wunsch_temp) : "egal", "Wunschtemperatur im Schlafraum (0 = egal)", "›")) feld_zahl(P_WUNSCH_TEMP, "Wunschtemperatur (15–24, 0 = egal)", x->wunsch_temp, 2, 0, "°C");
        static const struct { int feld; const char *titel, *bsp; } ES[2] = { { P_UNVERTRAEGLICH, "Unverträglichkeiten", "z. B. Laktose, Nüsse" }, { P_MAG_NICHT, "Mag ich nicht", "z. B. Pilze, Koriander" } };
        for (int e = 0; e < 2; e++) {
            const char *l = e == 0 ? x->unvertraeglich : x->mag_nicht;
            ui_abstand(6);
            ui_text(ES[e].titel, F_FETT, TH->muted);
            char teile[20][48]; int n = liste_teilen(l, teile, 20);
            int weg = chips(l, 1);
            if (!n) ui_text("Noch nichts eingetragen.", F_KLEIN, TH->muted);
            if (weg >= 0) { static char rest[1000]; liste_bauen(rest, sizeof rest, teile, n, weg); feld_setzen_text(ES[e].feld, rest); }
            if (ui_knopf_art(tmp("%s hinzufügen", e == 0 ? "Unverträglichkeit" : "Eintrag"), KN_TONAL, IC_PLUS)) tastatur(ID_PROFILFELD + 10 + e, tmp("%s (%s)", ES[e].titel, ES[e].bsp), "", 40);
            const char *neu = tastatur_ergebnis(ID_PROFILFELD + 10 + e);
            if (neu && neu[0]) {
                int doppelt = 0;
                for (int i = 0; i < n; i++) if (!strcmp(teile[i], neu)) doppelt = 1;
                if (doppelt) meldung("Steht schon in der Liste.");
                else { static char alles[1000]; snprintf(alles, sizeof alles, "%s%s%s", l, l[0] ? " · " : "", neu); feld_setzen_text(ES[e].feld, alles); }
            }
        }
        ui_text("Optional. Alle Heinis sehen es in deinem Profil, damit beim Kochen und Bestellen keiner leer ausgeht.", F_KLEIN, TH->muted);
    } else {
        ui_zeile("Getränke", x->kein_alkohol ? "kein Alkohol" : "alles", TH->text);
        if (x->paypal[0]) ui_zeile("PayPal", x->paypal, TH->text);
        ui_zeile("Wunschtemperatur", x->wunsch_temp ? tmp("%d °C", x->wunsch_temp) : "egal", TH->text);
        if (x->unvertraeglich[0]) ui_text(tmp("Verträgt nicht: %s", x->unvertraeglich), F_NORMAL, TH->muted);
        if (x->mag_nicht[0]) ui_text(tmp("Mag nicht: %s", x->mag_nicht), F_NORMAL, TH->muted);
    }
    ui_karte_ende();
    /* Pärchen */
    ui_karte("Pärchen", IC_HEART);
    int q = partner_von(p), paar = -1;
    for (int i = 0; i < db.paare.n; i++) if (!db.paare.v[i].weg && db.paare.v[i].status == PAAR_BESTAETIGT && (db.paare.v[i].von == p || db.paare.v[i].zu == p)) paar = i;
    if (q >= 0) {
        ui_person(q, tmp("Pärchen mit %s", pname(q)), wohnt_mit(p) >= 0 ? tmp("Wir wohnen zusammen · Räume von %s", pname(raum_besitzer(p))) : "Jeder hat seine eigenen Räume", NULL);
        if (eigen) {
            if (ui_knopf_art("Wir wohnen zusammen", KN_TONAL, IC_HOUSE)) {
                static const char *opt[3]; opt[0] = "Aus – jeder seine eigenen Räume"; opt[1] = tmp("Räume von %s", pname(p)); opt[2] = tmp("Räume von %s", pname(q));
                auswahl(ID_WOHNEN, "Wessen Räume und Dinge nutzt ihr?", opt, 3, wohnt_mit(p) < 0 ? 0 : raum_besitzer(p) == p ? 1 : 2);
            }
            if (ui_knopf_neben_art("Pärchen lösen", KN_RAND, IC_X)) frage(ID_PAAR + 1, tmp("Pärchen mit %s lösen?", pname(q)), "Lösen", "Behalten");
        }
    } else ui_text("Kein Pärchen.", F_NORMAL, TH->muted);
    if (eigen) {
        for (int i = 0; i < db.paare.n; i++) {
            const Paar *pa = &db.paare.v[i];
            if (pa->weg || pa->status != PAAR_OFFEN) continue;
            if (pa->zu == ich) {
                ui_person(pa->von, tmp("%s möchte ein Pärchen sein", pname(pa->von)), "Anfrage an dich", NULL);
                if (ui_knopf_art("Annehmen", KN_VOLL, IC_CHECK)) { Aktion *a = aktion_neu(A_PAAR_ANTWORT); a->a = (int16_t)i; a->b = 1; mach(a, "Pärchen bestätigt! Ab jetzt könnt ihr zusammen ins Bad."); }
                if (ui_knopf_neben_art("Ablehnen", KN_RAND, IC_X)) { Aktion *a = aktion_neu(A_PAAR_ANTWORT); a->a = (int16_t)i; a->b = 0; mach(a, "Abgelehnt. Autsch!"); }
            } else if (pa->von == ich) ui_text(tmp("Anfrage an %s läuft.", pname(pa->zu)), F_NORMAL, TH->muted);
        }
        if (q < 0 && ui_knopf_art("Pärchen anfragen", KN_TONAL, IC_HEART)) {
            static const char *namen[MAX_PROFILE]; static int idx[MAX_PROFILE]; int n = 0;
            for (int k = 0; k < db.profile.n; k++) if (k != ich && db.profile.v[k].aktiv) { idx[n] = k; namen[n++] = pname(k); }
            extern int paar_idx[MAX_PROFILE]; memcpy(paar_idx, idx, sizeof idx);
            auswahl(ID_PAAR, "Mit wem bist du ein Pärchen?", namen, n, 0);
        }
    }
    extern int paar_idx[MAX_PROFILE];
    w = auswahl_ergebnis(ID_PAAR);
    if (w >= 0) { Aktion *a = aktion_neu(A_PAAR_ANFRAGE); a->a = (int16_t)paar_idx[w]; mach(a, "Gefragt! Jetzt muss nur noch ja kommen."); }
    if (frage_ergebnis(ID_PAAR + 1) == 0 && paar >= 0) { Aktion *a = aktion_neu(A_PAAR_LOESEN); a->a = (int16_t)paar; mach(a, NULL); }
    w = auswahl_ergebnis(ID_WOHNEN);
    if (w >= 0 && paar >= 0) { Aktion *a = aktion_neu(A_WOHNEN); a->a = (int16_t)paar; a->b = (int16_t)(w == 0 ? -1 : w == 1 ? p : q); mach(a, w ? "Ihr nutzt jetzt dieselben Räume und Dinge." : "Jeder hat wieder seine eigenen Räume."); }
    ui_karte_ende();
    /* Wichtel */
    ui_karte("Wichtel", IC_GIFT);
    int nw = 0;
    for (int i = db.wichtel.n - 1; i >= 0; i--) {
        const Wichtel *wi = &db.wichtel.v[i];
        if (wi->weg || wi->profil != p) continue;
        nw++;
        if (ui_eintrag_symbol(IC_GIFT, NULL, tmp("%d: %s", wi->jahr, wi->geschenk), wi->von >= 0 ? tmp("von %s", pname(wi->von)) : "", NULL) && eigen)
            frage(ID_WICHTEL * 1000 + i, tmp("Geschenk %d löschen?", wi->jahr), "Löschen", "Behalten");
        if (frage_ergebnis(ID_WICHTEL * 1000 + i) == 0) { Aktion *a = aktion_neu(A_WICHTEL_WEG); a->a = (int16_t)i; mach(a, NULL); }
    }
    if (!nw) ui_text("Noch keine Wichtelgeschenke.", F_NORMAL, TH->muted);
    static int w_jahr, w_von;
    if (eigen && ui_knopf_art("Geschenk", KN_TONAL, IC_PLUS)) { int j, m, d; tag_zu_datum(tag_von(jetzt), &j, &m, &d); zahl_art(ID_WICHTEL, "Jahr", j, 4, 0, ""); }
    int32_t s;
    if (zahl_ergebnis(ID_WICHTEL, &s)) {
        w_jahr = (int)s;
        static const char *namen[MAX_PROFILE + 1]; namen[0] = "Weiß nicht mehr";
        for (int k = 0; k < db.profile.n; k++) namen[k + 1] = pname(k);
        auswahl(ID_WICHTEL + 1, "Von wem?", namen, db.profile.n + 1, 0);
    }
    w = auswahl_ergebnis(ID_WICHTEL + 1);
    if (w >= 0) { w_von = w - 1; tastatur(ID_WICHTEL + 2, tmp("Was hast du %d bekommen?", w_jahr), "", 100); }
    const char *gt = tastatur_ergebnis(ID_WICHTEL + 2);
    if (gt && gt[0]) { Aktion *a = aktion_neu(A_WICHTEL); a->b = (int16_t)w_jahr; a->c = (int16_t)w_von; strncpy(a->text, gt, AKTION_TEXT_MAX); mach(a, NULL); }
    ui_karte_ende();
    /* Gastgeber */
    ui_karte("Gastgeber", IC_HOUSE);
    if (eigen) {
        if (schalter("Ich kann Gastgeber sein", "Braucht eine vollständige Adresse", x->gastgeber)) feld_setzen(P_GASTGEBER, !x->gastgeber);
        if (x->gastgeber) {
            if (ui_eintrag_symbol(IC_SQUARE_PARKING, NULL, tmp("%d Parkplätze", b->parkplaetze), b->park_hinweis[0] ? b->park_hinweis : "Hinweis zum Parken", "›")) feld_zahl(P_PARKPLAETZE, "Parkplätze (0–20)", b->parkplaetze, 2, 0, "");
            if (ui_eintrag_symbol(IC_PENCIL, NULL, "Hinweis zum Parken", b->park_hinweis[0] ? b->park_hinweis : "keiner", "›")) feld_text(P_PARK_HINWEIS, "Hinweis zum Parken (eine Zeile)", b->park_hinweis, 200);
            int nr = 0, np = 0, nu = 0;
            for (int r = 0; r < db.raeume.n; r++) if (!db.raeume.v[r].weg && db.raeume.v[r].profil == bes && db.raeume.v[r].treffen < 0) nr++;
            for (int k = 0; k < db.plaetze.n; k++) if (!db.plaetze.v[k].weg && db.raeume.v[db.plaetze.v[k].raum].profil == bes && !db.raeume.v[db.plaetze.v[k].raum].weg) np++;
            for (int u = 0; u < db.utensilien.n; u++) if (!db.utensilien.v[u].weg && db.utensilien.v[u].profil == bes) nu++;
            if (ui_eintrag_symbol(IC_BED, NULL, "Räume, Schlafplätze und Ausleih-Dinge", tmp("%d Räume · %d Schlafplätze · %d Dinge", nr, np, nu), "›")) seite_auf(SE_RAEUME, 0);
        }
    } else ui_text(x->gastgeber ? tmp("Kann Gastgeber sein · %d Parkplätze", b->parkplaetze) : "Ist kein Gastgeber.", F_NORMAL, TH->text);
    ui_karte_ende();
    if (ui_eintrag_symbol(IC_TROPHY, NULL, "Trophäenschrank", NULL, "›")) { ruhm_reiter = 2; ruhm_person = p; seite_auf(SE_RUHM, 0); }
    ui_bereich_ende();
    const char *ft = tastatur_ergebnis(ID_PROFILFELD);
    if (ft) feld_setzen_text(profil_feld, ft);
    if (zahl_ergebnis(ID_PROFILFELD + 1, &s)) feld_setzen(profil_feld, (int)s);
}
int paar_idx[MAX_PROFILE];

/* Bild- und Farbwahl */
void seite_bildwahl(void) {
    seiten_kopf("Bild wählen", -1);
    int sp = 8, kb = ui_breite() / sp, y0 = ui_y();
    for (int i = 1; i <= 48; i++) {
        int x = ui_x() + ((i - 1) % sp) * kb, y = y0 + ((i - 1) / sp) * 72;
        int vergeben = -1; for (int q = 0; q < db.profile.n; q++) if (q != ich && db.profile.v[q].bild == i) vergeben = q;
        int an = ui_kachel_knopf(x + 4, y, kb - 8, 66), f = ui_fokussiert_letztes();
        if (f) ui_fokus_rahmen(x + 4, y, kb - 8, 66, 33);
        uint16_t hg = db.profile.v[ich].bild == i ? TH->accent : vergeben >= 0 ? TH->flaeche : pfarbe(ich);
        gfx_kreis(x + kb / 2, y + 30, 22, hg);
        gfx_bild4444(x + kb / 2 - 26, y + 4, 52, 52, AVA_DATEN[0] + (size_t)(i - 1) * 52 * 52);
        if (vergeben >= 0) { gfx_abdunkeln(x + 4, y, kb - 8, 62, 150); text_mitte(x + kb / 2, y + 50, tmp("%.6s", pname(vergeben)), F_KLEIN, TH->muted); }
        if (an) { Aktion *a = aktion_neu(A_PROFIL); a->a = P_BILD; a->betrag = i; if (mach(a, "Neues Bild!")) seite_zurueck(); }
    }
    ui_weiter(6 * 72 + 10);
    ui_bereich_ende();
}
void seite_farbwahl(void) {
    seiten_kopf("Deine Farbe", -1);
    int sp = 5, kb = ui_breite() / sp, y0 = ui_y();
    for (int i = 0; i < FARBEN_N; i++) {
        int x = ui_x() + (i % sp) * kb, y = y0 + (i / sp) * 96;
        int an = ui_kachel_knopf(x + kb / 2 - 30, y, 60, 60), f = ui_fokussiert_letztes();
        if (f) ui_fokus_rahmen(x + kb / 2 - 30, y, 60, 60, 30);
        gfx_kreis(x + kb / 2, y + 30, 28, FARBEN[i].farbe);
        if (db.profile.v[ich].farbe == i) gfx_symbol(IC_CHECK, x + kb / 2 - 14, y + 16, 28, FARBEN[i].hell ? C_SCHWARZ : C_WEISS);
        text_mitte(x + kb / 2, y + 66, FARBEN[i].name, F_KLEIN, TH->muted);
        if (an) { Aktion *a = aktion_neu(A_PROFIL); a->a = P_FARBE; a->betrag = i; mach(a, tmp("%s · die App nimmt deine Farbe an", FARBEN[i].name)); }
    }
    ui_weiter(4 * 96 + 10);
    ui_bereich_ende();
}

/* Räume, Plätze, Utensilien verwalten */
static int raum_wahl, utensil_wahl;
void seite_raeume(void) {
    seiten_kopf("Räume und Dinge", -1);
    int bes = raum_besitzer(ich);
    if (bes != ich) ui_symbolzeile(IC_USERS, tmp("Ihr wohnt zusammen: das sind die Räume von %s.", pname(bes)), TH->muted);
    for (int art = 0; art < 2; art++) {
        ui_ueberschrift(art == RAUM_BAD ? "Bäder" : "Schlafräume", NULL);
        for (int r = 0; r < db.raeume.n; r++) {
            const Raum *x = &db.raeume.v[r];
            if (x->weg || x->profil != bes || x->treffen >= 0 || x->art != art) continue;
            ui_karte(NULL, -1);
            if (ui_eintrag_symbol(art == RAUM_BAD ? IC_BATH : IC_BED, NULL, x->name, x->info && x->info[0] ? x->info : "keine Info", "⋮")) {
                static const char *const OPT_B[3] = { "Umbenennen", "Info (i) ändern", "Löschen" };
                static const char *const OPT_S[4] = { "Umbenennen", "Info (i) ändern", "Schlafplatz hinzufügen", "Löschen" };
                raum_wahl = r;
                auswahl(ID_RAUM, x->name, art == RAUM_BAD ? OPT_B : OPT_S, art == RAUM_BAD ? 3 : 4, 0);
            }
            for (int p = 0; p < db.plaetze.n; p++) {
                const Platz *pl = &db.plaetze.v[p];
                if (pl->weg || pl->raum != r) continue;
                if (ui_eintrag_symbol(IC_BED_DOUBLE, NULL, pl->name, pl->fuer_zwei ? "für zwei" : "für einen", "Löschen")) frage(ID_PLATZ * 1000 + p, tmp("Schlafplatz „%s“ löschen?", pl->name), "Löschen", "Behalten");
                if (frage_ergebnis(ID_PLATZ * 1000 + p) == 0) { Aktion *a = aktion_neu(A_PLATZ_WEG); a->a = (int16_t)p; mach(a, NULL); }
            }
            ui_karte_ende();
        }
        if (ui_knopf_art(art == RAUM_BAD ? "Bad" : "Schlafraum", KN_TONAL, IC_PLUS)) tastatur(ID_RAUM + 10 + art, art == RAUM_BAD ? "Name des Bades" : "Name des Schlafraums", "", 30);
        const char *rt = tastatur_ergebnis(ID_RAUM + 10 + art);
        if (rt && rt[0]) { Aktion *a = aktion_neu(A_RAUM_NEU); a->b = (int16_t)art; a->c = -1; strncpy(a->text, rt, AKTION_TEXT_MAX); mach(a, NULL); }
    }
    int w = auswahl_ergebnis(ID_RAUM);
    if (w >= 0) {
        const Raum *x = &db.raeume.v[raum_wahl];
        int code = x->art == RAUM_BAD && w == 2 ? 3 : w;
        if (code == 0) tastatur(ID_RAUM + 20, "Neuer Name", x->name, 30);
        else if (code == 1) tastatur(ID_RAUM + 21, "Info zum Raum (zum Beispiel: Föhn liegt im Regal)", x->info, 100);
        else if (code == 2) tastatur(ID_RAUM + 22, "Name des Schlafplatzes", "", 30);
        else frage(ID_RAUM + 23, tmp("„%s“ mit allen Schlafplätzen löschen?", x->name), "Löschen", "Behalten");
    }
    const char *rt = tastatur_ergebnis(ID_RAUM + 20);
    if (rt && rt[0]) { Aktion *a = aktion_neu(A_RAUM_AENDERN); a->a = (int16_t)raum_wahl; snprintf(a->text, sizeof a->text, "%s" TRENN "%s", rt, db.raeume.v[raum_wahl].info ? db.raeume.v[raum_wahl].info : ""); mach(a, NULL); }
    rt = tastatur_ergebnis(ID_RAUM + 21);
    if (rt) { Aktion *a = aktion_neu(A_RAUM_AENDERN); a->a = (int16_t)raum_wahl; snprintf(a->text, sizeof a->text, "%s" TRENN "%s", db.raeume.v[raum_wahl].name, rt); mach(a, NULL); }
    static char platz_name[40];
    rt = tastatur_ergebnis(ID_RAUM + 22);
    if (rt && rt[0]) { strncpy(platz_name, rt, sizeof platz_name - 1); platz_name[sizeof platz_name - 1] = 0; frage(ID_RAUM + 24, tmp("Ist „%s“ für zwei?", rt), "Für zwei", "Für einen"); }
    int fz = frage_ergebnis(ID_RAUM + 24);
    if (fz >= 0) { Aktion *a = aktion_neu(A_PLATZ_NEU); a->a = (int16_t)raum_wahl; a->b = fz == 0; strncpy(a->text, platz_name, AKTION_TEXT_MAX); mach(a, NULL); }
    if (frage_ergebnis(ID_RAUM + 23) == 0) { Aktion *a = aktion_neu(A_RAUM_WEG); a->a = (int16_t)raum_wahl; mach(a, NULL); }
    ui_ueberschrift("Zum Ausleihen", NULL);
    for (int u = 0; u < db.utensilien.n; u++) {
        const Utensil *x = &db.utensilien.v[u];
        if (x->weg || x->profil != bes) continue;
        if (ui_eintrag_symbol(IC_PACKAGE, NULL, x->name, tmp("%d Stück", x->anzahl), "⋮")) { static const char *const O[2] = { "Anzahl ändern", "Löschen" }; utensil_wahl = u; auswahl(ID_UTENSIL, x->name, O, 2, 0); }
    }
    if (ui_knopf_art("Ausleih-Ding", KN_TONAL, IC_PLUS)) tastatur(ID_UTENSIL + 1, "Was kann man ausleihen? (zum Beispiel Decke)", "", 30);
    static char u_name[40];
    rt = tastatur_ergebnis(ID_UTENSIL + 1);
    if (rt && rt[0]) { strncpy(u_name, rt, sizeof u_name - 1); u_name[sizeof u_name - 1] = 0; zahl_art(ID_UTENSIL + 2, tmp("Wie viele %s?", rt), 1, 2, 0, "Stück"); }
    int32_t s;
    if (zahl_ergebnis(ID_UTENSIL + 2, &s)) { Aktion *a = aktion_neu(A_UTENSIL_NEU); a->b = (int16_t)s; strncpy(a->text, u_name, AKTION_TEXT_MAX); mach(a, NULL); }
    w = auswahl_ergebnis(ID_UTENSIL);
    if (w == 0) zahl_art(ID_UTENSIL + 3, "Anzahl", db.utensilien.v[utensil_wahl].anzahl, 2, 0, "Stück");
    if (w == 1) { Aktion *a = aktion_neu(A_UTENSIL_WEG); a->a = (int16_t)utensil_wahl; mach(a, NULL); }
    if (zahl_ergebnis(ID_UTENSIL + 3, &s)) { Aktion *a = aktion_neu(A_UTENSIL_AENDERN); a->a = (int16_t)utensil_wahl; a->b = (int16_t)s; mach(a, NULL); }
    ui_bereich_ende();
}

/* Gaming: Spiele und Zubehör einer Konsole */
void seite_gaming(void) {
    int k = param();
    const Konsole *ko = &db.konsolen.v[k];
    const char *name = ko->art >= 0 && ko->art < KONSOLEN_N ? KONSOLEN[ko->art].name : (ko->name && ko->name[0] ? ko->name : "Andere");
    int eigen = ko->profil == ich;
    seiten_kopf(name, eigen ? IC_TRASH_2 : -1) == KOPF_EXTRA ? frage(ID_KONSOLE + 600, tmp("%s mit allen Spielen löschen?", name), "Löschen", "Behalten") : (void)0;
    if (frage_ergebnis(ID_KONSOLE + 600) == 0) { Aktion *a = aktion_neu(A_KONSOLE_WEG); a->a = (int16_t)k; if (mach(a, NULL)) { seite_zurueck(); return; } }
    if (ko->art >= 0 && ko->art < KONSOLEN_N) {
        ui_karte_art(KA_NORMAL, NULL, -1);
        gfx_runde_box(ui_x() - 16, ui_y() - 16, ui_breite() + 32, 64, 16, KONSOLEN[ko->art].farbe);
        gfx_symbol(IC_GAMEPAD_2, ui_x(), ui_y() + 2, 28, C_WEISS);
        text(ui_x() + 40, ui_y() + 2, name, F_TITEL, C_WEISS);
        ui_abstand(40);
        ui_text(tmp("gehört %s", pname(ko->profil)), F_NORMAL, TH->muted);
        ui_karte_ende();
    }
    for (int art = 0; art < 2; art++) {
        int n = 0;
        for (int d = 0; d < db.dinge.n; d++) if (!db.dinge.v[d].weg && db.dinge.v[d].konsole == k && db.dinge.v[d].art == art) n++;
        ui_ueberschrift(art == DING_SPIEL ? "Spiele" : "Zubehör", tmp("%d", n));
        for (int d = 0; d < db.dinge.n; d++) {
            const KonsolenDing *di = &db.dinge.v[d];
            if (di->weg || di->konsole != k || di->art != art) continue;
            if (ui_eintrag_symbol(art == DING_SPIEL ? IC_GAMEPAD_2 : IC_PACKAGE, NULL, di->name, di->anzahl > 1 ? tmp("%d Stück", di->anzahl) : NULL, eigen ? "Löschen" : NULL) && eigen)
                frage(ID_DING * 1000 + d, tmp("„%s“ löschen?", di->name), "Löschen", "Behalten");
            if (frage_ergebnis(ID_DING * 1000 + d) == 0) { Aktion *a = aktion_neu(A_DING_WEG); a->a = (int16_t)d; mach(a, NULL); }
        }
        if (eigen && ui_knopf_art(art == DING_SPIEL ? "Spiel" : "Zubehör", KN_TONAL, IC_PLUS)) tastatur(ID_DING + art, art == DING_SPIEL ? "Name des Spiels" : "Was für Zubehör? (zum Beispiel Controller)", "", 60);
        const char *dt = tastatur_ergebnis(ID_DING + art);
        if (dt && dt[0]) { Aktion *a = aktion_neu(A_DING_NEU); a->a = (int16_t)k; a->b = (int16_t)art; a->c = 1; strncpy(a->text, dt, AKTION_TEXT_MAX); mach(a, NULL); }
    }
    ui_text("Die Spielesuche (IGDB) braucht Internet – Namen hier selbst eintragen.", F_KLEIN, TH->muted);
    ui_bereich_ende();
}

/* ======================================================================= Treffen anlegen / bearbeiten */
static struct { int gast; tag_t von, bis; int ausserhalb; char titel[64], adresse[100], info[200]; } form;
void treffen_form_neu(int t) {
    memset(&form, 0, sizeof form);
    if (t >= 0) {
        const Treffen *x = &db.treffen.v[t];
        form.gast = x->gastgeber; form.von = x->von; form.bis = x->bis; form.ausserhalb = x->ausserhalb;
        strncpy(form.titel, x->titel ? x->titel : "", sizeof form.titel - 1);
        strncpy(form.adresse, x->adresse ? x->adresse : "", sizeof form.adresse - 1);
        strncpy(form.info, x->info ? x->info : "", sizeof form.info - 1);
    } else { form.gast = db.profile.v[ich].gastgeber ? ich : -1; form.von = tag_von(jetzt) + 7; form.bis = form.von + 2; }
}
static int form_gast_idx[MAX_PROFILE + 1];
void seite_treffen_form(int t) {
    seiten_kopf(t >= 0 ? "Treffen bearbeiten" : "Neues Treffen", -1);
    ui_karte(NULL, -1);
    if (ui_eintrag_symbol(IC_PENCIL, NULL, form.titel[0] ? form.titel : "Titel (optional, auswärts Pflicht)", "Titel", "›")) tastatur(ID_FORM, "Titel des Treffens", form.titel, 60);
    if (ui_eintrag_symbol(IC_HOUSE, NULL, form.gast >= 0 ? tmp("bei %s", pname(form.gast)) : "auswärts (ohne Gastgeber)", "Gastgeber", "›")) {
        static const char *namen[MAX_PROFILE + 1]; int n = 0;
        namen[n] = "Auswärts"; form_gast_idx[n++] = -1;
        for (int p = 0; p < db.profile.n; p++) if (db.profile.v[p].aktiv && db.profile.v[p].gastgeber) { namen[n] = pname(p); form_gast_idx[n++] = p; }
        auswahl(ID_FORM + 1, "Wo findet es statt?", namen, n, 0);
    }
    if (ui_eintrag_symbol(IC_CALENDAR, NULL, datum_lang(form.von), "Von", "›")) datum_dialog(ID_FORM + 2, "Erster Tag", (int32_t)form.von * 86400);
    if (ui_eintrag_symbol(IC_CALENDAR, NULL, datum_lang(form.bis), tmp("Bis · %d Tage", form.bis - form.von + 1), "›")) datum_dialog(ID_FORM + 3, "Letzter Tag", (int32_t)form.bis * 86400);
    if (form.gast >= 0 && ui_eintrag_symbol(form.ausserhalb ? IC_CHECK : IC_SQUARE, NULL, "Findet außerhalb statt", "Eigene Adresse statt der des Gastgebers", form.ausserhalb ? "an" : "aus")) form.ausserhalb = !form.ausserhalb;
    if ((form.gast < 0 || form.ausserhalb) && ui_eintrag_symbol(IC_MAP_PIN, NULL, form.adresse[0] ? form.adresse : "Adresse fehlt", "Adresse", "›")) tastatur(ID_FORM + 4, "Adresse", form.adresse, 99);
    if (ui_eintrag_symbol(IC_INFO, NULL, form.info[0] ? form.info : "Keine Infos", "Infos für alle (zum Beispiel: Bettwäsche mitbringen)", "›")) tastatur(ID_FORM + 5, "Infos für alle", form.info, 199);
    ui_karte_ende();
    if (ui_knopf_breit(t >= 0 ? "Speichern" : "Treffen anlegen", KN_VOLL, IC_CHECK)) {
        Aktion *a = aktion_neu(t >= 0 ? A_TREFFEN_AENDERN : A_TREFFEN_NEU);
        a->a = (int16_t)form.gast; a->b = form.von; a->c = form.bis; a->d = (int16_t)t; a->maske = (uint32_t)form.ausserhalb;
        snprintf(a->text, sizeof a->text, "%s" TRENN "%s" TRENN "%s", form.titel, form.adresse, form.info);
        if (mach(a, t >= 0 ? "Gespeichert." : "Treffen angelegt!")) seite_zurueck();
    }
    ui_bereich_ende();
    const char *s = tastatur_ergebnis(ID_FORM); if (s) strncpy(form.titel, s, sizeof form.titel - 1);
    s = tastatur_ergebnis(ID_FORM + 4); if (s) strncpy(form.adresse, s, sizeof form.adresse - 1);
    s = tastatur_ergebnis(ID_FORM + 5); if (s) strncpy(form.info, s, sizeof form.info - 1);
    int w = auswahl_ergebnis(ID_FORM + 1); if (w >= 0) form.gast = form_gast_idx[w];
    int32_t z;
    if (uhr_ergebnis(ID_FORM + 2, &z)) { int d = form.bis - form.von; form.von = tag_von(z); form.bis = (tag_t)(form.von + d); }
    if (uhr_ergebnis(ID_FORM + 3, &z)) form.bis = tag_von(z);
}

/* ======================================================================= Wer ist da? */
void seite_werda(void) {
    seiten_kopf("Wer ist da?", -1);
    int l = laufendes_treffen(), t = l >= 0 ? l : naechstes_treffen();
    for (int g = 0; g < 3; g++) {
        int n = 0;
        for (int p = 0; p < db.profile.n; p++) {
            if (!db.profile.v[p].aktiv) continue;
            int grp = l >= 0 && ist_anwesend(l, p) ? 0 : t >= 0 && hat_komme(t, p) ? 1 : 2;
            if (grp != g) continue;
            if (!n++) ui_ueberschrift(g == 0 ? "Ist da" : g == 1 ? "Kommt" : "Nicht dabei", NULL);
            const char *unter = db.profile.v[p].status && db.profile.v[p].status[0] ? db.profile.v[p].status : "";
            if (g == 0 && meine_uhr(p) >= 0) unter = tmp("ist im %s", db.raeume.v[db.buchungen.v[meine_uhr(p)].raum].name);
            if (ui_eintrag(p, pname(p), unter, "›")) seite_auf(SE_PROFIL, p);
        }
    }
    ui_bereich_ende();
}

/* ======================================================================= Frühere Treffen */
void seite_frueher(void) {
    seiten_kopf("Statistik", -1);
    HeiniZahl pers[32], gast[32]; int np, ng, tage;
    int n = frueher_statistik(pers, &np, gast, &ng, &tage);
    int kb = (ui_breite() - 12) / 2, x = ui_x(), y = ui_y();
    const char *werte[4] = { tmp("%d", n), tmp("%d", tage), tmp("%d,%d", n ? tage / n : 0, n ? (tage * 10 / n) % 10 : 0), tmp("%d", np) };
    const char *namen[4] = { "Treffen", "Tage zusammen", "Tage im Schnitt", "Heinis" };
    for (int i = 0; i < 4; i++) {
        int kx = x + (i % 2) * (kb + 12), ky = y + (i / 2) * 92;
        gfx_runde_box(kx, ky, kb, 82, 16, TH->flaeche);
        text(kx + 16, ky + 10, werte[i], F_GROSS, TH->text);
        text(kx + 16, ky + 52, namen[i], F_NORMAL, TH->muted);
    }
    ui_abstand(190);
    ui_ueberschrift("Am häufigsten dabei", NULL);
    for (int i = 0; i < np && i < 10; i++) {
        ui_zeile(tmp("%d. %s", i + 1, HEINI_NAMEN[pers[i].name]), tmp("%d · %d Tage · %d %%", pers[i].n, pers[i].tage, n ? pers[i].n * 100 / n : 0), TH->text);
        ui_balken(n ? pers[i].n * 1000 / n : 0, TH->accent);
    }
    ui_ueberschrift("Gastgeber", NULL);
    int gsum = 0; for (int i = 0; i < ng; i++) gsum += gast[i].n;
    for (int i = 0; i < ng && i < 8; i++) { ui_zeile(HEINI_NAMEN[gast[i].name], tmp("%d · %d %%", gast[i].n, gsum ? gast[i].n * 100 / gsum : 0), TH->text); ui_balken(gsum ? gast[i].n * 1000 / gast[0].n : 0, TH->ok); }
    int jahr = -1;
    for (int i = db.frueher.n - 1; i >= 0; i--) {
        const Frueher *f = &db.frueher.v[i];
        if (f->monat / 100 != jahr) { jahr = f->monat / 100; ui_ueberschrift(tmp("%d", jahr), NULL); }
        int k = 0; for (int h = 0; h < 32; h++) if (f->teilnehmer & (1u << h)) k++;
        const char *g = f->gastgeber >= 0 ? (f->gastgeber2 >= 0 ? tmp("%s & %s", HEINI_NAMEN[f->gastgeber], HEINI_NAMEN[f->gastgeber2]) : HEINI_NAMEN[f->gastgeber]) : "auswärts";
        if (ui_eintrag_symbol(-1, NULL, tmp("Nr. %d · %s", f->nr, f->name), tmp("%s · %d %s · %d Leute · %s", MONAT_NAME[(f->monat % 100 + 11) % 12], f->tage, f->tage == 1 ? "Tag" : "Tage", k, g), NULL)) {
            char tn[300] = ""; int pos = 0;
            for (int h = 0; h < HEINI_NAMEN_N; h++) if (f->teilnehmer & (1u << h)) pos += snprintf(tn + pos, sizeof tn - (size_t)pos, "%s%s", pos ? ", " : "", HEINI_NAMEN[h]);
            meldung(tmp("Dabei: %s", tn));
        }
    }
    ui_bereich_ende();
}

/* ======================================================================= Einstellungen */
void seite_einst(void) {
    seiten_kopf("Einstellungen", -1);
    ui_ueberschrift("Design", NULL);
    static const char *const D[2] = { "Hell", "Dunkel" };
    int w = ui_segment(D, 2, design_dunkel);
    if (w >= 0 && w != design_dunkel) { Aktion *a = aktion_neu(A_DESIGN); a->profil = (int8_t)(ich >= 0 ? ich : -1); a->b = (int16_t)w; tu(a); nochmal_zeichnen = 1; }
    ui_ueberschrift("Uhr", NULL);
    ui_text(tmp("%s, %s Uhr", datum_lang(tag_von(jetzt)), uhr_text(jetzt)), F_TITEL, TH->text);
    ui_text(uhr_gestellt == 2 ? "Die Zeit kommt von der Echtzeituhr des Moduls." : "Ohne Echtzeituhr im Modul läuft die Uhr ab dem Stellen weiter; nach dem Ausschalten bitte neu stellen.", F_KLEIN, TH->muted);
    if (ui_knopf_art("Uhr stellen", KN_TONAL, IC_CLOCK)) uhr_dialog(ID_UHR, jetzt);
    int32_t s;
    if (uhr_ergebnis(ID_UHR, &s)) { uhr_setzen(s); if (uhr_gestellt != 2) uhr_gestellt = 1; Aktion *a = aktion_neu(A_UHR); a->profil = -1; a->zeit = s; tu(a); nochmal_zeichnen = 1; }
    ui_ueberschrift("Regeln", NULL);
    if (ui_eintrag_symbol(IC_BATH, NULL, tmp("%d Minuten", regel_dauer_bad), "Vorschlag-Dauer fürs Bad", ich >= 0 ? "›" : NULL) && ich >= 0) {
        static const char *const DD[8] = { "5 Minuten", "10 Minuten", "15 Minuten", "20 Minuten", "25 Minuten", "30 Minuten", "45 Minuten", "60 Minuten" };
        auswahl(ID_REGEL, "Vorschlag-Dauer fürs Bad", DD, 8, 2);
    }
    ui_eintrag_symbol(IC_CAR, NULL, regel_verschleiss ? tmp("an, %d ct/km", regel_verschleiss_ct) : "aus", "Verschleißpauschale für Fahrten", NULL);
    w = auswahl_ergebnis(ID_REGEL);
    if (w >= 0) { static const int M[8] = { 5, 10, 15, 20, 25, 30, 45, 60 }; Aktion *a = aktion_neu(A_REGEL); a->a = (int16_t)M[w]; mach(a, NULL); }
    ui_ueberschrift("Speicher auf dem Modul", NULL);
    if (journal_da()) {
        ui_zeile("Änderungen am Fernseher", tmp("%d", journal_eintraege()), TH->text);
        ui_zeile("Frei", tmp("%d von %d Byte", journal_frei(), SPEICHER_GROESSE - 24), TH->text);
        ui_balken(1000 - journal_frei() * 1000 / (SPEICHER_GROESSE - 24), TH->accent);
        if (ui_eintrag_symbol(IC_HISTORY, NULL, "Änderungsverlauf", "Wer hat am Fernseher was geändert?", "›")) seite_auf(SE_VERLAUF, 0);
        if (ui_knopf_art("Alle Änderungen am Fernseher löschen", KN_RAND, IC_TRASH_2)) frage(ID_JOURNAL_WEG, "Wirklich alles löschen, was am Fernseher eingetragen wurde? Zurück bleibt der Stand aus dem Export.", "Löschen", "Abbrechen");
        if (frage_ergebnis(ID_JOURNAL_WEG) == 0) { journal_loeschen(); journal_laden(); meldung("Gelöscht. Stand wie beim Export."); }
    } else ui_text("Kein SRAM gefunden. Änderungen gelten nur bis zum Ausschalten. Auf Flashcarts den Speichertyp „SRAM 256k“ wählen.", F_NORMAL, TH->warn);
    ui_ueberschrift("Hilfe", NULL);
    if (ui_eintrag_symbol(IC_BOOK_OPEN, NULL, "Tour", "So funktioniert die App am Fernseher", "›")) seite_auf(SE_TOUR, 0);
    ui_ueberschrift("Über die App", NULL);
    ui_text("Heini-App 64 – die Heini-App 1.61.0 als stationäre App am Fernseher.", F_NORMAL, TH->text);
    ui_text(tmp("Daten aus dem Export vom %s (Datenstand %u).", EXPORT_STAND, (unsigned)EXPORT_KENNUNG), F_KLEIN, TH->muted);
    ui_text("Ohne Internet: kein Abgleich mit dem Pi, keine Push-Nachrichten und Erinnerungen, keine HeinAI, keine Spielesuche, keine Fotos und Sprachnachrichten, kein Feedback.", F_KLEIN, TH->muted);
    if (ONEDRIVE_LINK[0]) ui_text(tmp("Fotos und Videos: %s", ONEDRIVE_LINK), F_KLEIN, TH->muted);
    ui_text("Schrift Roboto Flex (OFL), Symbole Lucide (ISC), Emojis Noto Color Emoji (OFL), Bootcode libdragon IPL3 (Public Domain).", F_KLEIN, TH->faint);
    ui_bereich_ende();
}

/* ======================================================================= Änderungsverlauf */
void seite_verlauf(void) {
    seiten_kopf("Änderungsverlauf", -1);
    if (!db.verlauf.n) ui_text("Am Fernseher wurde noch nichts geändert.", F_NORMAL, TH->muted);
    tag_t letzter = -1;
    for (int i = db.verlauf.n - 1; i >= 0 && i >= db.verlauf.n - 150; i--) {
        const Verlauf *v = &db.verlauf.v[i];
        if (tag_von(v->zeit) != letzter) { letzter = tag_von(v->zeit); ui_ueberschrift(datum_lang(letzter), NULL); }
        ui_eintrag(v->profil, v->text, uhr_text(v->zeit), NULL);
    }
    ui_bereich_ende();
}

/* ======================================================================= Tour */
static int tour_seite;
void seite_tour(void) {
    static const struct { const char *emoji, *titel, *text; } T[7] = {
        { "🚽", "Willkommen bei der Heini-App 64", "Bad, Dusche, Klo, Schlafplatz und Ausleih-Dinge beim Freundestreffen – jetzt am Fernseher. Jeder wählt im Hauptmenü sein Profil." },
        { "🎮", "Bedienung", "Steuerkreuz oder Stick: auswählen. A: drücken. B: zurück. L und R: Reiter wechseln. Z: Chat. START: Menü. C-hoch/C-runter: schnell rollen." },
        { "⏱", "Rein und freigeben", "Auf „Heute“ hat jedes Bad eine Karte. „Rein“ startet deine Uhr, alle Angekommenen sehen sie. Fertig? „Bad jetzt freigeben“. Besetzt? „Ich will danach“." },
        { "🧻", "Klopapier-Alarm", "Papier leer? Der rote Knopf neben deiner Uhr schlägt Alarm. Alle Anwesenden sehen ihn groß auf dem Fernseher, bis jemand „Ich bring welche“ drückt." },
        { "🛏", "Mein Heini-Treffen", "Die goldene Karte: Schlafplatz je Nacht nehmen, Decke und Co. ausleihen, Trips und was heute für dich ansteht." },
        { "💶", "Kasse & Trips", "Einkäufe, Essen und Getränke eintragen. Jeder zahlt direkt an den, der bezahlt hat. Fahrer rechnen im Trip die Fahrtkosten ab." },
        { "🏆", "Ruhmeshalle", "Pokal oben: Rekorde des Treffens, ewige Bestenliste und dein Trophäenschrank. Alles am Fernseher bleibt auf dem Modul gespeichert." } };
    gfx_fuellen(TH->bg);
    if (tour_seite < 0 || tour_seite > 6) tour_seite = 0;
    int i = 0, cp = utf8_naechstes(T[tour_seite].emoji, &i);
    gfx_kreis(BILD_B / 2, 120, 64, TH->accentCont);
    gfx_emoji(emoji_index((uint32_t)cp), BILD_B / 2 - 18, 102, 36);
    text_mitte(BILD_B / 2, 206, T[tour_seite].titel, F_GROSS, TH->text);
    ui_bereich(250, BILD_H);
    ui_mitte(0);
    ui_text(T[tour_seite].text, F_NORMAL, TH->muted);
    int dy = ui_y() + 8;
    for (int k = 0; k < 7; k++) gfx_kreis(BILD_B / 2 - 60 + k * 20, dy, k == tour_seite ? 6 : 4, k == tour_seite ? TH->accent : TH->edge);
    ui_abstand(26);
    if (ui_knopf_breit(tour_seite < 6 ? "Weiter" : "Los geht's", KN_VOLL, -1)) { if (tour_seite < 6) tour_seite++; else { tour_seite = 0; seite_zurueck(); } nochmal_zeichnen = 1; }
    ui_bereich_ende();
    if (ui_taste() & T_B) { if (tour_seite > 0) tour_seite--; else seite_zurueck(); nochmal_zeichnen = 1; }
}
