CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
AR ?= ar

all: libsxbus.a sx_bus_probe sx_bus_daemon

libsxbus.a: sx_bus_core.o
	$(AR) rcs $@ $^

sx_bus_core.o: sx_bus_core.c sx_bus_core.h
	$(CC) $(CFLAGS) -c sx_bus_core.c -o sx_bus_core.o

sx_bus_probe: sx_bus_probe.c libsxbus.a sx_bus_core.h
	$(CC) $(CFLAGS) sx_bus_probe.c -L. -lsxbus -o sx_bus_probe

sx_bus_daemon: sx_bus_daemon.c libsxbus.a sx_bus_core.h
	$(CC) $(CFLAGS) sx_bus_daemon.c -L. -lsxbus -o sx_bus_daemon

clean:
	rm -f *.o *.a sx_bus_probe sx_bus_daemon
