#!/usr/bin/env node
/* export2c.js – macht aus dem JSON-Export der Heini-App die C-Grunddaten (src/gen/data.c).
 *
 *   node tools/export2c.js <heini-app-export.json> <pfad/zu/heini-app/public> <ausgabe.c>
 *
 * Aus dem Original-Programmcode (public/logik.js und public/rules.js) werden die festen Listen
 * übernommen: Profilfarben, Konsolen, Achievements, Kassen-Arten, Klo-Weisheiten, Sprüche.
 * Gelöschte Datensätze (_deleted = 1) bleiben draußen. Zeiten werden nach Europe/Berlin umgerechnet.
 */
'use strict';
const fs = require('fs');
const path = require('path');

const [, , exportPfad, publicPfad, ausgabePfad] = process.argv;
if (!exportPfad || !publicPfad || !ausgabePfad) {
  console.error('Aufruf: node tools/export2c.js <export.json> <heini-app/public> <data.c>');
  process.exit(1);
}
const D = JSON.parse(fs.readFileSync(exportPfad, 'utf8'));
const L = require(path.resolve(publicPfad, 'logik.js'));
const R = require(path.resolve(publicPfad, 'rules.js'));

// ---------- Text: UTF-8 bleibt UTF-8; Emojis bleiben Emojis (die ROM zeichnet sie mit Noto Color Emoji) ----------
const ERSATZ = { ' ': ' ', ' ': ' ', ' ': ' ' };
const EMOJI_SET = new Set();
const istEmoji = cp => cp >= 0x1f000 || (cp >= 0x2190 && cp <= 0x2bff) || cp === 0x00a9 || cp === 0x00ae || (cp >= 0x3030 && cp <= 0x303d);
function kodiere(s) {
  if (s === null || s === undefined) return '';
  const out = [];
  const z = Array.from(String(s));
  for (let i = 0; i < z.length; i++) {
    const ch = z[i], cp = ch.codePointAt(0);
    if (cp === 0xfe0f || cp === 0xfe0e || (cp >= 0x1f3fb && cp <= 0x1f3ff) || cp === 0x20e3) continue;   // Varianten, Hautfarben
    if (cp === 0x200d) { i++; continue; }                                                               // ZWJ-Folgen: nur das erste Emoji
    if (cp >= 0x1f1e6 && cp <= 0x1f1ff) { out.push(String.fromCharCode(65 + cp - 0x1f1e6)); continue; } // Flaggen → Buchstaben
    if (ERSATZ[ch] !== undefined) { out.push(ERSATZ[ch]); continue; }
    if (cp === 10) { out.push('\n'); continue; }
    if (cp < 32 || (cp >= 0x80 && cp < 0xa0)) continue;   // 0x80–0x9F sind Symbole der ROM
    if (istEmoji(cp)) EMOJI_SET.add(cp);
    out.push(ch);
  }
  return out.join('').replace(/ {2,}/g, ' ');
}
function cstr(s) {
  if (s === null || s === undefined) return 'NULL';
  // Ausgabe als UTF-8; Symbole liegen auf U+0080..U+009F (in UTF-8: C2 80..C2 9F)
  const bytes = Buffer.from(kodiere(s), 'utf8');
  let o = '"';
  for (const c of bytes) {
    const ch = String.fromCharCode(c);
    if (ch === '"') o += '\\"';
    else if (ch === '\\') o += '\\\\';
    else if (ch === '\n') o += '\\n';
    else if (ch === '?') o += '\\?';   // keine Trigraphen
    else if (c < 32 || c >= 127) o += '\\' + c.toString(8).padStart(3, '0');
    else o += ch;
  }
  return o + '"';
}

