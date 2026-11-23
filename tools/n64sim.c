/* n64sim.c – kleiner Prüfstand für die ROM: führt das MIPS-ELF der Heini-App 64 Befehl für Befehl aus
 * (MIPS I/II wie VR4300 im 32-Bit-Betrieb, COP0 mit VI-Interrupt) und bildet nur das nach, was die App
 * benutzt: RDRAM, Boot-Flags im DMEM, VI (Bildanfang, Halbbild-Interrupt), MI, PI-DMA für SRAM,
 * SI-DMA mit PIF (Controller 1, keine Echtzeituhr). Ersetzt keinen Emulator und keine echte Konsole,
 * zeigt aber, ob der erzeugte Maschinencode startet, rechnet und ein Bild in den Bildpuffer schreibt.
 *
 *   cc -O2 -o build/n64sim tools/n64sim.c -lpng -lz
 *   build/n64sim build/heini-app-64.elf "warte:120 shot:a a warte:5 shot:b"   (Wörter je Halbbild wie beim PC-Test)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>

#define RAM_GROESSE (4u << 20)
static uint8_t ram[RAM_GROESSE], dmem[0x2000], pif[64], sram[32768];
static uint32_t r[32], hi, lo, pc, npc, cop0[32];
static uint32_t vi[16], mi_mask, mi_intr, pi[16], si[8];
static uint64_t befehle;
static int vi_feld, fehler, halte;
static uint32_t letzter_anfang; static uint64_t letzter_wechsel; static int wechsel; static uint64_t summe_dauer, max_dauer;
static uint16_t tasten;

static void halt(const char *w) { fprintf(stderr, "HALT bei pc=%08x: %s (nach %llu Befehlen)\n", pc, w, (unsigned long long)befehle); fehler = 1; }

static uint32_t phys(uint32_t v) {
    if (v >= 0x80000000u && v < 0xC0000000u) return v & 0x1FFFFFFF;
    halt("Adresse außerhalb KSEG0/1"); return 0;
}
static uint32_t lies32(uint32_t v) {
    uint32_t a = phys(v);
    if (a + 3 < RAM_GROESSE) return (uint32_t)ram[a] << 24 | ram[a + 1] << 16 | ram[a + 2] << 8 | ram[a + 3];
    if (a >= 0x04000000 && a < 0x04002000) { a -= 0x04000000; return (uint32_t)dmem[a] << 24 | dmem[a + 1] << 16 | dmem[a + 2] << 8 | dmem[a + 3]; }
    if (a >= 0x04400000 && a < 0x04400040) {
        int i = (a - 0x04400000) / 4;
        if (i == 4) return (uint32_t)(0x200 | (vi_feld & 1));
        return vi[i];
    }
    if (a == 0x0430000C) return mi_mask;
    if (a == 0x04300008) return mi_intr;
    if (a >= 0x04600000 && a < 0x04600040) { int i = (a - 0x04600000) / 4; return i == 4 ? 0 : pi[i]; }
    if (a >= 0x04800000 && a < 0x04800020) { int i = (a - 0x04800000) / 4; return i == 6 ? 0 : si[i]; }
    if (a >= 0x1FC007C0 && a < 0x1FC00800) { a -= 0x1FC007C0; return (uint32_t)pif[a] << 24 | pif[a + 1] << 16 | pif[a + 2] << 8 | pif[a + 3]; }
    if (a >= 0x04700000 && a < 0x04700020) return 0;
    char t[64]; snprintf(t, sizeof t, "Lesen von %08x", a); halt(t); return 0;
}
static void pif_ausfuehren(void) {
    int i = 0, kanal = 0;
    while (i < 63 && kanal < 6) {
        uint8_t tx = pif[i];
        if (tx == 0xFE) break;
        if (tx == 0xFF || tx == 0xFD) { i++; continue; }
        if (tx == 0x00) { kanal++; i++; continue; }
        uint8_t rx = pif[i + 1] & 0x3F;
        uint8_t *cmd = &pif[i + 2], *ant = &pif[i + 2 + (tx & 0x3F)];
        if (kanal == 0 && cmd[0] == 0x01 && rx == 4) {
            ant[0] = (uint8_t)(tasten >> 8); ant[1] = (uint8_t)tasten; ant[2] = 0; ant[3] = 0;
        } else pif[i + 1] |= 0x80;   /* kein Gerät */
        i += 2 + (tx & 0x3F) + rx;
        kanal++;
    }
}
static void schreib32(uint32_t v, uint32_t w) {
    uint32_t a = phys(v);
    if (a + 3 < RAM_GROESSE) { ram[a] = w >> 24; ram[a + 1] = w >> 16; ram[a + 2] = w >> 8; ram[a + 3] = w; return; }
    if (a >= 0x04000000 && a < 0x04002000) { a -= 0x04000000; dmem[a] = w >> 24; dmem[a + 1] = w >> 16; dmem[a + 2] = w >> 8; dmem[a + 3] = w; return; }
    if (a >= 0x04400000 && a < 0x04400040) { int i = (a - 0x04400000) / 4; if (i == 4) mi_intr &= ~0x08u; else vi[i] = w; return; }
    if (a == 0x0430000C) {
        for (int b = 0; b < 6; b++) { if (w & (1u << (2 * b))) mi_mask &= ~(1u << b); if (w & (2u << (2 * b))) mi_mask |= 1u << b; }
        return;
    }
    if (a >= 0x04600000 && a < 0x04600040) {
        int i = (a - 0x04600000) / 4;
        pi[i] = w;
        if (i == 2 || i == 3) {   /* 2: RDRAM → Modul, 3: Modul → RDRAM */
            uint32_t d = pi[0] & 0x00FFFFFF, c = pi[1], n = w + 1;
            if (c < 0x08000000 || c + n > 0x08000000 + sizeof sram) { halt("PI-DMA außerhalb SRAM"); return; }
            c -= 0x08000000;
            if (i == 2) memcpy(sram + c, ram + d, n); else memcpy(ram + d, sram + c, n);
        }
        return;
    }
    if (a >= 0x04800000 && a < 0x04800020) {
        int i = (a - 0x04800000) / 4;
        si[i] = w;
        if (i == 4) { memcpy(pif, ram + (si[0] & 0x00FFFFFF), 64); pif_ausfuehren(); }       /* PIF_ADDR_WR64B */
        else if (i == 1) memcpy(ram + (si[0] & 0x00FFFFFF), pif, 64);                         /* PIF_ADDR_RD64B */
        return;
    }
    if (a >= 0x1FC007C0 && a < 0x1FC00800) return;
    char t[64]; snprintf(t, sizeof t, "Schreiben nach %08x", a); halt(t);
}
static uint8_t lies8(uint32_t v) { uint32_t w = lies32(v & ~3u); return (uint8_t)(w >> (8 * (3 - (v & 3)))); }
static uint16_t lies16(uint32_t v) { uint32_t w = lies32(v & ~3u); return (uint16_t)(w >> (8 * (2 - (v & 2)))); }
static void schreib8(uint32_t v, uint8_t b) { uint32_t a = phys(v); if (a < RAM_GROESSE) { ram[a] = b; return; } uint32_t w = lies32(v & ~3u); int s = 8 * (3 - (v & 3)); schreib32(v & ~3u, (w & ~(0xFFu << s)) | ((uint32_t)b << s)); }
static void schreib16(uint32_t v, uint16_t h) { uint32_t a = phys(v); if (a + 1 < RAM_GROESSE) { ram[a] = h >> 8; ram[a + 1] = (uint8_t)h; return; } schreib8(v, h >> 8); schreib8(v + 1, (uint8_t)h); }

