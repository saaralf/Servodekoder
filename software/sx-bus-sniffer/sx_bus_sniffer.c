#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_run = 1;

static void on_sigint(int sig) {
    (void)sig;
    g_run = 0;
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

    cfmakeraw(&tty);
    speed_t spd = baud_to_flag(baud);
    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 100ms

    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -1;
    return 0;
}

static int wr2(int fd, uint8_t a, uint8_t b) {
    uint8_t p[2] = {a, b};
    ssize_t n = write(fd, p, 2);
    return (n == 2) ? 0 : -1;
}

static void ts_now(char *buf, size_t n) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    localtime_r(&ts.tv_sec, &tmv);
    snprintf(buf, n, "%02d:%02d:%02d.%03ld", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
}

int main(int argc, char **argv) {
    const char *port = "/dev/ttyUSB0";
    int baud = 57600;

    if (argc >= 2) port = argv[1];
    if (argc >= 3) baud = atoi(argv[2]);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", port, strerror(errno));
        return 1;
    }
    if (set_serial(fd, baud) != 0) {
        fprintf(stderr, "serial setup failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Rautenhaus feedback ON auf beiden Bussen (ADR126=160)
    if (wr2(fd, 0xFE, 0xB0) != 0 || wr2(fd, 0xFE, 160) != 0) {
        fprintf(stderr, "warn: set SX0 ADR126=160 failed\n");
    }
    usleep(3000);
    if (wr2(fd, 0xFE, 0xB1) != 0 || wr2(fd, 0xFE, 160) != 0) {
        fprintf(stderr, "warn: set SX1 ADR126=160 failed\n");
    }

    printf("sniffer on %s @ %d (Ctrl-C to stop)\n", port, baud);
    printf("format: time bus adr data hex bits\n");

    int pending = -1;
    while (g_run) {
        uint8_t buf[1024];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            }
            fprintf(stderr, "read error: %s\n", strerror(errno));
            break;
        }
        if (n == 0) {
            usleep(5000);
            continue;
        }

        for (ssize_t i = 0; i < n; i++) {
            uint8_t b = buf[i];
            if (pending < 0) {
                pending = b;
                continue;
            }
            uint8_t adr_raw = (uint8_t)pending;
            pending = -1;

            int bus = (adr_raw & 0x80) ? 1 : 0;
            int adr = adr_raw & 0x7F;
            int d = b;

            char ts[32];
            ts_now(ts, sizeof(ts));

            printf("%s SX%d A%03d D%03d 0x%02X bits=%d%d%d%d%d%d%d%d",
                   ts, bus, adr, d, d,
                   (d>>7)&1, (d>>6)&1, (d>>5)&1, (d>>4)&1,
                   (d>>3)&1, (d>>2)&1, (d>>1)&1, d&1);

            if (adr == 0) {
                printf("  [K0 b7=%d b0=%d]", (d>>7)&1, d&1);
            }
            if (adr == 109) {
                printf("  [ADR109 track(bit7)=%d]", (d>>7)&1);
            }
            printf("\n");
            fflush(stdout);
        }
    }

    close(fd);
    return 0;
}
