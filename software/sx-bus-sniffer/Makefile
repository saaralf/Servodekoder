CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

all: sx_bus_sniffer

sx_bus_sniffer: sx_bus_sniffer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f sx_bus_sniffer
