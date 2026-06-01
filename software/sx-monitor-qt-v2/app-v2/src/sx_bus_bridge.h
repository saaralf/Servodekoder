#pragma once

#include <functional>
#include <string>

extern "C" {
#include "sx_bus_core.h"
}

class SxBusBridge {
public:
    bool open(const char* port, int baud);
    void close();
    int poll();

    std::function<void(int,int,int)> onFrame;
    std::function<void(int)> onTrack;

private:
    sx_bus_ctx ctx_{};
    bool opened_{false};
    bool daemonMode_{false};
    int daemonFd_{-1};
    std::string rxBuf_;

    int pollDaemon();

    static void frameCb(int bus, int adr, int data, void* user);
    static void trackCb(int track, void* user);
};
