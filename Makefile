SRC := $(wildcard src/*.c)

NAME = filmfs

DESTDIR = ~/.local/bin/

CC = gcc

BIN_DIR = bin

BUILD_DIR = build

CONFIG_DIR = ~/.config/filmfs/

OBJS = $(SRC:src/%.c=$(BUILD_DIR)/%.o)

CFLAGS = -Wall -Wextra -pedantic -g -I include

LDFLAGS := $(shell pkg-config fuse --libs) -lsqlite3
CFLAGS += $(shell pkg-config fuse --cflags)

all: bin $(BIN_DIR)/$(NAME)

$(BIN_DIR)/$(NAME): $(OBJS)
	$(CC) -o $(BIN_DIR)/$(NAME) $(OBJS) $(CFLAGS) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -g $(CFLAGS) -c $< -o $@

bin:
	mkdir -p $(BIN_DIR)

clean:
	rm -f $(OBJS)
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

install: $(BIN_DIR)/$(NAME) 
	mkdir $(DESTDIR)
	cp -f -r $(BIN_DIR)/$(NAME) $(DESTDIR)
	cp -n config/config $(CONFIG_DIR)

re: fclean all

uninstall: $(BIN_DIR)/$(NAME)
	rm -f $(DESTDIR)$(NAME)

.PHONY: all fclean install re uninstall