static void ausnahme(int code) {
    cop0[14] = pc; cop0[13] = (cop0[13] & ~0x7Cu) | (uint32_t)(code << 2);
    cop0[12] |= 2;   /* EXL */
    pc = 0x80000180; npc = pc + 4;
}

static void schritt(void) {
    /* Interrupt (nicht im Verzögerungsschlitz) */
    if ((mi_intr & mi_mask) && (cop0[12] & 1) && !(cop0[12] & 6) && (cop0[12] & 0x400) && npc == pc + 4) {
        cop0[13] |= 0x400;
        ausnahme(0);
    }
    uint32_t ins = lies32(pc), naechst = npc + 4;
    if (fehler) return;
    befehle++;
    int op = ins >> 26, rs = (ins >> 21) & 31, rt = (ins >> 16) & 31, rd = (ins >> 11) & 31, sa = (ins >> 6) & 31, fn = ins & 63;
    int32_t imm = (int16_t)ins; uint32_t uimm = ins & 0xFFFF;
    uint32_t ziel = pc + 4 + ((uint32_t)imm << 2);
    int likely_skip = 0;
    #define VERZW(c) do { if (c) naechst = ziel; } while (0)
    #define VERZWL(c) do { if (c) naechst = ziel; else likely_skip = 1; } while (0)
    switch (op) {
    case 0:
        switch (fn) {
        case 0: r[rd] = r[rt] << sa; break;
        case 2: r[rd] = r[rt] >> sa; break;
        case 3: r[rd] = (uint32_t)((int32_t)r[rt] >> sa); break;
        case 4: r[rd] = r[rt] << (r[rs] & 31); break;
        case 6: r[rd] = r[rt] >> (r[rs] & 31); break;
        case 7: r[rd] = (uint32_t)((int32_t)r[rt] >> (r[rs] & 31)); break;
        case 8: naechst = r[rs]; break;
        case 9: { uint32_t z = r[rs]; r[rd] = pc + 8; naechst = z; break; }
        case 0xC: halt("syscall"); return;
        case 0xD: halt("break"); return;
        case 0x10: r[rd] = hi; break;
        case 0x11: hi = r[rs]; break;
        case 0x12: r[rd] = lo; break;
        case 0x13: lo = r[rs]; break;
        case 0x18: { int64_t p = (int64_t)(int32_t)r[rs] * (int32_t)r[rt]; lo = (uint32_t)p; hi = (uint32_t)(p >> 32); break; }
        case 0x19: { uint64_t p = (uint64_t)r[rs] * r[rt]; lo = (uint32_t)p; hi = (uint32_t)(p >> 32); break; }
        case 0x1A: if (r[rt]) { lo = (uint32_t)((int32_t)r[rs] / (int32_t)r[rt]); hi = (uint32_t)((int32_t)r[rs] % (int32_t)r[rt]); } break;
        case 0x1B: if (r[rt]) { lo = r[rs] / r[rt]; hi = r[rs] % r[rt]; } break;
        case 0x20: case 0x21: r[rd] = r[rs] + r[rt]; break;
        case 0x22: case 0x23: r[rd] = r[rs] - r[rt]; break;
        case 0x24: r[rd] = r[rs] & r[rt]; break;
        case 0x25: r[rd] = r[rs] | r[rt]; break;
        case 0x26: r[rd] = r[rs] ^ r[rt]; break;
        case 0x27: r[rd] = ~(r[rs] | r[rt]); break;
        case 0x2A: r[rd] = (int32_t)r[rs] < (int32_t)r[rt]; break;
        case 0x2B: r[rd] = r[rs] < r[rt]; break;
        case 0x34: if (r[rs] == r[rt]) { halt("teq (Division durch 0?)"); return; } break;
        default: { char t[48]; snprintf(t, sizeof t, "unbekannt SPECIAL %02x", fn); halt(t); return; }
        }
        break;
    case 1:
        switch (rt) {
        case 0: VERZW((int32_t)r[rs] < 0); break;
        case 1: VERZW((int32_t)r[rs] >= 0); break;
        case 2: VERZWL((int32_t)r[rs] < 0); break;
        case 3: VERZWL((int32_t)r[rs] >= 0); break;
        case 0x10: { int c = (int32_t)r[rs] < 0; r[31] = pc + 8; VERZW(c); break; }
        case 0x11: { int c = (int32_t)r[rs] >= 0; r[31] = pc + 8; VERZW(c); break; }
        default: halt("unbekannt REGIMM"); return;
        }
        break;
    case 2: naechst = (pc & 0xF0000000) | ((ins & 0x03FFFFFF) << 2); break;
    case 3: r[31] = pc + 8; naechst = (pc & 0xF0000000) | ((ins & 0x03FFFFFF) << 2); break;
    case 4: VERZW(r[rs] == r[rt]); break;
    case 5: VERZW(r[rs] != r[rt]); break;
    case 6: VERZW((int32_t)r[rs] <= 0); break;
    case 7: VERZW((int32_t)r[rs] > 0); break;
    case 8: case 9: r[rt] = r[rs] + (uint32_t)imm; break;
    case 0xA: r[rt] = (int32_t)r[rs] < imm; break;
    case 0xB: r[rt] = r[rs] < (uint32_t)imm; break;
    case 0xC: r[rt] = r[rs] & uimm; break;
    case 0xD: r[rt] = r[rs] | uimm; break;
    case 0xE: r[rt] = r[rs] ^ uimm; break;
    case 0xF: r[rt] = uimm << 16; break;
    case 0x10:
        if (rs == 0) r[rt] = rd == 9 ? (uint32_t)(befehle / 2) : cop0[rd];
        else if (rs == 4) { if (rd != 9) cop0[rd] = r[rt]; }
        else if (rs == 0x10 && fn == 0x18) { cop0[12] &= ~2u; pc = cop0[14]; npc = pc + 4; r[0] = 0; return; }
        else { halt("unbekannt COP0"); return; }
        break;
    case 0x14: VERZWL(r[rs] == r[rt]); break;
    case 0x15: VERZWL(r[rs] != r[rt]); break;
    case 0x16: VERZWL((int32_t)r[rs] <= 0); break;
    case 0x17: VERZWL((int32_t)r[rs] > 0); break;
    case 0x20: r[rt] = (uint32_t)(int8_t)lies8(r[rs] + imm); break;
    case 0x21: { uint32_t a = r[rs] + imm; if (a & 1) { halt("lh unausgerichtet"); return; } r[rt] = (uint32_t)(int16_t)lies16(a); break; }
    case 0x22: { uint32_t a = r[rs] + imm; int s = 8 * (a & 3); uint32_t w = lies32(a & ~3u); r[rt] = s ? (r[rt] & ((1u << s) - 1)) | (w << s) : w; break; }
    case 0x23: { uint32_t a = r[rs] + imm; if (a & 3) { halt("lw unausgerichtet"); return; } r[rt] = lies32(a); break; }
    case 0x24: r[rt] = lies8(r[rs] + imm); break;
    case 0x25: { uint32_t a = r[rs] + imm; if (a & 1) { halt("lhu unausgerichtet"); return; } r[rt] = lies16(a); break; }
    case 0x26: { uint32_t a = r[rs] + imm; int s = 8 * (3 - (a & 3)); uint32_t w = lies32(a & ~3u); r[rt] = s ? (r[rt] & ~(0xFFFFFFFFu >> s)) | (w >> s) : w; break; }
    case 0x28: schreib8(r[rs] + imm, (uint8_t)r[rt]); break;
    case 0x29: { uint32_t a = r[rs] + imm; if (a & 1) { halt("sh unausgerichtet"); return; } schreib16(a, (uint16_t)r[rt]); break; }
    case 0x2A: { uint32_t a = r[rs] + imm; int s = 8 * (a & 3); uint32_t w = lies32(a & ~3u); w = s ? (w & ~(0xFFFFFFFFu >> s)) | (r[rt] >> s) : r[rt]; schreib32(a & ~3u, w); break; }
    case 0x2B: { uint32_t a = r[rs] + imm; if (a & 3) { halt("sw unausgerichtet"); return; } schreib32(a, r[rt]); break; }
    case 0x2E: { uint32_t a = r[rs] + imm; int k = a & 3; uint32_t w = lies32(a & ~3u); w = k == 3 ? r[rt] : (w & (0xFFFFFFFFu >> (8 * (k + 1)))) | (r[rt] << (8 * (3 - k))); schreib32(a & ~3u, w); break; }
    case 0x2F: break;   /* cache */
    default: { char t[48]; snprintf(t, sizeof t, "unbekannter Befehl %02x", op); halt(t); return; }
    }
    r[0] = 0;
    if (likely_skip) { pc = npc + 4; npc = pc + 4; }
    else { pc = npc; npc = naechst; }
}

