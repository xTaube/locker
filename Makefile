CC = gcc
C_VERSION = c2x
LDFLAGS = -Lsrc/lib -lsqlite3 -Lsrc/libsodium/lib -lsodium -Lsrc/ncurses/lib -ltinfow -lncursesw
DEBUG_CFLAGS = -O0 -fsanitize=undefined,address -g -std=$(C_VERSION) -Wall -Wextra -Werror -Isrc/include -Isrc/libsodium/include -Isrc/ncurses/include

INSTALL_CFLAGS = -O2 -std=$(C_VERSION) -Isrc/include -Isrc/libsodium/include -Isrc/ncurses/include

BIN = bin
EXEC = $(BIN)/locker
DEBUG_EXEC = $(BIN)/locker-debug
SRC = src/locker


$(BIN):
	mkdir -p $(BIN)

$(EXEC): $(BIN) $(SRC)
	$(CC) $(INSTALL_CFLAGS) -o $(EXEC) $(SRC)/*.c $(LDFLAGS)

$(DEBUG_EXEC): $(BIN) $(SRC)
	$(CC) $(DEBUG_CFLAGS) $(CFLAGS) -o $(DEBUG_EXEC) $(SRC)/*.c $(LDFLAGS)

.PHONY: install
install: $(EXEC)
	mkdir -p $(INSTALL_DIR)/locker
	cp $(EXEC) $(INSTALL_DIR)/locker

.PHONY: debug
debug: $(DEBUG_EXEC)
	./$(DEBUG_EXEC)

.PHONY: clean
clean:
	rm -rf $(BIN)
