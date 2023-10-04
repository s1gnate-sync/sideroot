CC = musl-gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?=

PROG := sideroot
SRCS := $(PROG).c

all: $(PROG)

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -static $(LDFLAGS)

install:
	sudo mv $(PROG) /usr/local/bin
	sudo setcap =ep /usr/local/bin/$(PROG)

clean:
	rm -f $(PROG)
