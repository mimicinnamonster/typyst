.POSIX:

CC = gcc
NAME = typyst
VERSION = 0.1
INSTALL_SHARE_DIR = ~/.local/share
INSTALL_BIN_DIR = ~/.local/bin
BUILD_DIR = _build
PKG_CONFIG = pkg-config
PKGCONF_DEPS = fontconfig sdl2 SDL2_ttf SDL2_gfx

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)
INCS = `$(PKG_CONFIG) --cflags $(PKGCONF_DEPS)`
# INCS = -D_REENTRANT -I/usr/include/uuid -I/usr/include/freetype2 -I/usr/include/libpng16 -I../SDL/include -I../SDL_ttf/SDL_ttf -I../SDL_gfx
LIBS = -lutil `$(PKG_CONFIG) --libs $(PKGCONF_DEPS)` #-lg
# LIBS = -L../SDL/build/build/ -L../SDL_ttf/build/ -L../SDL_gfx/ -lutil -lfontconfig -lfreetype -lSDL2_ttf -lSDL2_gfx -lSDL2
EXE = $(BUILD_DIR)/$(NAME)

BASE_CFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -std=c11 -pedantic # -Werror
# BASE_LDFLAGS = -Wl,-rpath,\$$ORIGIN
# sdl2-compat dlopens SDL3 by bare soname at load time and aborts the process
# if it is not found ("fatal error: cannot load sdl3"). The brew sdl3 dylibs
# live under $(brew --prefix sdl3)/lib, not the flat /opt/homebrew/lib, so give
# dyld an explicit absolute rpath that does not depend on sdl2-compat's
# relative Cellar rpath or on ambient DYLD_* environment.
SDL3_LIBDIR = `brew --prefix sdl3`/lib
BASE_LDFLAGS = -Wl,-rpath,$(SDL3_LIBDIR)

DEBUG_CFLAGS = -ggdb -Wall -Wextra -fsanitize=address # -fsanitize=undefined # -fprofile-arcs -ftest-coverag
DEBUG_LDFLAGS = -fsanitize=address # -fsanitize=undefined # -fprofile-arcs -ftest-coverage

RELEASE_CFLAGS = -O3 -s
RELEASE_LDFLAGS = -O3 -s

CC_CFLAGS = $(BASE_CFLAGS) $(CFLAGS) $(INCS)
CC_LDFLAGS = $(BASE_LDFLAGS) $(LDFLAGS) $(LIBS)

all: options $(EXE)

options:
	@echo build options:
	@echo "CFLAGS     = $(CFLAGS)"
	@echo "LDFLAGS    = $(LDFLAGS)"
	@echo "CC_CFLAGS  = $(CC_CFLAGS)"
	@echo "CC_LDFLAGS = $(CC_LDFLAGS)"
	@echo "CC         = $(CC)"

build_dir:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | build_dir
	$(CC) $(CC_CFLAGS) -o $@ -c $<

$(EXE): $(OBJ) | build_dir
	$(CC) -o $@ $(OBJ) $(CC_LDFLAGS)

release: clean
	CFLAGS="-g $(RELEASE_CFLAGS)" LDFLAGS="-g $(RELEASE_LDFLAGS)" make $(EXE)

test: clean $(EXE)
	$(EXE) bash --init-file ./test.sh

debug: clean
	@echo $(CFLAGS)
	CFLAGS="-g -DDEBUG $(DEBUG_CFLAGS) $(CFLAGS)" LDFLAGS="-g $(DEBUG_LDFLAGS)" make $(EXE)
	#$(EXE) -a ~/Videos/bgs/girl.gif bash --init-file ./test.sh
	#$(EXE) bash --init-file ./test2.sh

analyze:
	CFLAGS="-fanalyzer $(CFLAGS)" make debug

profile:
	sudo sysctl kernel.perf_event_paranoid=-1
	LDFLAGS="-g" CFLAGS="-g" make $(EXE)
	perf record -g --call-graph dwarf $(EXE) -a ~/Videos/bgs/girl.gif
	sudo sysctl kernel.perf_event_paranoid=0

clean:
	rm -rf $(EXE) $(OBJ)

install: $(EXE)
	rm -rf $(INSTALL_BIN_DIR)/$(NAME)
	rm -rf $(INSTALL_SHARE_DIR)/$(NAME)
	mkdir -p $(INSTALL_SHARE_DIR)/$(NAME)
	cp $(EXE) $(INSTALL_BIN_DIR)/$(NAME)
	tic -sx tic.info

uninstall:
	rm -f $(INSTALL_BIN_DIR)/$(NAME)
	rm -f $(INSTALL_SHARE_DIR)/$(NAME)

.PHONY: all options clean install uninstall
