# GBDK-2020 build for the CARD JOURNEY GBC ROM.
# Override GBDK_HOME if you installed it elsewhere: make GBDK_HOME=/path/to/gbdk/

GBDK_HOME ?= $(HOME)/gbdk/
LCC       = $(GBDK_HOME)bin/lcc

# -Wm-yC : mark the ROM as Game Boy Color exclusive (the game needs color)
# -Wm-yn : cartridge title in the ROM header
CFLAGS = -Wm-yC -Wm-yn"CARDJOURNEY"

ROM = card-journey.gbc
SRC = src/main.c src/assets.c

all: $(ROM)

$(ROM): $(SRC) src/assets.h
	$(LCC) $(CFLAGS) -o $(ROM) $(SRC)

# regenerate the tile data from the ASCII art definitions
assets:
	python3 tools/gen_assets.py

clean:
	rm -f $(ROM) src/*.o src/*.lst src/*.sym src/*.asm src/*.ihx \
	      src/*.map src/*.noi src/*.rel src/*.adb *.map *.noi *.ihx

.PHONY: all assets clean
