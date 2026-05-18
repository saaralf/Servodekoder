#define _POSIX_C_SOURCE 200809L

#include "sx_bus_core.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 16
#define SOCK_PATH_DEFAULT "/tmp/sxbusd2.sock"

static volatile sig_atomic_t g_run = 1;
static int clients[MAX_CLIENTS];
static sx_bus_ctx* g_ctx = NULL;
static int g_last_track = -1;
static int g_suppress_broadcast = 0;
static int g_quiet_mode = 0; // 0: broadcast all SX0 FRAMEs + TRACK
static int g_track_candidate = -1;
static int g_track_candidate_count = 0;
static long long g_track_last_commit_ms = 0;
static long long g_last_track_poll_ms = 0;

static void on_sig(int s){ (void)s; g_run = 0; }

static int write_full(int fd, const unsigned char* p, size_t n){
    size_t off = 0;
    while(off < n){
        ssize_t w = write(fd, p + off, n - off);
        if(w > 0){ off += (size_t)w; continue; }
        if(w < 0 && (errno == EINTR)) continue;
        if(w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){ usleep(2000); continue; }
        return -1;
    }
    return 0;
}

static int wr2(int fd, unsigned char a, unsigned char b){
    unsigned char p[2]={a,b};
    return write_full(fd, p, 2);
}

static int sx_write_direct(int bus, int adr, int val){
    if(!g_ctx || g_ctx->fd < 0) return -1;
    (void)bus; // SLX825: single bus
    unsigned char cmd = (unsigned char)(0x80 | (adr & 0x7F));
    unsigned char data = (unsigned char)(val & 0xFF);
    return wr2(g_ctx->fd, cmd, data);
}

static int sx_read_byte_timeout(int fd, int timeout_ms){
    unsigned char b = 0;
    int loops = (timeout_ms > 0) ? timeout_ms : 1;
    for(int i=0;i<loops;++i){
        ssize_t r = read(fd, &b, 1);
        if(r == 1) return (int)b;
        if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return -1;
        usleep(1000);
    }
    return -1;
}

static int sx_read_adr_data_sx0(int adr_expect, int* out_data){
    if(!g_ctx || g_ctx->fd < 0 || !out_data) return -1;

    /* Rautenhaus read request: send ONLY address byte (bit7=0). */
    unsigned char rd = (unsigned char)(adr_expect & 0x7F);
    if(write_full(g_ctx->fd, &rd, 1) != 0) return -1;

    /*
      Interfaces seen in the wild return either:
      A) addr+data pair (a,d)
      B) data-only single byte
      We accept both to stay compatible.
    */

    int first = sx_read_byte_timeout(g_ctx->fd, 20);
    if(first < 0) return -1;

    /* Try pair mode first: if next byte arrives quickly, validate address. */
    int second = sx_read_byte_timeout(g_ctx->fd, 4);
    if(second >= 0){
        int bus = (first & 0x80) ? 1 : 0;
        int adr = (first & 0x7F);
        if(bus == 0 && adr == adr_expect){
            *out_data = second & 0xFF;
            return 0;
        }
        /* Pair did not match requested address; keep scanning a short window. */
        for(int i=0;i<12;++i){
            int a = sx_read_byte_timeout(g_ctx->fd, 4);
            if(a < 0) continue;
            int d = sx_read_byte_timeout(g_ctx->fd, 4);
            if(d < 0) continue;
            int b = (a & 0x80) ? 1 : 0;
            int ad = (a & 0x7F);
            if(b == 0 && ad == adr_expect){
                *out_data = d & 0xFF;
                return 0;
            }
        }
        return -1;
    }

    /* No second byte => treat first as data-only reply. */
    *out_data = first & 0xFF;
    return 0;
}

static int sx_read_track_adr127(int* out_track, int* out_raw){
    int d = 0;
    if(sx_read_adr_data_sx0(127, &d) != 0) return -1;
    if(out_raw) *out_raw = d;
    *out_track = (d >> 7) & 0x01;
    return 0;
}

static int g_last_sx0[112];
static unsigned char g_have_sx0[112];

