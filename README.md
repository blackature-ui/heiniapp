# Heini-App 64

Die Heini-App (Version 1.61.0) als stationäre App für den Fernseher – portiert auf den **Nintendo 64**.
Bad, Dusche, Klo, Schlafplatz und Leih-Utensilien beim Freundestreffen, dazu Chat, Klopapier-Alarm,
Kasse und Ruhmeshalle. Profile wählt man vor dem Start im Hauptmenü.

## So funktioniert es

- **Grunddaten aus dem JSON-Export** der Heini-App landen fest im ROM (`src/gen/data.c`, wird erzeugt).
- **Alles, was am Fernseher passiert** (Ankunft, „Rein“, Freigeben, Klopapier-Alarm, Zusagen, Schlafplätze,
  Ausleihen, Chat, Umfragen, Reaktionen, Ausgaben, Kasse schließen, Zahlungen, Agenda, Kommentare, Ideen, Daumen …)
  wird als Aktion geprüft, ausgeführt und in ein **Journal im SRAM** (32 KB, batteriegepuffert) geschrieben.
  Beim Einschalten wird das Journal erneut ausgeführt. Kommt ein neuer Export ins ROM, beginnt das Journal leer.
- Die Regeln stammen aus `logik.js`, `rules.js` und den Abläufen in `server.js` der Web-App (Texte wörtlich).
  Die Kasse rechnet centgenau wie das Original (`make test` prüft gegen Werte, die `logik.js` selbst ausrechnet).
- **Uhr:** Hat das Modul eine Echtzeituhr (z. B. EverDrive 64 X7, SummerCart64), wird sie gelesen. Sonst
  im Hauptmenü einmal „Uhr stellen“; die Zeit läuft dann bis zum Ausschalten weiter.

## Bedienung (Controller)

| Taste | Wirkung |
| --- | --- |
| Steuerkreuz / Analogstick | Fokus bewegen |
| A | wählen, Knopf drücken |
| B | zurück |
| L / R | Reiter wechseln (Heute · Plan · Kasse · Treffen · Ideen) |
| Z | Chat öffnen bzw. Nachricht schreiben |
| START | Menü (Chat, Ruhmeshalle, Wer ist da?, Mein Profil, Einstellungen, Profil wechseln) |
| C-hoch / C-runter | schnell rollen |

Bildschirmtastatur: A tippen, B löschen, Z groß, L Symbole/Emojis, R Leerzeichen, START fertig.

## Aussehen

Wie am Handy: Material-You-Farben aus der Profilfarbe (hell oder dunkel, in den Einstellungen umschaltbar),
Kopfzeile mit Pokal, Chat und eigenem Profilbild, „Wer ist da?“-Zeile, unten die Reiterleiste mit den
originalen Lucide-Symbolen (Haus, Kalender, Euro, Ortsmarke, Formen), Karten, Segmente, Bottom-Sheets,
Toasts, Roboto-Flex-Schrift mit Kantenglättung und Farb-Emojis (Noto Color Emoji).

## Was geht, was nicht

Portiert: Hauptmenü mit Profilwahl, Heute (Klo-Weisheit, laufende Uhr mit Ring, „Rein“, Warteschlange,
„Als Pärchen“, „Dauert länger“, Klopapier-Alarm als Vollbild, Ankunft, „Mein Heini-Treffen“, Kasse kurz,
Offenes, Nach dem Treffen), Plan (Zeitleiste je Bad und Tag, eigenen Besuch nachträglich korrigieren),
Kasse & Trips (Ausgaben, Essensbestellung mit Einzelbeträgen und Lieferkosten, Stände, Direktzahlungen,
Kasse schließen/öffnen, Zahlungen, Trips und Autos anlegen, mitfahren, Fahrtkosten abrechnen),
Treffen (anlegen, bearbeiten, absagen, beenden, Zusagen, Agenda, Kommentare, Schlafplätze je Nacht,
Utensilien ausleihen, Bäder, Wunschtemperatur, Gold), Aktivitäten & Ideen, Chat (schreiben, antworten,
reagieren, anheften, löschen, Umfragen), Ruhmeshalle, Profile ansehen wie im Original (Kopf in der Profilfarbe, Adresse, Gaming-Equipment als
Konsolen-Kacheln mit Spielen und Zubehör, Auto, Startort, Pärchen, Getränke, Wunschtemperatur, Parkplätze, PayPal,
Räume, Unverträglichkeiten und „Mag ich nicht“ als Chips, Verleiht, Wichtelgeschenke) und bearbeiten (Über dich, Adresse,
Profilbild, Farbe, Gaming und Konsolen, Auto, Vorlieben, Pärchen anfragen/annehmen/lösen, „Wir wohnen
zusammen“, Wichtel, Gastgeber mit Räumen, Plätzen, Parkplätzen und Utensilien), Wer ist da?, Frühere Treffen
mit Statistik, alte Treffen-Aktivitäten (erledigt / nicht geklappt, mit Stimmen), Einstellungen (Design, Uhr,
Regeln, Speicher), Änderungsverlauf, Tour.

