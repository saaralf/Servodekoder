#define _POSIX_C_SOURCE 200809L

#include "sx_runtime.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
static int wr2(int fd, unsigned char a, unsigned char b){ unsigned char p[2]={a,b}; return ::write(fd,p,2)==2 ? 0 : -1; }

static void cb_frame(int bus, int adr, int data, void *u){
    auto *self = static_cast<SxRuntime*>(u);
    if(self->onFrame) self->onFrame(bus, adr, data);
}
static void cb_track(int tr, void *u){
    auto *self = static_cast<SxRuntime*>(u);
    if(self->onTrack) self->onTrack(tr);
}
}

bool SxRuntime::connectPort(const std::string& port, int baud){
    lastPort_ = port;
    lastBaud_ = baud;
    disconnectPort();
    const std::string prefix = "daemon://";
        if(port.rfind(prefix, 0) == 0){
        std::string sockPath = port.substr(prefix.size());
        if(onStatus) onStatus(std::string("daemon connect try: ") + sockPath);
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if(fd < 0){ if(onStatus) onStatus("daemon socket create failed"); return false; }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path)-1);
        if(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0){
            if(onStatus) onStatus(std::string("daemon connect failed: ") + strerror(errno));
            ::close(fd);
            return false;
        }
        int fl = fcntl(fd, F_GETFL, 0);
        if(fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        daemonFd_ = fd;
        daemonMode_ = true;
        connected_ = true;
        if(onStatus) onStatus(std::string("runtime connected (daemon): ") + sockPath);
        const char* cmdTrack = "GET_TRACK\n";
        ssize_t wr1 = ::write(daemonFd_, cmdTrack, strlen(cmdTrack));
        if(wr1 <= 0 && onStatus) onStatus("daemon init GET_TRACK failed");
        usleep(10000);
        const char* cmdSnapshot = "SNAPSHOT\n";
        ssize_t wr2 = ::write(daemonFd_, cmdSnapshot, strlen(cmdSnapshot));
        if(wr2 <= 0 && onStatus) onStatus("daemon init SNAPSHOT failed");
        usleep(10000);
        const char* initReads[] = {
            "READADR 0 0\n",
            "READADR 0 20\n",
            "READADR 0 109\n"
        };
        for(const char* cmd : initReads){
            ssize_t wr = ::write(daemonFd_, cmd, strlen(cmd));
            if(wr <= 0 && onStatus) onStatus(std::string("daemon init failed: ") + cmd);
            usleep(5000);
        }
        return true;
    }

    if(sx_open(&ctx_, port.c_str(), baud) != 0){ if(onStatus) onStatus("sx_open failed"); return false; }
    if(sx_enable_feedback(&ctx_) != 0){ if(onStatus) onStatus("sx_enable_feedback failed"); sx_close(&ctx_); return false; }
    connected_ = true;
    daemonMode_ = false;
    if(onStatus) onStatus("runtime connected");
    return true;
}

void SxRuntime::disconnectPort(){
    if(!connected_) return;
    if(daemonMode_){
        if(daemonFd_ >= 0) ::close(daemonFd_);
        daemonFd_ = -1;
        rxBuf_.clear();
    } else {
        sx_close(&ctx_);
    }
    connected_ = false;
    daemonMode_ = false;
}

int SxRuntime::poll(){
    if(!connected_){
        if(!lastPort_.empty() && lastPort_.rfind("daemon://", 0) == 0){
            auto now = std::chrono::steady_clock::now();
            if(lastReconnectTry_.time_since_epoch().count() == 0 || now - lastReconnectTry_ > std::chrono::seconds(2)){
                lastReconnectTry_ = now;
                if(connectPort(lastPort_, lastBaud_)){
                    if(onStatus) onStatus("daemon reconnected");
                    return 0;
                }
            }
        }
        return -1;
    }
    if(daemonMode_) return pollDaemon();

    int n = sx_poll(&ctx_, cb_frame, cb_track, this);
    if(n > 0){
        idle_polls_ = 0;
        return n;
    }
    if(n < 0){
        if(onStatus) onStatus("poll error");
        return n;
    }

    idle_polls_++;
    if(idle_polls_ >= 40){
        idle_polls_ = 0;
        if(sx_enable_feedback(&ctx_) == 0){
            if(onStatus) onStatus("rearm feedback (ADR126=160)");
        } else {
            if(onStatus) onStatus("rearm failed");
        }
    }
    return 0;
}

