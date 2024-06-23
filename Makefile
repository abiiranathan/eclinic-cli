CC=gcc
CFLAGS=-Wall -Werror -Wextra -pedantic -std=c2x -O3
LDFLAGS=-lm -lcsvparser -lsolidc -lpq
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
TARGET=eclinic

all: eclinic

eclinic: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
