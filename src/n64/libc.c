/* libc.c – das Nötigste an C-Bibliothek für die N64 (ohne newlib). */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---------- Speicher ---------- */
void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d; const unsigned char *b = s;
    if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) {
        while (n >= 4) { *(uint32_t *)a = *(const uint32_t *)b; a += 4; b += 4; n -= 4; }
    }
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    unsigned char *a = d; const unsigned char *b = s;
    if (a == b || n == 0) return d;
    if (a < b) { while (n--) *a++ = *b++; }
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
void *memset(void *d, int c, size_t n) {
    unsigned char *a = d;
    uint32_t w = (unsigned char)c; w |= w << 8; w |= w << 16;
    while (n && ((uintptr_t)a & 3)) { *a++ = (unsigned char)c; n--; }
    while (n >= 4) { *(uint32_t *)a = w; a += 4; n -= 4; }
    while (n--) *a++ = (unsigned char)c;
    return d;
}
int memcmp(const void *x, const void *y, size_t n) {
    const unsigned char *a = x, *b = y;
    for (; n; n--, a++, b++) if (*a != *b) return *a - *b;
    return 0;
}
size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b || !*a) return (unsigned char)*a - (unsigned char)*b; }
    return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
char *strncpy(char *d, const char *s, size_t n) {
    char *r = d;
    while (n && *s) { *d++ = *s++; n--; }
    while (n--) *d++ = 0;
    return r;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strncat(char *d, const char *s, size_t n) {
    char *e = d + strlen(d);
    while (n-- && *s) *e++ = *s++;
    *e = 0;
    return d;
}
char *strchr(const char *s, int c) {
    for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return NULL; }
}
char *strstr(const char *h, const char *n) {
    size_t l = strlen(n);
    if (!l) return (char *)h;
    for (; *h; h++) if (*h == *n && !strncmp(h, n, l)) return (char *)h;
    return NULL;
}

/* ---------- Heap: einfacher Zuteiler mit Größe im Kopf; freigegebene Blöcke kommen in
 * Listen nach Größenklasse (2er-Potenzen) und werden wiederverwendet ---------- */
extern char __heap_start[];
static char *heap_pos, *heap_ende;
static void *frei[32];

void heap_init(char *ende) { heap_pos = __heap_start; heap_ende = ende; }
size_t heap_frei(void) { return (size_t)(heap_ende - heap_pos); }

static int klasse(size_t n) { int k = 4; while (((size_t)1 << k) < n) k++; return k; }

void *malloc(size_t n) {
    if (n == 0) n = 1;
    int k = klasse(n + 8);
    if (frei[k]) { void **b = frei[k]; frei[k] = *b; return b; }
    size_t g = (size_t)1 << k;
    if (heap_pos + g > heap_ende) return NULL;
    uint32_t *kopf = (uint32_t *)(void *)heap_pos;
    kopf[0] = (uint32_t)k; kopf[1] = 0x48454e49;   /* "HENI" */
    heap_pos += g;
    return kopf + 2;
}
void free(void *p) {
    if (!p) return;
    uint32_t *kopf = (uint32_t *)p - 2;
    if (kopf[1] != 0x48454e49) return;
    int k = (int)kopf[0];
    *(void **)p = frei[k]; frei[k] = p;
}
void *calloc(size_t n, size_t m) { void *p = malloc(n * m); if (p) memset(p, 0, n * m); return p; }
void *realloc(void *p, size_t n) {
    if (!p) return malloc(n);
    uint32_t *kopf = (uint32_t *)p - 2;
    size_t alt = ((size_t)1 << kopf[0]) - 8;
    if (n <= alt) return p;
    void *q = malloc(n);
    if (!q) return NULL;
    memcpy(q, p, alt);
    free(p);
    return q;
}

int abs(int x) { return x < 0 ? -x : x; }
long strtol(const char *s, char **ende, int basis) {
    long v = 0; int neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    if (basis == 0) basis = 10;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= basis) break;
        v = v * basis + d;
    }
    if (ende) *ende = (char *)s;
    return neg ? -v : v;
}
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }

/* Einfügesortierung, stabil (kleine Listen) */
void qsort(void *b, size_t n, size_t g, int (*vgl)(const void *, const void *)) {
    unsigned char *a = b, tmp[64];
    if (g > sizeof tmp) return;
    for (size_t i = 1; i < n; i++) {
        size_t j = i;
        memcpy(tmp, a + i * g, g);
        while (j > 0 && vgl(a + (j - 1) * g, tmp) > 0) { memcpy(a + j * g, a + (j - 1) * g, g); j--; }
        memcpy(a + j * g, tmp, g);
    }
}