int SxRuntime::pollDaemon(){
    char buf[2048];
    const ssize_t n = ::read(daemonFd_, buf, sizeof(buf));
    if(n == 0){
        if(onStatus) onStatus("daemon disconnected");
        if(daemonFd_ >= 0) ::close(daemonFd_);
        daemonFd_ = -1;
        connected_ = false;
        daemonMode_ = false;
        rxBuf_.clear();
        return -1;
    }
    if(n < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        if(onStatus) onStatus(std::string("daemon read error: ") + strerror(errno));
        return -1;
    }
    rxBuf_.append(buf, static_cast<size_t>(n));

    size_t pos = 0;
    while(true){
        size_t nl = rxBuf_.find('\n', pos);
        if(nl == std::string::npos) break;
        std::string line = rxBuf_.substr(pos, nl-pos);
        int bus=0, adr=0, data=0, tr=0;
        if(sscanf(line.c_str(), "FRAME %d %d %d", &bus, &adr, &data) == 3){
            if(onStatus && (adr == 10 || adr == 11 || adr == 12 || adr == 13 || adr == 14 || adr == 126 || adr == 127)) onStatus(std::string("daemon rx: ") + line);
            if(onFrame) onFrame(bus, adr, data);
        } else if(sscanf(line.c_str(), "TRACK %d", &tr) == 1){
            if(onStatus) onStatus(std::string("daemon rx: ") + line);
            if(tr >= 0 && onTrack) onTrack(tr);
            else if(tr < 0 && onStatus) onStatus("track unknown (-1), wait for ADR127 stream");
        } else if(line == "OK") {
            if(onStatus) onStatus("daemon ack: OK");
        } else if(line.rfind("ERR", 0) == 0) {
            if(onStatus) onStatus(std::string("daemon ack: ") + line);
        } else if(!line.empty()) {
            if(onStatus) onStatus(std::string("daemon rx: ") + line);
        }
        pos = nl + 1;
    }
    if(pos > 0) rxBuf_.erase(0, pos);
    return 0;
}

bool SxRuntime::send(int bus, int adr, int val){
    if(daemonMode_){
        if(!connected_ || daemonFd_ < 0) return false;
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "WRITE %d %d %d\n", bus, adr, val);
        if(onStatus) onStatus(std::string("daemon tx: ") + cmd);
        ssize_t wr = ::write(daemonFd_, cmd, strlen(cmd));
        if(wr <= 0){ if(onStatus) onStatus("daemon write failed"); return false; }
        return true;
    }
    if(!connected_ || ctx_.fd < 0) return false;
    unsigned char sel = (bus==1)?0xB1:0xB0;
    if(wr2(ctx_.fd,0xFE,sel)!=0) return false;
    usleep(4000);
    unsigned char cmd = (unsigned char)(0x80 | (adr & 0x7F));
    unsigned char data = (unsigned char)(val & 0xFF);
    return wr2(ctx_.fd, cmd, data)==0;
}

bool SxRuntime::readAdr(int bus, int adr){
    if(daemonMode_){
        if(!connected_ || daemonFd_ < 0) return false;
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "READADR %d %d\n", bus, adr);
        if(onStatus) onStatus(std::string("daemon tx: ") + cmd);
        ssize_t wr = ::write(daemonFd_, cmd, strlen(cmd));
        if(wr <= 0){ if(onStatus) onStatus("daemon read request failed"); return false; }
        return true;
    }
    if(!connected_ || ctx_.fd < 0) return false;
    unsigned char sel = (bus==1)?0xB1:0xB0;
    if(wr2(ctx_.fd,0xFE,sel)!=0) return false;
    usleep(4000);
    unsigned char cmd = (unsigned char)(adr & 0x7F);
    return ::write(ctx_.fd, &cmd, 1) == 1;
}
