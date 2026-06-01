#define _POSIX_C_SOURCE 200809L

#include "sx_bus_core.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_run = 1;

static void on_sig(int s){ (void)s; g_run=0; }

static void ts(char *buf, size_t n){
    struct timespec t; clock_gettime(CLOCK_REALTIME, &t);
    struct tm tmv; localtime_r(&t.tv_sec, &tmv);
    snprintf(buf, n, "%02d:%02d:%02d.%03ld", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, t.tv_nsec/1000000);
}

static void on_frame(int bus, int adr, int data, void *u){
    (void)u;
    if (adr==0 || adr==109 || adr==127) {
        char b[32]; ts(b,sizeof(b));
        printf("%s SX%d A%03d D%03d 0x%02X\n", b, bus, adr, data, data);
    }
}

static void on_track(int track, void *u){
    (void)u;
    char b[32]; ts(b,sizeof(b));
    printf("%s TRACK(SX0/ADR109.bit7) => %d\n", b, track);
}

int main(int argc, char **argv){
    const char *port = argc>1 ? argv[1] : "/dev/ttyUSB0";
    int baud = argc>2 ? atoi(argv[2]) : 57600;

    signal(SIGINT,on_sig);
    signal(SIGTERM,on_sig);

    sx_bus_ctx ctx;
    if (sx_open(&ctx, port, baud) != 0) {
        perror("sx_open"); return 1;
    }
    if (sx_enable_feedback(&ctx) != 0) {
        perror("sx_enable_feedback"); sx_close(&ctx); return 1;
    }

    printf("sx_bus_probe on %s @ %d\n", port, baud);
    while(g_run){
        int r = sx_poll(&ctx, on_frame, on_track, NULL);
        if (r < 0) { perror("sx_poll"); break; }
        usleep(10000);
    }
    sx_close(&ctx);
    return 0;
}
