# GBDK-2020 build for the CARD JOURNEY GBC ROM.
# Override GBDK_HOME if you installed it elsewhere: make GBDK_HOME=/path/to/gbdk/

GBDK_HOME ?= $(HOME)/gbdk/
LCC       = $(GBDK_HOME)bin/lcc

# -Wm-yC : mark the ROM as Game Boy Color exclusive (the game needs color)
# -Wm-yn : cartridge title in the ROM header
CFLAGS = -Wm-yC -Wm-yn"CARDJOURNEY"

ROM = card-journey.gbc
SRC = src/main.c src/assets.c src/cards.c src/battle.c src/ai.c
HEADERS = src/assets.h src/cards.h src/battle.h src/ai.h src/duel_ui.h
HOST_CC ?= cc
PYBOY_PYTHON ?= python3

all: $(ROM)

$(ROM): $(SRC) $(HEADERS)
	$(LCC) $(CFLAGS) -o $(ROM) $(SRC)

# regenerate the tile data from the ASCII art definitions
assets:
	python3 tools/gen_assets.py

test:
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -pedantic -Isrc \
		src/cards.c src/battle.c src/ai.c tests/battle_test.c -o /tmp/card-journey-battle-test
	/tmp/card-journey-battle-test

emulator-test: $(ROM)
	$(LCC) -S -o /tmp/card-journey-main.asm src/main.c
	$(HOST_CC) -std=c99 -shared -fPIC -Isrc src/cards.c src/battle.c src/ai.c \
		-o /tmp/card-journey-rules.so
	$(PYBOY_PYTHON) tests/emulator_smoke.py

clean:
	rm -f $(ROM) src/*.o src/*.lst src/*.sym src/*.asm src/*.ihx \
	      src/*.map src/*.noi src/*.rel src/*.adb *.map *.noi *.ihx

.PHONY: all assets clean test emulator-test
