/* text.c – Datums-, Zeit- und Geldtexte, UTF-8, Zeilenumbruch */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "text.h"

/* ---------- Ringpuffer für kurze Texte ---------- */
#define TMP_N 96
#define TMP_GROESSE 192
static char tmp_puffer[TMP_N][TMP_GROESSE];
static int tmp_pos;

char *tmp(const char *fmt, ...) {
    char *p = tmp_puffer[tmp_pos];
    tmp_pos = (tmp_pos + 1) % TMP_N;
    va_list a;
    va_start(a, fmt);
    vsnprintf(p, TMP_GROESSE, fmt, a);
    va_end(a);
    return p;
}

char *text_kopie(const char *s) {
    size_t n = strlen(s ? s : "");
    char *k = malloc(n + 1);
    memcpy(k, s ? s : "", n + 1);
    return k;
}

/* ---------- UTF-8 ---------- */
int utf8_naechstes(const char *s, int *i) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned c = u[*i];
    if (!c) return 0;
    (*i)++;
    if (c < 0x80) return (int)c;
    int folge = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
    unsigned cp = c & (0x3f >> folge);
    for (int k = 0; k < folge; k++) {
        unsigned d = u[*i];
        if ((d & 0xc0) != 0x80) return '?';
        cp = (cp << 6) | (d & 0x3f);
        (*i)++;
    }
    return folge ? (int)cp : '?';
}
int utf8_laenge(const char *s) {
    int i = 0, n = 0;
    while (utf8_naechstes(s, &i)) n++;
    return n;
}
int utf8_bytes_fuer(const char *s, int zeichen) {
    int i = 0;
    while (zeichen-- > 0 && utf8_naechstes(s, &i)) {}
    return i;
}
const char *sym(int code) {
    static char puf[16][4];
    static int pos;
    char *p = puf[pos];
    pos = (pos + 1) & 15;
    if (code < 0x80) { p[0] = (char)code; p[1] = 0; }
    else { p[0] = (char)(0xc0 | (code >> 6)); p[1] = (char)(0x80 | (code & 0x3f)); p[2] = 0; }
    return p;
}

/* ---------- Datum ---------- */
const char *const WOCHENTAG_KURZ[7] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
const char *const WOCHENTAG_LANG[7] = {"Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"};
const char *const MONAT_NAME[12] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August",
                                    "September", "Oktober", "November", "Dezember"};

/* Tage seit 1970 (Algorithmus von Howard Hinnant) */
static long tage_seit_1970(int j, int m, int d) {
    j -= m <= 2;
    long era = (j >= 0 ? j : j - 399) / 400;
    long yoe = j - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
#define TAG_2020 18262L  /* tage_seit_1970(2020,1,1) */

tag_t datum_zu_tag(int j, int m, int d) { return (tag_t)(tage_seit_1970(j, m, d) - TAG_2020); }

void tag_zu_datum(tag_t t, int *j, int *m, int *d) {
    long z = (long)t + TAG_2020 + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    long dd = doy - (153 * mp + 2) / 5 + 1;
    long mm = mp + (mp < 10 ? 3 : -9);
    *j = (int)(y + (mm <= 2)); *m = (int)mm; *d = (int)dd;
}
int wochentag(tag_t t) { long w = ((long)t + 2) % 7; return (int)(w < 0 ? w + 7 : w); } /* 01.01.2020 war ein Mittwoch */

const char *datum_text(tag_t t) {
    int j, m, d; tag_zu_datum(t, &j, &m, &d);
    return tmp("%s %d.%d.", WOCHENTAG_KURZ[wochentag(t)], d, m);
}
const char *datum_lang(tag_t t) {
    int j, m, d; tag_zu_datum(t, &j, &m, &d);
    return tmp("%s, %d. %s %d", WOCHENTAG_LANG[wochentag(t)], d, MONAT_NAME[m - 1], j);
}
const char *datum_voll(tag_t t) {
    int j, m, d; tag_zu_datum(t, &j, &m, &d);
    return tmp("%02d.%02d.%d", d, m, j);
}
const char *zeitraum_text(tag_t von, tag_t bis) {
    int j1, m1, d1, j2, m2, d2;
    tag_zu_datum(von, &j1, &m1, &d1); tag_zu_datum(bis, &j2, &m2, &d2);
    if (von == bis) return tmp("%s %d.%d.%d", WOCHENTAG_KURZ[wochentag(von)], d1, m1, j1);
    if (j1 == j2 && m1 == m2)
        return tmp("%s %d. – %s %d.%d.%d", WOCHENTAG_KURZ[wochentag(von)], d1, WOCHENTAG_KURZ[wochentag(bis)], d2, m2, j2);
    if (j1 == j2)
        return tmp("%s %d.%d. – %s %d.%d.%d", WOCHENTAG_KURZ[wochentag(von)], d1, m1, WOCHENTAG_KURZ[wochentag(bis)], d2, m2, j2);
    return tmp("%d.%d.%d – %d.%d.%d", d1, m1, j1, d2, m2, j2);
}
const char *uhr_text(zeit_t z) {
    int m = minute_von(z);
    return tmp("%d:%02d", m / 60, m % 60);
}
const char *wann_text(zeit_t z, zeit_t jetzt) {
    tag_t a = tag_von(z), b = tag_von(jetzt);
    if (a == b) return uhr_text(z);
    if (a == b - 1) return tmp("gestern %s", uhr_text(z));
    return tmp("%s %s", datum_text(a), uhr_text(z));
}
const char *dauer_text(int sek) {
    if (sek < 0) sek = 0;
    int h = sek / 3600, m = (sek / 60) % 60, s = sek % 60;
    return h ? tmp("%d:%02d:%02d", h, m, s) : tmp("%d:%02d", m, s);
}
const char *euro_text(int32_t cent) {
    int32_t a = cent < 0 ? -cent : cent;
    int32_t ganz = a / 100;
    char g[24];
    if (ganz >= 1000) snprintf(g, sizeof g, "%ld.%03ld", (long)(ganz / 1000), (long)(ganz % 1000));
    else snprintf(g, sizeof g, "%ld", (long)ganz);
    return tmp("%s%s,%02ld " S_EURO, cent < 0 ? "-" : "", g, (long)(a % 100));
}

/* ---------- Zeilenumbruch (nach Zeichen, an Leerzeichen) ---------- */
int umbrechen(const char *s, int max_zeichen, const char **starts, int *laengen, int max) {
    int n = 0;
    if (max_zeichen < 4) max_zeichen = 4;
    const char *p = s ? s : "";
    while (*p && n < max) {
        /* eine Zeile: bis Zeilenende oder max_zeichen Zeichen */
        int i = 0, zeichen = 0, letztes_leer = -1, zeichen_bis_leer = 0, c;
        while (p[i] && p[i] != '\n' && zeichen < max_zeichen) {
            int vor = i;
            c = utf8_naechstes(p, &i);
            if (c == ' ') { letztes_leer = vor; zeichen_bis_leer = zeichen; }
            zeichen++;
        }
        int laenge = i, anzahl = zeichen;
        if (p[i] && p[i] != '\n' && letztes_leer > 0) { laenge = letztes_leer; anzahl = zeichen_bis_leer; }
        starts[n] = p; laengen[n] = anzahl; n++;   /* laengen = Zeichen (nicht Bytes) */
        p += laenge;
        if (*p == '\n' || *p == ' ') p++;
    }
    if (n == 0 && max > 0) { starts[0] = p; laengen[0] = 0; n = 1; }
    return n;
}
