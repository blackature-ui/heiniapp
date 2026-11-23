# Heini-App 64 – Bau der N64-ROM (clang/lld, ohne libdragon-Bibliothek) und des PC-Testprogramms.
#
#   make                 ROM bauen: build/heini-app-64.z64
#   make daten EXPORT=…  Grunddaten aus einem JSON-Export der Heini-App erzeugen (src/gen/data.c)
#   make host            Testprogramm für den PC (rendert Bilder als PNG, liest Tasten aus einer Datei)
#   make test            Selbsttest der Fachlogik auf dem PC (Kasse, Weisheit … gegen logik.js)

CLANG    ?= clang
LLD      ?= ld.lld
PYTHON   ?= python3
NODE     ?= node
HEINI_PUBLIC ?= ../heini-app/public

N64_CFLAGS = --target=mips-none-elf -march=mips2 -mabi=32 -EB -G0 -mno-abicalls -fno-pic \
	-ffreestanding -fno-stack-protector -mno-check-zero-division -ffunction-sections -fdata-sections -O2 \
	-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Isrc/n64/include -nostdinc -isystem $(shell $(CLANG) -print-resource-dir)/include -DN64=1
HOST_CFLAGS = -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare -DHOST=1

APP_SRC = $(wildcard src/*.c) src/gen/data.c src/gen/assets.c
N64_SRC = src/n64/crt0.S src/n64/libc.c src/n64/plat_n64.c $(APP_SRC)
HOST_SRC = src/host/plat_host.c $(APP_SRC)

N64_OBJ = $(patsubst %,build/n64/%.o,$(N64_SRC))

all: build/heini-app-64.z64

build/n64/%.o: %
	@mkdir -p $(dir $@)
	$(CLANG) $(N64_CFLAGS) -c $< -o $@

build/heini-app-64.elf: $(N64_OBJ) src/n64/n64.ld
	$(LLD) -m elf32btsmip -T src/n64/n64.ld --gc-sections -o $@ $(N64_OBJ)

build/heini-app-64.z64: build/heini-app-64.elf tools/mkrom.py tools/ipl3_prod.z64
	$(PYTHON) tools/mkrom.py tools/ipl3_prod.z64 $< $@ "HEINI-APP 64"

host: build/heini-host
build/heini-host: $(HOST_SRC) $(wildcard src/*.h)
	@mkdir -p build
	cc $(HOST_CFLAGS) -o $@ $(HOST_SRC) -lpng -lz

test: build/heini-host
	./build/heini-host --selbsttest src/gen/pruefwerte.txt

daten:
	$(NODE) tools/export2c.js $(EXPORT) $(HEINI_PUBLIC) src/gen/data.c

assets:
	$(PYTHON) tools/make_assets.py $(HEINI_PUBLIC) src/gen/assets.c

sim: build/n64sim
build/n64sim: tools/n64sim.c
	@mkdir -p build/sim
	cc -O2 -Wall -o $@ $< -lpng -lz

.PHONY: all host test daten assets sim
