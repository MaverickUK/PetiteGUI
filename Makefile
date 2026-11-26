# Builds build/PETITE.EXE with Open Watcom v2, a 16-bit DOS program for any 8088 or later.
#
#   make            the program
#   make test       the host unit tests (plain cc, no DOS)
#   make shots      screenshots of every scene in both looks, through DOSBox-X
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

# -bt=dos: DOS target; -ms: small model (64 KB code, 64 KB data); -0: 8088 instructions only;
# -ox: optimise; -w4: all warnings; -zq: quiet.
CFLAGS = -bt=dos -ms -0 -ox -w4 -zq

SRC = main.c draw.c gfx.c drv_cga.c drv_ega.c mouse.c theme.c rect.c util.c
OBJ = $(patsubst %.c,build/%.obj,$(SRC))
HDR = $(wildcard src/*.h)

build/PETITE.EXE: $(OBJ)
	$(OWBIN)/wlink option quiet system dos name $@ file { $(OBJ) }

build/%.obj: src/%.c $(HDR) | build
	$(OWBIN)/wcc $(CFLAGS) -fo=$@ $<

build:
	mkdir -p build

test:
	tests/run_tests.sh

shots: build/PETITE.EXE
	tools/shot.sh

clean:
	rm -rf build

.PHONY: test shots clean
