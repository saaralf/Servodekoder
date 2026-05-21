#include "sx_bus_bridge.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

bool SxBusBridge::open(const char* port, int baud){
    if (opened_) close();

    const char* prefix = "daemon://";
    if (port && strncmp(port, prefix, strlen(prefix)) == 0) {
        const char* sockPath = port + strlen(prefix);
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return false;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path)-1);
        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(fd);
            return false;
        }
        daemonFd_ = fd;
        daemonMode_ = true;
        opened_ = true;
        return true;
    }

    if (sx_open(&ctx_, port, baud) != 0) return false;
    if (sx_enable_feedback(&ctx_) != 0) { sx_close(&ctx_); return false; }
    opened_ = true;
    daemonMode_ = false;
    return true;
}

void SxBusBridge::close(){
    if (!opened_) return;
    if (daemonMode_) {
        if (daemonFd_ >= 0) ::close(daemonFd_);
        daemonFd_ = -1;
        rxBuf_.clear();
    } else {
        sx_close(&ctx_);
    }
    opened_ = false;
    daemonMode_ = false;
}

int SxBusBridge::poll(){
    if (!opened_) return -1;
    if (daemonMode_) return pollDaemon();
    return sx_poll(&ctx_, &SxBusBridge::frameCb, &SxBusBridge::trackCb, this);
}

int SxBusBridge::pollDaemon(){
    char buf[1024];
    const ssize_t n = ::read(daemonFd_, buf, sizeof(buf));
    if (n <= 0) return (n == 0) ? -1 : 0;
    rxBuf_.append(buf, static_cast<size_t>(n));

    size_t pos = 0;
    while (true) {
        size_t nl = rxBuf_.find('\n', pos);
        if (nl == std::string::npos) break;
        std::string line = rxBuf_.substr(pos, nl - pos);
        int bus=0, adr=0, data=0, tr=0;
        if (sscanf(line.c_str(), "FRAME %d %d %d", &bus, &adr, &data) == 3) {
            if (onFrame) onFrame(bus, adr, data);
        } else if (sscanf(line.c_str(), "TRACK %d", &tr) == 1) {
            if (onTrack) onTrack(tr);
        }
        pos = nl + 1;
    }
    if (pos > 0) rxBuf_.erase(0, pos);
    return 0;
}

void SxBusBridge::frameCb(int bus, int adr, int data, void* user){
    auto* self = static_cast<SxBusBridge*>(user);
    if (self && self->onFrame) self->onFrame(bus, adr, data);
}

void SxBusBridge::trackCb(int track, void* user){
    auto* self = static_cast<SxBusBridge*>(user);
    if (self && self->onTrack) self->onTrack(track);
}