// ---------- Zeit ----------
const BASIS = Date.UTC(2020, 0, 1);
const fmt = new Intl.DateTimeFormat('en-CA', { timeZone: 'Europe/Berlin', year: 'numeric', month: '2-digit', day: '2-digit',
  hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' });
function ortSek(ms) {
  const t = {};
  for (const p of fmt.formatToParts(new Date(ms))) t[p.type] = p.value;
  return Math.round((Date.UTC(+t.year, +t.month - 1, +t.day, +t.hour, +t.minute, +t.second) - BASIS) / 1000);
}
const zeit = iso => (iso ? ortSek(Date.parse(iso)) : 0);
const tag = d => (d && /^\d{4}-\d{2}-\d{2}/.test(d) ? Math.round((Date.UTC(+d.slice(0, 4), +d.slice(5, 7) - 1, +d.slice(8, 10)) - BASIS) / 86400000) : 0);
const minuten = hm => (hm && /^\d{1,2}:\d{2}/.test(hm) ? (+hm.split(':')[0]) * 60 + (+hm.split(':')[1]) : -1);
const idZeit = id => { const ms = L.idZeit(id); return ms ? ortSek(ms) : 0; };

// ---------- Tabellen und Verweise ----------
const lebend = t => (D[t] || []).filter(r => !r._deleted);
const indexVon = {};
function tabelle(name, liste) { indexVon[name] = new Map(liste.map((r, i) => [r.id, i])); return liste; }
const ix = (name, id) => (id && indexVon[name].has(id) ? indexVon[name].get(id) : -1);
const json = (v, ersatz) => { try { const x = JSON.parse(v); return x === null ? ersatz : x; } catch (e) { return ersatz; } };

const profile = tabelle('profile', lebend('profile'));
if (profile.length > 32) throw new Error('mehr als 32 Profile');
const treffen = tabelle('treffen', lebend('treffen'));
const raeume = tabelle('raeume', lebend('raeume').filter(r => r.art === 'bad' || r.art === 'schlaf'));
const plaetze = tabelle('plaetze', lebend('plaetze').filter(p => ix('raeume', p.raum_id) >= 0));
const utensilien = tabelle('utensilien', lebend('utensilien'));
const konsolen = tabelle('konsolen', lebend('konsolen').filter(k => ix('profile', k.profil_id) >= 0));
const dinge = lebend('konsolen_dinge').filter(d => ix('konsolen', d.konsole_id) >= 0);
const wichtel = lebend('wichtel').filter(w => ix('profile', w.profil_id) >= 0);
const zusagen = lebend('zusagen').filter(z => ix('treffen', z.treffen_id) >= 0 && ix('profile', z.profil_id) >= 0);
const anwesenheit = lebend('anwesenheit').filter(a => ix('treffen', a.treffen_id) >= 0 && ix('profile', a.profil_id) >= 0);
const buchungen = lebend('buchungen').filter(b => ix('raeume', b.raum_id) >= 0 && ix('profile', b.profil_id) >= 0);
const warteliste = lebend('warteliste');
const alarme = lebend('alarme').filter(a => ix('profile', a.profil_id) >= 0);
const schlaf = lebend('schlafbuchungen').filter(s => ix('plaetze', s.platz_id) >= 0 && ix('profile', s.profil_id) >= 0);
const ausleihen = lebend('ausleihen').filter(a => ix('utensilien', a.utensil_id) >= 0 && ix('profile', a.profil_id) >= 0);
const nachrichten = tabelle('nachrichten', lebend('nachrichten').sort((a, b) => (L.idZeit(a.id) || 0) - (L.idZeit(b.id) || 0)));
const umfragen = tabelle('umfragen', lebend('umfragen').filter(u => ix('nachrichten', u.nachricht_id) >= 0));
const stimmen = lebend('stimmen').filter(s => ix('umfragen', s.umfrage_id) >= 0 && ix('profile', s.profil_id) >= 0);
const EMOJIS = R.WERTE.emojis;
const reaktionen = lebend('reaktionen').filter(r => ix('nachrichten', r.nachricht_id) >= 0 && ix('profile', r.profil_id) >= 0);
const ACHS = L.ACHIEVEMENTS;
const erfolge = lebend('achievements').filter(a => ix('profile', a.profil_id) >= 0 && ACHS.some(x => x.code === a.code));
const paare = lebend('paare').filter(p => ix('profile', p.von_id) >= 0 && ix('profile', p.zu_id) >= 0);
const trips = tabelle('trips', lebend('trips').filter(t => ix('treffen', t.treffen_id) >= 0));
const ausgaben = tabelle('ausgaben', lebend('ausgaben').filter(a => ix('treffen', a.treffen_id) >= 0 && ix('profile', a.profil_id) >= 0));
const zahlungen = lebend('zahlungen').filter(z => ix('profile', z.von_id) >= 0 && ix('profile', z.an_id) >= 0);
const medien = tabelle('medien', lebend('medien'));
const daumen = lebend('medien_daumen').filter(d => ix('medien', d.medien_id) >= 0 && ix('profile', d.profil_id) >= 0);
const agenda = lebend('treffen_agenda').filter(a => ix('treffen', a.treffen_id) >= 0);
const kommentare = lebend('treffen_kommentare').filter(k => ix('treffen', k.treffen_id) >= 0);
const fahrten = tabelle('fahrten', lebend('fahrten').filter(f => ix('profile', f.profil_id) >= 0));
const mitfahrer = lebend('mitfahrer').filter(m => ix('fahrten', m.fahrt_id) >= 0 && ix('profile', m.profil_id) >= 0);
const frueher = lebend('fruehere_treffen').sort((a, b) => a.nr - b.nr);

// Heini-Namen: Liste aus heini_namen + Profilnamen + allen Namen aus „Frühere Treffen“ (höchstens 32, als Bitmaske)
const namen = [];
const nameDazu = n => { if (n && !namen.includes(n)) namen.push(n); };
lebend('heini_namen').forEach(h => nameDazu(h.name));
profile.forEach(p => nameDazu(p.name));
const gastgeberNamen = f => String(f.gastgeber || '').split(' & ').map(x => x.trim()).filter(Boolean);
frueher.forEach(f => { json(f.teilnehmer, []).forEach(nameDazu); gastgeberNamen(f).forEach(nameDazu); });
if (namen.length > 32) throw new Error('mehr als 32 Heini-Namen: ' + namen.length);

// Bilder je Nachricht (Fotos, GIFs, Sprachnachrichten haben im Export keine Bilddaten)
const medienJe = new Map();
for (const b of lebend('bilder')) {
  if (b.bezug !== 'nachricht' && b.bezug !== 'nachrichten') continue;
  const m = medienJe.get(b.bezug_id) || { n: 0, art: 1 };
  m.n++;
  if (String(b.mime).startsWith('audio/')) m.art = 3; else if (b.mime === 'image/gif' || b.original_mime === 'image/gif') m.art = Math.max(m.art, 2);
  medienJe.set(b.bezug_id, m);
}

// ---------- Ausgabe ----------
const zeilen = [];
const out = s => zeilen.push(s);
const farbe16 = hex => {
  const h = hex.replace('#', ''), r = parseInt(h.slice(0, 2), 16), g = parseInt(h.slice(2, 4), 16), b = parseInt(h.slice(4, 6), 16);
  return '0x' + (((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1).toString(16).padStart(4, '0');
};
const istHell = hex => { const h = hex.replace('#', ''); const r = parseInt(h.slice(0, 2), 16), g = parseInt(h.slice(2, 4), 16), b = parseInt(h.slice(4, 6), 16); return (0.299 * r + 0.587 * g + 0.114 * b) > 150 ? 1 : 0; };

out('/* Erzeugt von tools/export2c.js aus ' + path.basename(exportPfad) + ' – nicht von Hand ändern.');
out(' * App-Version ' + D.app_version + ', Schema ' + D.schema_version + ', Datenstand ' + D.data_version + ', exportiert ' + D.exportiert_am + ' */');
out('#include <stdlib.h>');
out('#include <string.h>');
out('#include "../model.h"');
out('#include "../assets.h"');
out('');

// Feste Listen
const FARB_HEX = { tuerkis: '#004c53', blau: '#144278', violett: '#602a5c', rosa: '#6d2245', rot: '#7b1619', orange: '#6b2e00', gelb: '#f7ca67',
  gruen: '#1e4e22', nachtblau: '#334263', koralle: '#72222e', mint: '#95e4c8', himmelblau: '#9fdafd', indigo: '#39387c', lavendel: '#d9c6fe',
  magenta: '#71115b', weinrot: '#692b37', braun: '#583b26', oliv: '#414618', limette: '#bee080', graphit: '#3e4348' };
const FARBEN = R.WERTE.farben;
out('const Farbe FARBEN[] = {');
FARBEN.forEach(f => out(`  {${cstr(f)}, ${cstr(R.WERTE.farbNamen[f])}, ${farbe16(FARB_HEX[f])}, ${istHell(FARB_HEX[f])}},`));
out('};');
out('const int FARBEN_N = ' + FARBEN.length + ';');
out('const KonsolenArt KONSOLEN[] = {');
R.KONSOLEN.forEach(k => out(`  {${cstr(k[0])}, ${cstr(k[1])}, ${cstr(k[2])}, ${farbe16(k[4])}},`));
out('};');
out('const int KONSOLEN_N = ' + R.KONSOLEN.length + ';');
out('const ErfolgArt ERFOLGE[] = {');
ACHS.forEach(a => out(`  {${cstr(a.code)}, ${cstr(a.name)}, ${cstr(a.hinweis)}, ${cstr(a.emoji)}, '${a.art}', ${a.geheim ? 1 : 0}},`));
out('};');
out('const int ERFOLGE_N = ' + ACHS.length + ';');
const ARTEN = R.WERTE.ausgabeArten;
out('const AusgabeArt AUSGABE_ARTEN[] = {');
const icId = n => 'IC_' + String(n || 'package').replace(/[^A-Za-z0-9]/g, '_').toUpperCase();
ARTEN.forEach(a => { const x = L.AUSGABE_ART[a] || {}; out(`  {${cstr(a)}, ${cstr(x.text || a)}, ${cstr(x.satz || '')}, ${cstr(x.emoji || '')}, ${icId(x.icon)}},`); });
out('};');
out('const int AUSGABE_ARTEN_N = ' + ARTEN.length + ';');
const liste = (name, arr) => { out(`const char *const ${name}[] = {`); arr.forEach(s => out('  ' + cstr(s) + ',')); out('};'); out(`const int ${name}_N = ${arr.length};`); };
liste('WEISHEITEN', L.WEISHEITEN);
liste('SPRUECHE_UEBERZOGEN', L.SPRUECHE_UEBERZOGEN);
liste('HEINAI_SPRUECHE', L.HEINAI_SPRUECHE || []);
liste('HEINI_NAMEN', namen);
out('const char *const EMOJIS[] = {' + EMOJIS.map(cstr).join(', ') + '};');
out('const int EMOJIS_N = ' + EMOJIS.length + ';');
out('const char *const MEDIEN_ARTEN[] = {"YouTube", "Spotify", "Link", "Idee", "Bucketliste"};');
const ein = k => { const e = lebend('einstellungen').find(x => x.schluessel === k); return e ? e.wert : null; };
const regel = (k, start) => { const w = ein(k); return w === null || w === '' ? start : parseInt(w, 10); };
out('const int REGEL_DAUER_BAD = ' + regel('dauer_bad', 15) + ';');
out('const int REGEL_VERSCHLEISS = ' + regel('regel_verschleiss', 1) + ';');
out('const int REGEL_VERSCHLEISS_CT = ' + regel('verschleiss_ct', 5) + ';');
out('const char *const ONEDRIVE_LINK = ' + cstr(ein('onedrive_link') || '') + ';');
out('const char *const ESHOP_LINK = ' + cstr(ein('eshop_link') || '') + ';');
const ez = zeit(D.exportiert_am);
const ezD = new Date(BASIS + ez * 1000);
const p2 = n => String(n).padStart(2, '0');
out('const char *const EXPORT_STAND = "' + `${p2(ezD.getUTCDate())}.${p2(ezD.getUTCMonth() + 1)}.${ezD.getUTCFullYear()} ${p2(ezD.getUTCHours())}:${p2(ezD.getUTCMinutes())}` + '";');
out('const uint32_t EXPORT_KENNUNG = ' + (D.data_version >>> 0) + 'u;');
out('const zeit_t EXPORT_ZEIT = ' + ez + ';');
out('');

// Datensätze
const tabellen = [];
function emit(typ, feld, arr, f) {
  tabellen.push([feld, arr.length]);
  if (!arr.length) return;
  out(`static const ${typ} G_${feld}[] = {`);
  arr.forEach((r, i) => out('  {' + f(r, i) + '},'));
  out('};');
}
const ja = v => (v ? 1 : 0);
const listeText = v => json(v, []).filter(x => typeof x === 'string').join(' · ');

emit('Profil', 'profile', profile, p => [
  `.name=${cstr(p.name)}`, `.status=${cstr(p.status || '')}`, `.wohnort=${cstr(p.wohnort || '')}`, `.strasse=${cstr(p.strasse || '')}`,
  `.plz=${cstr(p.plz || '')}`, `.auto_name=${cstr(p.auto_name || '')}`, `.paypal=${cstr(p.paypal || '')}`, `.park_hinweis=${cstr(p.park_hinweis || '')}`,
  `.unvertraeglich=${cstr(listeText(p.unvertraeglich))}`, `.mag_nicht=${cstr(listeText(p.mag_nicht))}`,
  `.farbe=${Math.max(0, FARBEN.indexOf(p.farbe))}`, `.bild=${parseInt(p.bild, 10) || 0}`, `.gastgeber=${ja(p.gastgeber)}`, `.aktiv=${ja(p.aktiv)}`,
  `.heini=${ja(p.heini)}`, `.auto_=${ja(p.auto)}`, `.elektro=${ja(p.elektro)}`, `.auto_plaetze=${p.auto_plaetze || 0}`, `.kein_alkohol=${ja(p.kein_alkohol)}`,
  `.parkplaetze=${p.parkplaetze || 0}`, `.wunsch_temp=${p.wunsch_temp || 0}`, `.verbrauch=${p.verbrauch || 0}`].join(', '));
emit('Raum', 'raeume', raeume, r => `${ix('profile', r.profil_id)}, ${ix('treffen', r.treffen_id)}, ${cstr(r.name)}, ${cstr(r.info || '')}, ${r.art === 'bad' ? 0 : 1}, ${r.reihenfolge || 0}`);
emit('Platz', 'plaetze', plaetze, p => `${ix('raeume', p.raum_id)}, ${cstr(p.name)}, ${ja(p.fuer_zwei)}`);
emit('Utensil', 'utensilien', utensilien, u => `${ix('profile', u.profil_id)}, ${cstr(u.name)}, ${ja(u.zaehlbar)}, ${u.anzahl || 1}`);
emit('Konsole', 'konsolen', konsolen, k => `${ix('profile', k.profil_id)}, ${R.KONSOLEN.findIndex(x => x[0] === k.art)}, ${cstr(k.name || '')}, ${k.pos || 0}`);
emit('KonsolenDing', 'dinge', dinge, d => `${ix('konsolen', d.konsole_id)}, ${d.art === 'spiel' ? 0 : 1}, ${cstr(d.name)}, ${d.anzahl || 1}`);
emit('Wichtel', 'wichtel', wichtel, w => `${ix('profile', w.profil_id)}, ${ix('profile', w.von_id)}, ${w.jahr}, ${cstr(w.geschenk)}`);
emit('Treffen', 'treffen', treffen, t => [cstr(t.titel || ''), cstr(t.adresse || ''), cstr(t.info || ''), ix('profile', t.gastgeber_id), ix('profile', t.gastgeber2_id),
  ix('profile', t.ersteller_id), tag(t.von), tag(t.bis), zeit(t.beendet_um), zeit(t.kasse_zu_um), ja(t.ausserhalb), 0].join(', '));
emit('Zusage', 'zusagen', zusagen, z => `${ix('treffen', z.treffen_id)}, ${ix('profile', z.profil_id)}, ${Math.max(0, R.WERTE.zusageStatus.indexOf(z.status))}`);
emit('Anwesenheit', 'anwesenheit', anwesenheit, a => `${ix('treffen', a.treffen_id)}, ${ix('profile', a.profil_id)}, ${zeit(a.angekommen_um)}, ${zeit(a.abgereist_um)}`);
emit('Buchung', 'buchungen', buchungen, b => {
  const start = tag(b.datum) * 86400 + Math.max(0, minuten(b.beginn)) * 60;
  return `${ix('treffen', b.treffen_id)}, ${ix('raeume', b.raum_id)}, ${ix('profile', b.profil_id)}, ${ix('profile', b.partner_id)}, ${start}, ${zeit(b.fertig_um)}, ${b.dauer || 0}, 0, ${ja(b.offen)}, ${b.dauer_hinweis === 'lang' ? 1 : 0}, ${zeit(b.partner_seit)}, ${zeit(b.partner_bis)}`;
});
emit('Warte', 'warteliste', warteliste.filter(w => ix('raeume', w.raum_id) >= 0), w => `${ix('raeume', w.raum_id)}, -1, ${ix('profile', w.profil_id)}, ${ix('profile', w.partner_id)}, ${zeit(w.seit)}, 0`);
emit('Alarm', 'alarme', alarme, a => `${ix('treffen', a.treffen_id)}, ${ix('raeume', a.raum_id)}, ${ix('profile', a.profil_id)}, ${ix('profile', a.erledigt_von)}, ${zeit(a.ausgeloest_um)}, ${zeit(a.erledigt_um)}, 0`);
emit('Schlaf', 'schlaf', schlaf, s => `${ix('treffen', s.treffen_id)}, ${ix('plaetze', s.platz_id)}, ${ix('profile', s.profil_id)}, ${tag(s.nacht)}, 0`);
emit('Ausleihe', 'ausleihen', ausleihen, a => `${ix('treffen', a.treffen_id)}, ${ix('utensilien', a.utensil_id)}, ${ix('profile', a.profil_id)}, ${a.anzahl || 1}`);
emit('Nachricht', 'nachrichten', nachrichten, n => {
  const m = medienJe.get(n.id) || { n: 0, art: 0 };
  const art = Math.max(0, R.WERTE.nachrichtArten.indexOf(n.art));
  return `${ix('profile', n.profil_id)}, ${art}, ${ja(n.angeheftet)}, ${m.art}, ${m.n}, 0, ${ix('nachrichten', n.antwort_auf)}, ${idZeit(n.id) || zeit(n._updated_at)}, ${cstr(n.text || '')}`;
});
emit('Umfrage', 'umfragen', umfragen, u => {
  const a = json(u.antworten, []).slice(0, 6);
  return `${ix('nachrichten', u.nachricht_id)}, ${a.length}, ${ja(u.mehrfach)}, ${zeit(u.beendet_um)}, ${zeit(u.endet_um)}, {${a.map(cstr).join(', ')}}`;
});
emit('Stimme', 'stimmen', stimmen, s => `${ix('umfragen', s.umfrage_id)}, ${ix('profile', s.profil_id)}, ${json(s.wahl, []).reduce((m, i) => m | (1 << i), 0)}`);
emit('Reaktion', 'reaktionen', reaktionen, r => {
  let e = EMOJIS.findIndex(x => kodiere(x) === kodiere(r.emoji));
  if (e < 0) { const k = kodiere(r.emoji); if (k) EMOJI_SET.add(k.codePointAt(0)); }
  return `${ix('nachrichten', r.nachricht_id)}, ${ix('profile', r.profil_id)}, ${e < 0 ? 5 : e}`;
});
emit('Erfolg', 'erfolge', erfolge, a => `${ix('profile', a.profil_id)}, ${ACHS.findIndex(x => x.code === a.code)}, ${ix('treffen', a.treffen_id)}, ${zeit(a.erreicht_am)}, 0`);
emit('Paar', 'paare', paare, p => `${ix('profile', p.von_id)}, ${ix('profile', p.zu_id)}, ${ix('profile', p.raeume_von)}, ${p.status === 'bestaetigt' ? 1 : 0}`);
const maske = ids => ids.reduce((m, id) => { const i = ix('profile', id); return i >= 0 ? (m | (1 << i)) >>> 0 : m; }, 0) >>> 0;
emit('Ausgabe', 'ausgaben', ausgaben, a => [
  `.treffen=${ix('treffen', a.treffen_id)}`, `.trip=${ix('trips', a.trip_id)}`, `.profil=${ix('profile', a.profil_id)}`, `.eingetragen_von=${ix('profile', a.eingetragen_von)}`,
  `.betrag=${a.betrag || 0}`, `.art=${Math.max(0, ARTEN.indexOf(a.art))}`, `.alkohol=${ja(a.alkohol)}`, `.fuer=${maske(json(a.fuer, []))}u`, `.fuer_n=${json(a.fuer, []).filter(id => ix('profile', id) >= 0).length}`, `.fuer_reihe={${json(a.fuer, []).filter(id => ix('profile', id) >= 0).map(id => ix('profile', id)).join(', ')}}`, `.liefer=${a.liefer || 0}`,
  `.km=${a.km || 0}`, `.verbrauch=${a.verbrauch || 0}`, `.spritpreis=${a.spritpreis || 0}`, `.verschleiss=${a.verschleiss || 0}`, `.hin_zurueck=${ja(a.hin_zurueck)}`,
  `.titel=${cstr(a.titel || '')}`].join(', '));
const posten = [];
ausgaben.forEach((a, i) => { if (a.art === 'essen') L.postenListe(a.posten).forEach(p => { if (ix('profile', p.id) >= 0) posten.push([i, ix('profile', p.id), p.cent, p.was]); }); });
emit('Posten', 'posten', posten, p => `${p[0]}, ${p[1]}, ${p[2]}, ${cstr(p[3])}`);
emit('Zahlung', 'zahlungen', zahlungen, z => `${ix('treffen', z.treffen_id)}, ${ix('profile', z.von_id)}, ${ix('profile', z.an_id)}, ${z.betrag}, ${Math.max(0, R.WERTE.zahlungStatus.indexOf(z.status))}, ${zeit(z.gemeldet_um)}, ${zeit(z.bestaetigt_um)}`);
emit('Frueher', 'frueher', frueher, f => {
  const m = String(f.monat || '').match(/^(\d{4})-(\d{2})/);
  const t = json(f.teilnehmer, []).reduce((x, n) => (x | (1 << namen.indexOf(n))) >>> 0, 0) >>> 0;
  const g = gastgeberNamen(f);
  return `${f.nr}, ${f.tage || 0}, ${m ? +m[1] * 100 + +m[2] : 0}, ${cstr(f.name)}, ${g.length ? namen.indexOf(g[0]) : -1}, ${g.length > 1 ? namen.indexOf(g[1]) : -1}, ${t}u`;
});
emit('Medium', 'medien', medien, m => `${Math.max(0, R.WERTE.medienArten.indexOf(m.art))}, ${cstr(m.symbol || '')}, 0, ${ix('profile', m.profil_id)}, ${ix('profile', m.gastgeber_id)}, ${m.pos || 0}, ${zeit(m.erledigt_um)}, ${cstr(m.titel || '')}, ${cstr(m.adresse || '')}`);
emit('Daumen', 'daumen', daumen, d => `${ix('medien', d.medien_id)}, ${ix('profile', d.profil_id)}, ${d.wert > 0 ? 1 : -1}`);
emit('Agenda', 'agenda', agenda, a => `${ix('treffen', a.treffen_id)}, ${ix('profile', a.profil_id)}, ${tag(a.datum)}, ${minuten(a.uhrzeit)}, ${cstr(a.name)}, ${cstr(a.notiz || '')}, 0`);
emit('Kommentar', 'kommentare', kommentare, k => `${ix('treffen', k.treffen_id)}, ${ix('profile', k.profil_id)}, ${ja(k.angepinnt)}, 0, ${idZeit(k.id)}, ${cstr(k.text)}`);
emit('Trip', 'trips', trips, t => `${ix('treffen', t.treffen_id)}, ${ix('profile', t.profil_id)}, ${tag(t.datum)}, ${minuten(t.uhrzeit)}, ${cstr(t.titel || '')}, ${cstr(t.wann || '')}`);
emit('Fahrt', 'fahrten', fahrten, f => `${ix('treffen', f.treffen_id)}, ${ix('trips', f.trip_id)}, ${ix('profile', f.profil_id)}, ${f.plaetze || 1}, ${f.richtung === 'rueck' ? 1 : 0}, 0`);
const tempE = lebend('temperatur_erledigt').filter(z => ix('treffen', z.treffen_id) >= 0 && ix('raeume', z.raum_id) >= 0);
emit('Temperatur', 'temperatur', tempE, z => `${ix('treffen', z.treffen_id)}, ${ix('raeume', z.raum_id)}, ${tag(z.nacht)}, ${z.temp || 0}`);
emit('Mitfahrer', 'mitfahrer', mitfahrer, m => `${ix('fahrten', m.fahrt_id)}, ${ix('profile', m.profil_id)}, 0`);

out('');
out('#define LADE(feld, quelle) do { db.feld.n = db.feld.cap = (int)(sizeof(quelle) / sizeof(quelle[0])); \\');
out('    db.feld.v = malloc(sizeof(quelle)); memcpy(db.feld.v, quelle, sizeof(quelle)); } while (0)');
out('void daten_laden(void) {');
for (const [feld, n] of tabellen) if (n) out(`  LADE(${feld}, G_${feld});`);
out('}');
fs.writeFileSync(ausgabePfad, zeilen.join('\n') + '\n');
EMOJIS.forEach(kodiere); ACHS.forEach(a => kodiere(a.emoji)); Object.values(L.AUSGABE_ART).forEach(x => kodiere(x.emoji));
fs.writeFileSync(path.join(path.dirname(ausgabePfad), 'emoji.txt'), [...EMOJI_SET].sort((a, b) => a - b).map(x => x.toString(16)).join('\n') + '\n');

// Prüfwerte für den Selbsttest (tools/pruefwerte.json): Kasse je Treffen nach logik.js
const pruef = { treffen: [] };
for (const t of treffen) {
  const st = L.kassenStand(D.ausgaben, t.id, D.profile);
  const paare = L.direktPaare(D.ausgaben, t.id, D.profile).filter(z => z.betrag > 0)
    .map(z => [ix('profile', z.von_id), ix('profile', z.an_id), z.betrag]);
  pruef.treffen.push({ i: ix('treffen', t.id), summe: st.summe, stand: Object.fromEntries(Object.entries(st.stand).map(([k, v]) => [ix('profile', k), v])), paare });
}
pruef.weisheit = { tag: tag('2026-11-22'), text: kodiere(L.weisheitDesTages('2026-11-22')) };
fs.writeFileSync(path.join(path.dirname(ausgabePfad), 'pruefwerte.txt'), toTxt(pruef));
function toTxt(p) {
  const z = [];
  for (const t of p.treffen) {
    z.push(`T ${t.i} ${t.summe}`);
    for (const [k, v] of Object.entries(t.stand)) z.push(`S ${t.i} ${k} ${v}`);
    for (const [a, b, c] of t.paare) z.push(`P ${t.i} ${a} ${b} ${c}`);
  }
  z.push(`W ${p.weisheit.tag} ${Buffer.from(p.weisheit.text, 'utf8').toString('hex')}`);
  return z.join('\n') + '\n';
}
console.log('Tabellen:', tabellen.map(([f, n]) => f + '=' + n).join(' '));
console.log('Heini-Namen:', namen.length);
