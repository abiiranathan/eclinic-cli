CC=gcc
CFLAGS=-Wall -Werror -Wextra -pedantic -std=c2x -O3 -s
LDFLAGS=-lm -lcsvparser -lsolidc -lpq -Wl,-rpath=./libs

SRC_DIR=src
OBJ_DIR=obj
SRCS=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
BIN_DIR=bin
TARGET=$(BIN_DIR)/eclinic
LIBS_DIR=$(BIN_DIR)/libs

all: $(TARGET) copy_libs

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	
$(OBJ_DIR): bcrypt
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LIBS_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ libbcrypt/bcrypt.a -o $@ $(LDFLAGS)

bcrypt:
	make -C libbcrypt

copy_libs:
	python3 copylibs.py $(TARGET) $(LIBS_DIR)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) libbcrypt/*.{o,a}

.PHONY: all clean
