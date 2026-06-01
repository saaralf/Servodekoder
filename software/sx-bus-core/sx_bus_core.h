#ifndef SX_BUS_CORE_H
#define SX_BUS_CORE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fd;
    int last_track_sx0; // -1 unknown, 0 off, 1 on
    int pending;
    int profile;
} sx_bus_ctx;

enum {
    SX_PROFILE_SLX825_SX0_STREAM = 1
};

typedef void (*sx_frame_cb)(int bus, int adr, int data, void *user);
typedef void (*sx_track_cb)(int track, void *user);

int sx_open(sx_bus_ctx *ctx, const char *port, int baud);
void sx_close(sx_bus_ctx *ctx);
int sx_enable_feedback(sx_bus_ctx *ctx);
int sx_poll(sx_bus_ctx *ctx, sx_frame_cb on_frame, sx_track_cb on_track, void *user);
int sx_set_profile(sx_bus_ctx *ctx, int profile);
const char* sx_profile_name(int profile);

#endif
