CC ?= gcc
C_VERSION ?= c11
LDFLAGS = -Lsrc/lib -lsqlite3 -Lsrc/libsodium/lib -lsodium -Lsrc/ncurses/lib -ltinfow -lncursesw
DEBUG_CFLAGS = -O0 -fsanitize=undefined,address -g -std=$(C_VERSION) -Wall -Wextra -Werror -Isrc/include -Isrc/libsodium/include -Isrc/ncurses/include

INSTALL_CFLAGS = -O2 -std=$(C_VERSION) -Isrc/include -Isrc/libsodium/include -Isrc/ncurses/include

BIN = bin
EXEC = $(BIN)/locker
DEBUG_EXEC = $(BIN)/locker-debug
SRC = src/locker
DEVELOPMENT_DIR = development

$(BIN):
	@mkdir -p $(BIN)

$(DEBUG_EXEC): $(BIN) $(SRC)
	$(CC) $(DEBUG_CFLAGS) -o $(DEBUG_EXEC) $(SRC)/*.c $(LDFLAGS)

$(DEVELOPMENT_DIR):
	@mkdir -p $(DEVELOPMENT_DIR)
	@mkdir -p $(DEVELOPMENT_DIR)/lockers

.PHONY: install
install:
	$(CC) $(INSTALL_CFLAGS) -o $(EXEC) $(SRC)/*.c $(LDFLAGS)
	mkdir -p $(INSTALL_DIR)/locker

	mkdir -p $(INSTALL_DIR)/locker/bin
	cp $(EXEC) $(INSTALL_DIR)/locker/bin

	mkdir -p $(INSTALL_DIR)/locker/lockers

.PHONY: debug
debug: $(DEVELOPMENT_DIR) $(DEBUG_EXEC)
	LOCKER_PATH=$(DEVELOPMENT_DIR) ./$(DEBUG_EXEC)

.PHONY: clean
clean:
	@rm -rf $(BIN)/*
