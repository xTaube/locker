CC = gcc
C_VERSION = c2x
CFLAGS = -std=$(C_VERSION) -Wall -Wextra -Werror -Isrc/include -Isrc/libsodium/include -Isrc/ncurses/include
LDFLAGS = -Lsrc/lib -lsqlite3 -Lsrc/libsodium/lib -lsodium -Lsrc/ncurses/lib -ltinfow -lncursesw
DEBUG_FLAGS = -fsanitize=undefined -g


BIN = bin
EXEC = $(BIN)/locker.exe
DEBUG_EXEC = $(BIN)/debug.exe
SRC = src/locker


$(BIN):
	mkdir -p $(BIN)

$(EXEC): $(BIN) $(SRC)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRC)/*.c $(LDFLAGS)

$(DEBUG_EXEC): $(BIN) $(SRC)
	$(CC) $(DEBUG_FLAGS) $(CFLAGS) -o $(DEBUG_EXEC) $(SRC)/*.c $(LDFLAGS)

.PHONY: run
run: $(EXEC)
	./$(EXEC)

.PHONY: debug
debug: $(DEBUG_EXEC)
	./$(DEBUG_EXEC)

clean: $(EXEC)
	rm $(EXEC)