Nicht möglich ohne Internet: Abgleich mit dem Pi, Push und Erinnerungen, HeinAI, Spielesuche,
YouTube-Vorschauen, Fotos, GIFs und Sprachnachrichten (im Chat steht nur „1 Foto (nur in der App)“),
OneDrive-Links, Feedback, Teilen als Bild. Änderungen am Fernseher fließen nicht zurück in die Web-App.

## Bauen

Benötigt: `clang` und `ld.lld` (LLVM ≥ 15, mit MIPS-Ziel), `python3` mit Pillow, `node` (≥ 20) und den
Programmcode der Heini-App (Ordner `public/` mit `logik.js`, `rules.js`, `index.html`, Profilbildern und Logo).
Für `make assets` zusätzlich `rsvg-convert`, die Schriften Roboto Flex und Noto Color Emoji.

```sh
make daten EXPORT=heini-app-export.json HEINI_PUBLIC=../heini-app/public   # src/gen/data.c + Prüfwerte
make assets HEINI_PUBLIC=../heini-app/public                                # src/gen/assets.c (Schrift, Symbole, Emojis, Bilder)
make test          # Selbsttest der Kasse und Weisheit gegen logik.js (PC)
make               # build/heini-app-64.z64
```

`src/gen/` steht absichtlich nicht im Repository: dort landen Namen, Adressen, PayPal-Namen und Chats aus
dem Export sowie die Profilbilder.

Die ROM nutzt **keine libdragon-Bibliothek**, nur deren offenen IPL3-Bootcode (`tools/ipl3_prod.z64`,
Public Domain). VI, SI/PIF, PI und COP0 werden in `src/n64/` direkt angesprochen; `src/n64/libc.c` ist eine
kleine C-Bibliothek. Code ist MIPS II (läuft auf der VR4300), ohne Gleitkomma.

## Testen ohne Konsole

- `make host` baut dieselbe App für den PC: `build/heini-host --skript "warte:85 a shot:heute R shot:plan"`
  rendert Bilder nach `build/shots/` (Wörter siehe `src/host/plat_host.c`).
- `make sim` baut `tools/n64sim.c`, einen kleinen Befehlssimulator, der das echte N64-ELF ausführt
  (MIPS-Kern, VI-Interrupt, PI/SRAM, SI/PIF-Controller) und den Bildpuffer als PNG speichert:
  `build/n64sim build/heini-app-64.elf "warte:120 shot:hauptmenue a warte:10 shot:heute"`.
  Er ersetzt keinen Emulator und keine echte Konsole.

## Auf die Konsole

`build/heini-app-64.z64` auf ein Flashcart (EverDrive 64, SummerCart64 …) kopieren. Der ROM-Kopf meldet
„SRAM 256 kbit + Echtzeituhr“ (erweiterter Homebrew-Kopf); falls das Flashcart fragt: Speichertyp SRAM 32 KB.
Läuft auf PAL- und NTSC-Konsolen (640 × 480, Zeilensprung), ohne Expansion Pak. Nicht auf echter Hardware getestet.

## Lizenzen der Grafiken

Symbole: Lucide (ISC). Schrift: Roboto Flex (OFL). Emojis: Noto Color Emoji (OFL). Profilbilder stammen aus
der Heini-App und stehen deshalb nicht im Repository.
