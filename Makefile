.POSIX:

NAME = typyst
VERSION = 0.1
INSTALL_SHARE_DIR = ~/.local/share
INSTALL_BIN_DIR = ~/.local/bin
BUILD_DIR = _build
PKG_CONFIG = pkg-config
SDL_CONFIG = ./sdl2/build/bin/sdl2-config
SRC = $(wildcard src/*.c)

OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)
INCS = `$(PKG_CONFIG) --cflags fontconfig` `$(SDL_CONFIG) --prefix=./sdl2/build --cflags`
SDL_LIBS = -L./sdl2/build/lib '-Wl,-rpath,$$ORIGIN/lib' -Wl,--enable-new-dtags -lSDL2 -lSDL2_ttf -lSDL2_gfx
LIBS = -lutil `$(PKG_CONFIG) --libs fontconfig` $(SDL_LIBS)

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
	mkdir -p $(BUILD_DIR)/lib
	cp -r sdl2/build/lib/*.so* $(BUILD_DIR)/lib

$(BUILD_DIR)/%.o: src/%.c | build_dir
	$(CC) -o $@ $(STCFLAGS) -c $<

$(EXE): $(OBJ) | build_dir
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

run: $(EXE)
	$(EXE)

test: clean $(EXE)
	$(EXE) bash --init-file ./test.sh

debug:
	CFLAGS=-DDEBUG make test

clean:
	rm -rf $(BUILD_DIR)

install: $(EXE)
	rm -rf $(INSTALL_BIN_DIR)/$(NAME)
	rm -rf $(INSTALL_SHARE_DIR)/$(NAME)
	mkdir -p $(INSTALL_SHARE_DIR)/$(NAME)
	cp -r $(BUILD_DIR)/* $(INSTALL_SHARE_DIR)/$(NAME)
	ln -st $(INSTALL_BIN_DIR) $(INSTALL_SHARE_DIR)/$(NAME)/$(NAME)
	tic -sx tic.info

uninstall:
	rm -f $(INSTALL_BIN_DIR)/$(NAME)
	rm -f $(INSTALL_SHARE_DIR)/$(NAME)

.PHONY: all options clean install uninstall
