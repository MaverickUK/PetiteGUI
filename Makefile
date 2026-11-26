# Builds build/PETITE.EXE with Open Watcom v2, a 16-bit DOS program for any 8088 or later.
#
#   make            the program
#   make test       the host unit tests (plain cc, no DOS)
#   make shots      screenshots of every scene in both looks, through DOSBox-X
#   make dostest    end-to-end tests in DOSBox-X: scanning, swapping, running games
#   make clean
#
# WATCOM is where Open Watcom v2 is unpacked (default ~/opt/openwatcom).

WATCOM ?= $(HOME)/opt/openwatcom
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
  OWBIN ?= $(WATCOM)/$(if $(filter arm64,$(UNAME_M)),armo64,bino64)
else
  OWBIN ?= $(WATCOM)/$(if $(filter aarch64 arm64,$(UNAME_M)),arml64,binl64)
endif
export WATCOM
export PATH := $(OWBIN):$(PATH)
export INCLUDE := $(WATCOM)/h

# -bt=dos: DOS target; -mm: medium model (code in as many 64 KB segments as it needs, data in
# one: calls are far, data pointers near); -0: 8088 instructions only; -ox: optimise; -w4: all
# warnings; -zq: quiet.
CFLAGS = -bt=dos -mm -0 -ox -w4 -zq

SRC = main.c draw.c gfx.c drv_cga.c drv_ega.c mouse.c theme.c rect.c util.c gminfo.c games.c launch.c swap.c files.c edit.c textbuf.c saverlib.c saver.c savers_fx.c savers_grid.c savers_info.c
# swapcore must come first: it has to be the first code in memory, so that everything after
# it can be swapped out while a game runs (see src/swapcore.asm).
OBJ = build/swapcore.obj $(patsubst %.c,build/%.obj,$(SRC))
HDR = $(wildcard src/*.h)
NASM ?= nasm

build/PETITE.EXE: $(OBJ)
	$(OWBIN)/wlink option quiet option stack=8192 option map=build/PETITE.MAP system dos name $@ file { $(OBJ) }

build/swapcore.obj: src/swapcore.asm | build
	$(NASM) -f obj -o $@ $<

build/%.obj: src/%.c $(HDR) | build
	$(OWBIN)/wcc $(CFLAGS) -fo=$@ $<

build:
	mkdir -p build

test:
	tests/run_tests.sh

shots: build/PETITE.EXE
	tools/shot.sh

dostest: build/PETITE.EXE
	tools/dostest.sh

clean:
	rm -rf build

.PHONY: test shots dostest clean