/* ---------- 64-Bit-Hilfen für den 32-Bit-ABI ---------- */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rest) {
    uint64_t q = 0, r = 0;
    if (d == 0) { if (rest) *rest = 0; return 0; }
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (uint64_t)1 << i; }
    }
    if (rest) *rest = r;
    return q;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) { return __udivmoddi4(n, d, NULL); }
uint64_t __umoddi3(uint64_t n, uint64_t d) { uint64_t r; __udivmoddi4(n, d, &r); return r; }
int64_t __divdi3(int64_t n, int64_t d) {
    int neg = (n < 0) != (d < 0);
    uint64_t q = __udivmoddi4(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, NULL);
    return neg ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t n, int64_t d) {
    uint64_t r;
    __udivmoddi4(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
    return n < 0 ? -(int64_t)r : (int64_t)r;
}

/* ---------- printf ---------- */
typedef struct { char *p; size_t n, pos; } Aus;
static void aus(Aus *a, char c) { if (a->pos + 1 < a->n) a->p[a->pos] = c; a->pos++; }

int vsnprintf(char *p, size_t n, const char *f, va_list ap) {
    Aus a = { p, n, 0 };
    for (; *f; f++) {
        if (*f != '%') { aus(&a, *f); continue; }
        f++;
        int links = 0, null = 0, plus = 0, breite = 0, genau = -1, lang = 0;
        for (;; f++) {
            if (*f == '-') links = 1; else if (*f == '0') null = 1; else if (*f == '+') plus = 1; else break;
        }
        if (*f == '*') { breite = va_arg(ap, int); if (breite < 0) { links = 1; breite = -breite; } f++; }
        else while (*f >= '0' && *f <= '9') breite = breite * 10 + (*f++ - '0');
        if (*f == '.') {
            f++; genau = 0;
            if (*f == '*') { genau = va_arg(ap, int); f++; }
            else while (*f >= '0' && *f <= '9') genau = genau * 10 + (*f++ - '0');
        }
        while (*f == 'l' || *f == 'z' || *f == 'h') { if (*f == 'l') lang++; f++; }
        char puf[24]; const char *s = puf; int len = 0, neg = 0;
        switch (*f) {
        case 'd': case 'i': {
            int64_t v = lang >= 2 ? va_arg(ap, long long) : lang ? va_arg(ap, long) : va_arg(ap, int);
            uint64_t u = v < 0 ? -(uint64_t)v : (uint64_t)v; neg = v < 0;
            char *q = puf + sizeof puf; do { *--q = (char)('0' + (int)(u % 10)); u /= 10; } while (u);
            s = q; len = (int)(puf + sizeof puf - q);
            break;
        }
        case 'u': case 'x': case 'X': case 'p': {
            uint64_t u = *f == 'p' ? (uintptr_t)va_arg(ap, void *) :
                         lang >= 2 ? va_arg(ap, unsigned long long) : lang ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            unsigned basis = *f == 'u' ? 10 : 16;
            const char *z = *f == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char *q = puf + sizeof puf; do { *--q = z[u % basis]; u /= basis; } while (u);
            s = q; len = (int)(puf + sizeof puf - q);
            break;
        }
        case 'c': puf[0] = (char)va_arg(ap, int); len = 1; break;
        case 's': s = va_arg(ap, const char *); if (!s) s = "(null)";
            len = (int)strlen(s); if (genau >= 0 && len > genau) len = genau; genau = -1; break;
        case '%': puf[0] = '%'; len = 1; break;
        default: puf[0] = '?'; len = 1; break;
        }
        int ziffern = len;
        if (genau > ziffern) ziffern = genau;
        int gesamt = ziffern + (neg || plus ? 1 : 0);
        if (!links && !null) while (breite > gesamt) { aus(&a, ' '); breite--; }
        if (neg) aus(&a, '-'); else if (plus && (*f == 'd' || *f == 'i')) aus(&a, '+');
        if (!links && null) while (breite > gesamt) { aus(&a, '0'); breite--; }
        for (int i = len; i < ziffern; i++) aus(&a, '0');
        for (int i = 0; i < len; i++) aus(&a, s[i]);
        if (links) while (breite > gesamt) { aus(&a, ' '); breite--; }
    }
    if (n) p[a.pos < n ? a.pos : n - 1] = 0;
    return (int)a.pos;
}
int snprintf(char *p, size_t n, const char *f, ...) {
    va_list a; va_start(a, f); int r = vsnprintf(p, n, f, a); va_end(a); return r;
}
size_t strspn(const char *s, const char *a) {
    size_t n = 0;
    while (s[n] && strchr(a, s[n])) n++;
    return n;
}
