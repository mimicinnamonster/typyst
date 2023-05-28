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
LIBS = -lutil `$(PKG_CONFIG) --libs $(PKGCONF_DEPS)`

EXE = $(BUILD_DIR)/$(NAME)
STCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=600
STCFLAGS = $(INCS) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
STLDFLAGS = $(LIBS) $(LDFLAGS)

all: options $(EXE)

options:
	@echo build options:
	@echo "CFLAGS  = $(STCFLAGS)"
	@echo "LDFLAGS = $(STLDFLAGS)"
	@echo "CC      = $(CC)"

build_dir:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | build_dir
	$(CC) -o $@ $(STCFLAGS) -c $<

$(EXE): $(OBJ) | build_dir
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

run: $(EXE)
	$(EXE)

test: clean $(EXE)
	$(EXE) bash --init-file ./test.sh

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
	tic -sx tic.info

uninstall:
	rm -f $(INSTALL_BIN_DIR)/$(NAME)
	rm -f $(INSTALL_SHARE_DIR)/$(NAME)

.PHONY: all options clean install uninstall
