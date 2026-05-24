#include "dual_runtime_controller.h"

DualRuntimeController::DualRuntimeController(QObject* parent): QObject(parent){
    sx.onFrame = [this](int bus,int adr,int val){ emit frameReceived(BackendKind::SX,bus,adr,val); };
    rmx.onFrame = [this](int bus,int adr,int val){ emit frameReceived(BackendKind::RMX,bus,adr,val); };
    sx.onTrack = [this](int t){ emit trackUpdated(BackendKind::SX, t); };
    rmx.onTrack = [this](int t){ emit trackUpdated(BackendKind::RMX, t); };
    sx.onStatus = [this](const std::string& s){ emit status(BackendKind::SX, QString::fromStdString(s)); };
    rmx.onStatus = [this](const std::string& s){ emit status(BackendKind::RMX, QString::fromStdString(s)); };
    connect(&pollTimer,&QTimer::timeout,this,[this]{ if(sxOn) sx.poll(); if(rmxOn) rmx.poll(); });
    pollTimer.start(25);
}

bool DualRuntimeController::connectBackend(BackendKind b, const QString& endpoint, int baud){
    if(b==BackendKind::SX){
        sx.disconnectPort();
        sxOn = sx.connectPort(endpoint.toStdString(), baud);
        emit connectedChanged(b, sxOn);
        return sxOn;
    }
    rmx.disconnectPort();
    rmxOn = rmx.connectPort(endpoint.toStdString(), baud);
    emit connectedChanged(b, rmxOn);
    return rmxOn;
}

void DualRuntimeController::disconnectBackend(BackendKind b){
    if(b==BackendKind::SX){ sx.disconnectPort(); sxOn=false; emit connectedChanged(b,false); return; }
    rmx.disconnectPort(); rmxOn=false; emit connectedChanged(b,false);
}

bool DualRuntimeController::send(BackendKind b, int bus, int adr, int val){
    return (b==BackendKind::SX) ? sx.send(bus,adr,val) : rmx.send(bus,adr,val);
}

bool DualRuntimeController::readAdr(BackendKind b, int bus, int adr){
    return (b==BackendKind::SX) ? sx.readAdr(bus,adr) : rmx.readAdr(bus,adr);
}
