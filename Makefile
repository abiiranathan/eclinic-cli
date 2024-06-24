CC=gcc
CFLAGS=-Wall -Werror -Wextra -pedantic -std=c2x -O3
LDFLAGS=-lm -lcsvparser -lsolidc -lpq
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
TARGET=eclinic

all: eclinic

eclinic: $(OBJS) | bcrypt
	$(CC) $(CFLAGS) -o $@ $^ libbcrypt/bcrypt.a $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# This will generate libbcrypt/bcrypt.a
bcrypt:
	make -C libbcrypt

clean:
	rm -f $(OBJS) $(TARGET)
