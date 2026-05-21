#pragma once

#include <functional>
#include <string>
#include <chrono>

extern "C" {
#include "sx_bus_core.h"
}

class SxRuntime {
public:
    bool connectPort(const std::string& port, int baud);
    void disconnectPort();
    int poll();
    bool send(int bus, int adr, int val);

    std::function<void(int,int,int)> onFrame;
    std::function<void(int)> onTrack;
    std::function<void(const std::string&)> onStatus;

private:
    sx_bus_ctx ctx_{};
    bool connected_ = false;
    bool daemonMode_ = false;
    int daemonFd_ = -1;
    std::string rxBuf_;
    int idle_polls_ = 0;

    std::string lastPort_;
    int lastBaud_ = 57600;
    std::chrono::steady_clock::time_point lastReconnectTry_{};

    int pollDaemon();
};
