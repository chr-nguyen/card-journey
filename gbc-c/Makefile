# GBDK-2020 build for the card-journey GBC ROM.
# Override GBDK_HOME if you installed it elsewhere: make GBDK_HOME=/path/to/gbdk/

GBDK_HOME ?= $(HOME)/gbdk/
LCC       = $(GBDK_HOME)bin/lcc

# -Wm-yc : mark the ROM as Game Boy Color compatible (runs on GBC and DMG)
CFLAGS = -Wm-yc

ROM = card-journey.gbc
SRC = src/main.c

all: $(ROM)

$(ROM): $(SRC)
	$(LCC) $(CFLAGS) -o $(ROM) $(SRC)

clean:
	rm -f $(ROM) src/*.o src/*.lst src/*.sym src/*.asm src/*.ihx \
	      src/*.map src/*.noi src/*.rel src/*.adb *.map *.noi *.ihx

.PHONY: all clean