static void png(const char *name) {
    char pfad[256]; snprintf(pfad, sizeof pfad, "build/sim/%s.png", name);
    uint32_t anfang = vi[1] & 0x00FFFFFF, breite = vi[2];
    if (!(vi_feld & 1) == 0) {}   /* vi[1] zeigt je Halbbild auf eine andere Zeile */
    if (anfang % (breite * 2) != 0) anfang -= breite * 2;
    FILE *f = fopen(pfad, "wb"); if (!f) { perror(pfad); return; }
    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(p);
    png_init_io(p, f);
    png_set_IHDR(p, info, (png_uint_32)breite, 480, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(p, info);
    uint8_t zeile[2048 * 3];
    for (uint32_t y = 0; y < 480; y++) {
        for (uint32_t x = 0; x < breite; x++) {
            uint32_t a = anfang + (y * breite + x) * 2;
            uint16_t c = (uint16_t)(ram[a] << 8 | ram[a + 1]);
            int rr = c >> 11, g = (c >> 6) & 31, b = (c >> 1) & 31;
            zeile[x * 3] = (uint8_t)(rr << 3 | rr >> 2); zeile[x * 3 + 1] = (uint8_t)(g << 3 | g >> 2); zeile[x * 3 + 2] = (uint8_t)(b << 3 | b >> 2);
        }
        png_write_row(p, zeile);
    }
    png_write_end(p, NULL); png_destroy_write_struct(&p, &info); fclose(f);
    printf("Bild: %s (VI_ORIGIN %06x, Breite %u, VI_CTRL %08x, Y_SCALE %08x)\n", pfad, vi[1] & 0xFFFFFF, breite, vi[0], vi[13]);
}

static uint32_t mess_adr; static uint64_t mess_summe; static int mess_n; static uint32_t mess_alt, mess_max;
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Aufruf: n64sim <elf> [skript]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
    static uint8_t elf[8 << 20]; size_t n = fread(elf, 1, sizeof elf, f); fclose(f);
    #define B32(o) ((uint32_t)elf[o] << 24 | elf[(o) + 1] << 16 | elf[(o) + 2] << 8 | elf[(o) + 3])
    #define B16(o) ((uint32_t)elf[o] << 8 | elf[(o) + 1])
    if (n < 52 || memcmp(elf, "\x7f" "ELF", 4)) { fprintf(stderr, "kein ELF\n"); return 2; }
    uint32_t entry = B32(24), phoff = B32(28); int phnum = (int)B16(44), phsz = (int)B16(42);
    for (int i = 0; i < phnum; i++) {
        uint32_t o = phoff + (uint32_t)i * (uint32_t)phsz;
        if (B32(o) != 1) continue;
        uint32_t off = B32(o + 4), va = B32(o + 8), fsz = B32(o + 16);
        memcpy(ram + (va & 0x1FFFFFFF), elf + off, fsz);
        printf("Segment %08x: %u Byte\n", va, fsz);
    }
    /* Zustand wie nach libdragons IPL3 */
    dmem[0] = 0x00; dmem[1] = 0x40; dmem[2] = 0; dmem[3] = 0;   /* 4 MB */
    dmem[9] = 0;   /* PAL */
    r[29] = 0x80400000; pc = entry; npc = pc + 4;
    cop0[12] = 0x34000000;
    /* Skript */
    char *wort[4096]; int nw = 0;
    if (argc > 3) mess_adr = (uint32_t)strtoul(argv[3], NULL, 16);
    if (argc > 2) { char *s = strdup(argv[2]); for (char *w = strtok(s, " "); w && nw < 4096; w = strtok(NULL, " ")) wort[nw++] = w; }
    int pos = 0, warte = 0;
    const uint64_t BEFEHLE_JE_HALBBILD = 93750000ull / 50;
    uint64_t naechstes = BEFEHLE_JE_HALBBILD;
    for (;;) {
        schritt();
        if (fehler) return 1;
        if (befehle >= naechstes) {
            naechstes += BEFEHLE_JE_HALBBILD;
            vi_feld++;
            mi_intr |= 0x08;   /* VI */
            uint32_t anf = vi[1] & ~0xFFFu;
            if (anf != letzter_anfang) { if (letzter_wechsel) { uint64_t d = befehle - letzter_wechsel; summe_dauer += d; if (d > max_dauer) max_dauer = d; wechsel++; } letzter_anfang = anf; letzter_wechsel = befehle; }
            if (mess_adr) { uint32_t m = lies32(mess_adr); if (m != mess_alt && m) { mess_summe += m; mess_n++; if (m > mess_max) mess_max = m; mess_alt = m; } }
            if (halte > 0) { halte--; continue; }
            tasten = 0;
            if (warte > 0) { warte--; continue; }
            while (pos < nw) {
                char *w = wort[pos++];
                if (!strncmp(w, "shot:", 5)) { png(w + 5); continue; }
                if (!strncmp(w, "warte:", 6)) { warte = atoi(w + 6) - 1; break; }
                static const struct { const char *n; uint16_t t; } K[] = { {"a", 0x8000}, {"b", 0x4000}, {"z", 0x2000}, {"start", 0x1000}, {"hoch", 0x0800},
                    {"runter", 0x0400}, {"links", 0x0200}, {"rechts", 0x0100}, {"L", 0x0020}, {"R", 0x0010}, {"ch", 0x0008}, {"cu", 0x0004}, {"cl", 0x0002}, {"cr", 0x0001}, {"-", 0} };
                for (unsigned k = 0; k < sizeof K / sizeof K[0]; k++) if (!strcmp(w, K[k].n)) tasten = K[k].t;
                halte = 5;   /* wie ein Mensch: Taste etwa 120 ms gedrückt */
                break;
            }
            if (pos >= nw && warte <= 0) { if (mess_n) printf("Zeichnen: %d Bilder gemessen, im Schnitt %.1f ms, längstens %.1f ms (VR4300 mit 93,75 MHz, ohne Speicherwartezeiten)\n", mess_n, mess_summe / (double)mess_n / 46875.0, mess_max / 46875.0);
                printf("Skript fertig nach %llu Befehlen (%d Halbbilder), %d Bildwechsel, im Schnitt %.1f ms, längstens %.1f ms je Bild (bei 1 Befehl je Takt)\n", (unsigned long long)befehle, vi_feld, wechsel, wechsel ? summe_dauer / (double)wechsel / 93750.0 : 0, max_dauer / 93750.0); return 0; }
        }
    }
}
