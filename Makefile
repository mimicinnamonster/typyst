.POSIX:

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
LIBS = -lutil `$(PKG_CONFIG) --libs $(PKGCONF_DEPS)` #-lg 
EXE = $(BUILD_DIR)/$(NAME)

BASE_CFLAGS = # -fprofile-arcs -ftest-coverage # -fsanitize=address -fsanitize=undefined # -Wall -Wextra -fanalyzer

BASE_LDFLAGS = # -fprofile-arcs -ftest-coverage # -fsanitize=address -fsanitize=undefined

CC_CFLAGS = $(INCS) $(BASE_CFLAGS) $(CFLAGS)
CC_LDFLAGS = $(LIBS) $(BASE_LDFLAGS) $(LDFLAGS)

all: options $(EXE)

options:
	@echo build options:
	@echo "CFLAGS  = $(CC_CFLAGS)"
	@echo "LDFLAGS = $(CC_LDFLAGS)"
	@echo "CC      = $(CC)"

build_dir:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | build_dir
	$(CC) $(CC_CFLAGS) -o $@ -c $<

$(EXE): $(OBJ) | build_dir
	$(CC) -o $@ $(OBJ) $(CC_LDFLAGS)

run: $(EXE)
	$(EXE)

test: clean $(EXE)
	$(EXE) bash --init-file ./test.sh

test2: clean $(EXE)
	$(EXE) bash ./test2.sh

debug:
	CFLAGS="-g -DDEBUG" LDFLAGS="-g" make test

profile:
	LDFLAGS="-g" CFLAGS="-g" make $(EXE)
	perf record -g --call-graph dwarf $(EXE)

clean:
	rm -rf $(BUILD_DIR)

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