static void send_snapshot_to_client(int cfd, int bus){
    if(cfd < 0) return;
    int b = (bus==1)?1:0;
    for(int adr=0; adr<112; ++adr){
        if(!g_have_sx0[adr]) continue;
        char line[64];
        snprintf(line, sizeof(line), "FRAME %d %d %d\n", b, adr, g_last_sx0[adr] & 0xFF);
        (void)write(cfd, line, strlen(line));
    }
}

static void clients_init(){ for(int i=0;i<MAX_CLIENTS;++i) clients[i] = -1; }

static void clients_add(int fd){
    for(int i=0;i<MAX_CLIENTS;++i){ if(clients[i] < 0){ clients[i]=fd; return; } }
    close(fd);
}

static void clients_broadcast(const char* msg){
    if(g_suppress_broadcast) return;
    size_t n = strlen(msg);
    for(int i=0;i<MAX_CLIENTS;++i){
        if(clients[i] < 0) continue;
        ssize_t wr = write(clients[i], msg, n);
        if(wr < 0){ close(clients[i]); clients[i] = -1; }
    }
}

static int should_broadcast_frame(int bus, int adr){
    if(!g_quiet_mode) return 1;
    if(bus != 0) return 0;
    return (adr == 19 || adr == 20);
}

static void sx_drain_rx(int ms){
    if(!g_ctx || g_ctx->fd < 0) return;
    unsigned char b;
    int loops = (ms > 0) ? ms : 1;
    for(int i=0;i<loops;++i){
        while(read(g_ctx->fd, &b, 1) == 1){}
        usleep(1000);
    }
}

static void handle_client_cmd(int idx){
    char buf[1024];
    ssize_t n = recv(clients[idx], buf, sizeof(buf)-1, MSG_DONTWAIT);
    if(n == 0){ close(clients[idx]); clients[idx] = -1; return; }
    if(n < 0){ if(errno!=EAGAIN && errno!=EWOULDBLOCK){ close(clients[idx]); clients[idx]=-1; } return; }
    buf[n] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(buf, "\r\n", &saveptr);
    while(line){
        int bus=0, adr=0, val=0;
        if(strncmp(line, "GET_TRACK_DBG", 13) == 0){
        g_suppress_broadcast = 1;
        sx_drain_rx(8);
        int d109 = -1;
        int d0 = -1;
        int tr109 = -1;
        int trk0b7 = -1;
        int trk0b0 = -1;
        if(sx_read_adr_data_sx0(109, &d109) == 0) tr109 = (d109 >> 7) & 0x01;
        if(sx_read_adr_data_sx0(0, &d0) == 0){
            trk0b7 = (d0 >> 7) & 0x01;
            trk0b0 = d0 & 0x01;
        }
        char ans[128];
        snprintf(ans, sizeof(ans), "TRACKDBG ADR109=%d TR109=%d K0=%d K0B7=%d K0B0=%d\n", d109, tr109, d0, trk0b7, trk0b0);
        (void)write(clients[idx], ans, strlen(ans));
        fprintf(stderr, "sxbusd TRACKDBG adr109=%d tr109=%d k0=%d k0b7=%d k0b0=%d\n", d109, tr109, d0, trk0b7, trk0b0);
        g_suppress_broadcast = 0;
    } else if(strncmp(line, "GET_TRACK", 9) == 0){
        g_suppress_broadcast = 1;
        sx_drain_rx(8);
        int tr = -1;
        int raw127 = -1;
        const char* src = "unknown";
        for(int attempt=0; attempt<10; ++attempt){
            if(sx_read_track_adr127(&tr, &raw127) == 0){
                g_last_track = tr;
                src = "ADR127.bit7";
                break;
            }
            usleep(20000);
        }
        char ans[64];
        snprintf(ans, sizeof(ans), "TRACK %d\n", tr);
        (void)write(clients[idx], ans, strlen(ans));
        fprintf(stderr, "sxbusd TRACK source=%s raw127=%d value=%d cache=%d\n", src, raw127, tr, g_last_track);
        g_suppress_broadcast = 0;

    } else if(strncmp(line, "SNAPSHOT", 8) == 0){
        g_suppress_broadcast = 1;
        sx_drain_rx(8);
        send_snapshot_to_client(clients[idx], 0);
        const char* ans = "SNAPSHOT_DONE\n";
        (void)write(clients[idx], ans, strlen(ans));
        g_suppress_broadcast = 0;

    } else if(sscanf(line, "READADR %d %d", &bus, &adr) == 2){
        int d = -1;
        int rc = -1;
        int last_errno = 0;
        g_suppress_broadcast = 1;
        sx_drain_rx(8);
        if(bus == 0 && adr >= 0 && adr < 112){
            for(int attempt=0; attempt<10; ++attempt){
                rc = sx_read_adr_data_sx0(adr, &d);
                if(rc == 0) break;
                last_errno = errno;
                usleep(30000);
            }
        }
        if(rc == 0){
            g_last_sx0[adr] = d & 0xFF;
            g_have_sx0[adr] = 1;
            char fline[64];
            snprintf(fline, sizeof(fline), "FRAME %d %d %d\n", bus, adr, d & 0xFF);
            (void)write(clients[idx], fline, strlen(fline));
            const char* ans = "OK\n";
            (void)write(clients[idx], ans, strlen(ans));
        } else {
            char ans[64];
            snprintf(ans, sizeof(ans), "ERR readadr bus=%d adr=%d errno=%d\n", bus, adr, last_errno ? last_errno : errno);
            (void)write(clients[idx], ans, strlen(ans));
        }
        g_suppress_broadcast = 0;
    } else if(sscanf(line, "WRITE %d %d %d", &bus, &adr, &val) == 3){
        int rc = sx_write_direct(bus, adr, val);
        if(rc==0){
            char fline[64];
            snprintf(fline, sizeof(fline), "FRAME %d %d %d\n", (bus==1)?1:0, adr & 0x7F, val & 0xFF);
            clients_broadcast(fline);
            const char* ans = "OK\n";
            (void)write(clients[idx], ans, strlen(ans));
        } else {
            char ans[64];
            snprintf(ans, sizeof(ans), "ERR write errno=%d\n", errno);
            (void)write(clients[idx], ans, strlen(ans));
        }
    } else {
        const char* ans = "ERR badcmd\n";
        (void)write(clients[idx], ans, strlen(ans));
    }
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
}

static long long now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
}

