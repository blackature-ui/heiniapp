/* text.h – Texte sind UTF-8. Die Schrift kennt Latin-1 (U+0000..U+00FF); die Symbole der App
 * (Klo, Klopapier, Herz, ...) liegen auf U+0080..U+009F. Alles andere wird als „?“ gezeigt. */
#ifndef TEXT_H
#define TEXT_H
#include <stddef.h>
#include "model.h"

/* Symbole der Schrift (tools/make_assets.py) */
#define S_EURO      "\xc2\x80"
#define S_KLO       "\xc2\x81"
#define S_KLOPAPIER "\xc2\x82"
#define S_KACKE     "\xc2\x83"
#define S_DUSCHE    "\xc2\x84"
#define S_LACHEN    "\xc2\x85"
#define S_HERZ      "\xc2\x86"
#define S_DAUMEN    "\xc2\x87"
#define S_DAUMEN_R  "\xc2\x88"
#define S_BIER      "\xc2\x89"
#define S_POKAL     "\xc2\x8a"
#define S_PAD       "\xc2\x8b"
#define S_AUTO      "\xc2\x8c"
#define S_BETT      "\xc2\x8d"
#define S_KRONE     "\xc2\x8e"
#define S_STERN     "\xc2\x8f"
#define S_HAKEN     "\xc2\x90"
#define S_KREUZ     "\xc2\x91"
#define S_PUNKT     "\xc2\x92"
#define S_RECHTS    "\xc2\x93"
#define S_LINKS     "\xc2\x94"
#define S_HOCH      "\xc2\x95"
#define S_RUNTER    "\xc2\x96"
#define S_BIRNE     "\xc2\x97"
#define S_NADEL     "\xc2\x98"
#define S_UHR       "\xc2\x99"
#define S_BOX       "\xc2\x9a"
#define S_BOX_JA    "\xc2\x9b"
#define S_KAMERA    "\xc2\x9c"
#define S_NOTE      "\xc2\x9d"
#define S_HAUS      "\xc2\x9e"
#define S_ALARM     "\xc2\x9f"
#define S_MITTE     "\xc2\xb7"
#define S_GRAD      "\xc2\xb0"


/* Puffer für kurzlebige Texte (Ringpuffer, reicht für ein Bild) */
char *tmp(const char *fmt, ...);

/* Datum */
void tag_zu_datum(tag_t t, int *j, int *m, int *d);
tag_t datum_zu_tag(int j, int m, int d);
int wochentag(tag_t t);                 /* 0 = Montag */
extern const char *const WOCHENTAG_KURZ[7];
extern const char *const WOCHENTAG_LANG[7];
extern const char *const MONAT_NAME[12];
const char *datum_text(tag_t t);         /* "Sa 21.11." */
const char *datum_lang(tag_t t);         /* "Samstag, 21. November 2026" */
const char *datum_voll(tag_t t);         /* "21.11.2026" */
const char *zeitraum_text(tag_t von, tag_t bis);   /* "Fr 20. – So 22.11.2026" */
const char *uhr_text(zeit_t z);          /* "14:05" */
const char *wann_text(zeit_t z, zeit_t jetzt);     /* "14:05" heute, "gestern 14:05", "Sa 21.11. 14:05" */
const char *dauer_text(int sek);         /* "4:07" bzw. "1:04:07" */
const char *euro_text(int32_t cent);     /* "22,84 €" */
static inline tag_t tag_von(zeit_t z) { return (tag_t)(z / 86400); }
static inline int minute_von(zeit_t z) { return (int)((z % 86400) / 60); }

/* Zeilenumbruch: liefert Anzahl Zeilen, starts/laengen bis max Zeilen */
int umbrechen(const char *s, int max_zeichen, const char **starts, int *laengen, int max);

char *text_kopie(const char *s);
/* UTF-8 */
int utf8_naechstes(const char *s, int *i);          /* Zeichen-Code an s[*i], *i rückt vor; 0 am Ende */
int utf8_laenge(const char *s);                      /* Anzahl Zeichen */
int utf8_bytes_fuer(const char *s, int zeichen);     /* Bytes für die ersten n Zeichen */
const char *sym(int code);                           /* ein Zeichen-Code als UTF-8-Text */

#endif
