.POSIX:

NAME = typyst
VERSION = 0.1
INSTALL_DIR = ~/.local/bin
BUILD_DIR = _build
PKG_CONFIG = pkg-config
SDL_CONFIG = ./sdl2/install/bin/sdl2-config
SRC = $(wildcard src/*.c)

OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)
INCS = `$(PKG_CONFIG) --cflags fontconfig` `$(SDL_CONFIG) --cflags`
LIBS = -lutil `$(PKG_CONFIG) --libs fontconfig` `$(SDL_CONFIG) --libs` -lSDL2_ttf
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
	$(EXE) ./test.sh

clean:
	rm -rf $(BUILD_DIR)

install: $(EXE)
	mkdir -p $(INSTALL_DIR)
	cp -f $(EXE) $(INSTALL_DIR)
	# tic -sx tic.info

uninstall:
	rm -f $(INSTALL_DIR)/$(NAME)

.PHONY: all options clean install uninstall