static void maybe_commit_track_from_stream(int tr, int raw){
    long long now = now_ms();
    if(g_track_candidate != tr){
        g_track_candidate = tr;
        g_track_candidate_count = 1;
        return;
    }
    g_track_candidate_count++;
    if(g_last_track == -1 || (g_track_candidate_count >= 2 && (now - g_track_last_commit_ms) >= 80)){
        if(tr != g_last_track){
            g_last_track = tr;
            g_track_last_commit_ms = now;
            char tline[32];
            snprintf(tline, sizeof(tline), "TRACK %d\n", tr);
            clients_broadcast(tline);
            fprintf(stderr, "sxbusd TRACK stream source=ADR127 raw=%d value=%d debounced=1\n", raw, tr);
        }
    }
}

static void periodic_track_poll(void){
    long long now = now_ms();
    if((now - g_last_track_poll_ms) < 200) return;
    g_last_track_poll_ms = now;

    int tr = -1;
    int raw127 = -1;
    if(sx_read_track_adr127(&tr, &raw127) == 0){
        if(tr != g_last_track){
            g_last_track = tr;
            char tline[32];
            snprintf(tline, sizeof(tline), "TRACK %d\n", tr);
            clients_broadcast(tline);
            fprintf(stderr, "sxbusd TRACK poll source=ADR127.bit7 raw=%d value=%d\n", raw127, tr);
        }
    }

    // Live-poll für alle SX0-Adressen:
    // Einige Setups liefern keinen zuverlässigen spontanen Stream.
    // Daher alle 112 Adressen zyklisch lesen und nur Änderungen broadcasten.
    static long long last_hot_poll_ms = 0;
    long long now_hot = now_ms();
    if(now_hot - last_hot_poll_ms >= 77){
        for(int adr=0; adr<112; ++adr){
            int d = -1;
            if(sx_read_adr_data_sx0(adr, &d) != 0) continue;
            const int v = d & 0xFF;
            if(!g_have_sx0[adr]){
                g_last_sx0[adr] = v;
                g_have_sx0[adr] = 1;
            } else if(g_last_sx0[adr] != v){
                int old = g_last_sx0[adr];
                g_last_sx0[adr] = v;
                char line[64];
                snprintf(line, sizeof(line), "FRAME %d %d %d\n", 0, adr, v);
                if(should_broadcast_frame(0, adr)) clients_broadcast(line);
                fprintf(stderr, "sxbusd ADR poll adr=%d old=%d new=%d\n", adr, old, v);
            }
        }
        last_hot_poll_ms = now_hot;
    }
}

