#!/usr/bin/env python3
"""mkrom.py – baut die .z64: libdragon-IPL3 (Public Domain) + statisches ELF, 256-Byte-ausgerichtet.

Aufruf: python3 tools/mkrom.py <ipl3_prod.z64> <programm.elf> <ausgabe.z64> [Titel]

Kopf: Titel (20 Zeichen), Spielkennung „NEDP“ = erweiterter Homebrew-Kopf (ID „ED“), Region P (Europa),
Byte 0x3F = 0x31: SRAM 256 kbit + Echtzeituhr (wird von Flashcarts und Emulatoren gelesen).
"""
import sys

ipl3_pfad, elf_pfad, aus_pfad = sys.argv[1:4]
titel = (sys.argv[4] if len(sys.argv) > 4 else 'HEINI-APP 64').encode('ascii')[:20].ljust(20, b' ')

rom = bytearray(open(ipl3_pfad, 'rb').read())
assert rom[:4] == b'\x80\x37\x12\x40', 'IPL3 nicht im z64-Format'
rom[0x20:0x34] = titel
rom[0x3B:0x40] = b'NEDP' + bytes([0x31])
while len(rom) % 256:
    rom.append(0)
elf = open(elf_pfad, 'rb').read()
assert elf[:4] == b'\x7fELF', 'kein ELF'
rom += elf
mib = 1 << 20
ziel = max(mib, (len(rom) + mib - 1) // mib * mib)
rom += bytes(ziel - len(rom))
open(aus_pfad, 'wb').write(rom)
print(f'{aus_pfad}: {len(rom)} Byte, ELF ab 0x{len(open(ipl3_pfad, "rb").read()) + 255 & ~255:x}, {len(elf)} Byte')
