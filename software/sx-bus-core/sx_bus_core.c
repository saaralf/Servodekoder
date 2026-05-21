#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "sx_bus_core.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

static int g_trace_raw = -1;

static int trace_raw_enabled(void) {
    if (g_trace_raw < 0) {
        const char *e = getenv("SX_TRACE_RAW");
        g_trace_raw = (e && *e && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return g_trace_raw;
}

static void trace_hex(const char *dir, const uint8_t *b, int n) {
    if (!trace_raw_enabled()) return;
    fprintf(stderr, "sxraw %s", dir);
    for (int i = 0; i < n; i++) fprintf(stderr, " %02X", b[i]);
    fputc('\n', stderr);
}

static speed_t baud_to_flag(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B57600;
    }
}

static int set_serial(int fd, int baud) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -1;
#ifdef __GLIBC__
    cfmakeraw(&tty);
#else
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
#endif
    speed_t spd = baud_to_flag(baud);
    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    return tcsetattr(fd, TCSANOW, &tty);
}

static int wr2(int fd, uint8_t a, uint8_t b) {
    uint8_t p[2] = {a, b};
    trace_hex("TX", p, 2);
    return write(fd, p, 2) == 2 ? 0 : -1;
}

int sx_open(sx_bus_ctx *ctx, const char *port, int baud) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
    ctx->last_track_sx0 = -1;
    ctx->pending = -1;
    ctx->profile = SX_PROFILE_SLX825_SX0_STREAM;

    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) return -1;
    if (set_serial(fd, baud) != 0) {
        close(fd);
        return -1;
    }
    ctx->fd = fd;
    return 0;
}

void sx_close(sx_bus_ctx *ctx) {
    if (ctx->fd >= 0) close(ctx->fd);
    ctx->fd = -1;
}

int sx_set_profile(sx_bus_ctx *ctx, int profile) {
    if (!ctx) return -1;
    if (profile != SX_PROFILE_SLX825_SX0_STREAM) return -1;
    ctx->profile = profile;
    return 0;
}

const char* sx_profile_name(int profile) {
    switch (profile) {
        case SX_PROFILE_SLX825_SX0_STREAM: return "slx825_sx0_stream";
        default: return "unknown";
    }
}

int sx_enable_feedback(sx_bus_ctx *ctx) {
    if (ctx->fd < 0) return -1;

    // SLX825 wie SX4: Rautenhaus+Feedback per FE A0
    if (wr2(ctx->fd, 0xFE, 0xA0) != 0) return -1;
    usleep(10000);

    return 0;
}

int sx_poll(sx_bus_ctx *ctx, sx_frame_cb on_frame, sx_track_cb on_track, void *user) {
    if (ctx->fd < 0) return -1;
    uint8_t buf[1024];
    int frames = 0;

    while (1) {
        ssize_t n = read(ctx->fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
        if (n == 0) break;
        trace_hex("RX", buf, (int)n);

        for (ssize_t i = 0; i < n; i++) {
            uint8_t b = buf[i];
            if (ctx->pending < 0) {
                ctx->pending = b;
                continue;
            }
            uint8_t adr_raw = (uint8_t)ctx->pending;
            ctx->pending = -1;

            int bus = (adr_raw & 0x80) ? 1 : 0;
            int adr = adr_raw & 0x7F;
            int data = b;
            if (adr == 0 && data == 0) {
                // SX4 workaround: sporadische 0/0-Artefakte ignorieren
                continue;
            }
            frames++;

            if (on_frame) on_frame(bus, adr, data, user);
            if (bus == 0 && adr == 109) {
                int tr = (data >> 7) & 1;
                if (tr != ctx->last_track_sx0) {
                    ctx->last_track_sx0 = tr;
                    if (on_track) on_track(tr, user);
                }
            }
        }
    }
    return frames;
}