static void on_frame(int bus, int adr, int data, void* u){
    (void)u;
    if(bus == 0 && adr >= 0 && adr < 112){
        g_last_sx0[adr] = data & 0xFF;
        g_have_sx0[adr] = 1;
    }
    if(bus == 0 && adr == 127){
        int tr = (data >> 7) & 0x01;
        maybe_commit_track_from_stream(tr, data);
    }
    if(bus == 0 && (adr == 19 || adr == 20)){
        fprintf(stderr, "sxbusd STREAM adr=%d data=%d\n", adr, data & 0xFF);
    }

    char line[64];
    snprintf(line, sizeof(line), "FRAME %d %d %d\n", bus, adr, data);
    if(should_broadcast_frame(bus, adr)) clients_broadcast(line);
}

static void on_track(int track, void* u){
    (void)u;
    g_last_track = track;
    char line[32];
    snprintf(line, sizeof(line), "TRACK %d\n", track);
    clients_broadcast(line);
}

static int make_server(const char* sock_path){
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(s < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path)-1);
    unlink(sock_path);
    if(bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0){ close(s); return -1; }
    if(listen(s, 8) < 0){ close(s); return -1; }
    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);
    return s;
}

int main(int argc, char** argv){
    const char* serial = argc > 1 ? argv[1] : "/dev/ttyUSB0";
    int baud = argc > 2 ? atoi(argv[2]) : 19200;
    const char* sock = argc > 3 ? argv[3] : SOCK_PATH_DEFAULT;
    const char* profile_arg = argc > 4 ? argv[4] : "slx825_sx0_stream";

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    signal(SIGPIPE, SIG_IGN);

    sx_bus_ctx ctx;
    if(sx_open(&ctx, serial, baud) != 0){ perror("sx_open"); return 1; }

    int profile = SX_PROFILE_SLX825_SX0_STREAM;
    if(strcmp(profile_arg, "slx825_sx0_stream") != 0){
        fprintf(stderr, "sxbusd WARN unknown profile '%s', fallback to slx825_sx0_stream\n", profile_arg);
    }
    if(sx_set_profile(&ctx, profile) != 0){
        fprintf(stderr, "sxbusd ERR profile set failed: %s\n", sx_profile_name(profile));
        sx_close(&ctx);
        return 1;
    }

    if(sx_enable_feedback(&ctx) != 0){ perror("sx_enable_feedback"); sx_close(&ctx); return 1; }
    g_ctx = &ctx;

    int srv = make_server(sock);
    if(srv < 0){ perror("socket_server"); sx_close(&ctx); return 1; }

    clients_init();
    printf("sx_bus_daemon serial=%s baud=%d sock=%s profile=%s init=FE_A0 power_addr=127 bus_switch=off\n",
           serial, baud, sock, sx_profile_name(ctx.profile));

    while(g_run){
        int cfd = accept(srv, NULL, NULL);
        if(cfd >= 0) clients_add(cfd);
        for(int i=0;i<MAX_CLIENTS;++i) if(clients[i]>=0) handle_client_cmd(i);
        int r = sx_poll(&ctx, on_frame, on_track, NULL);
        if(r < 0){ perror("sx_poll"); break; }
        periodic_track_poll();
        usleep(5000);
    }

    close(srv);
    unlink(sock);
    sx_close(&ctx);
    return 0;
}
