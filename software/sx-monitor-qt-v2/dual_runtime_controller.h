#pragma once
#include <QObject>
#include <QTimer>
#include "sx_runtime.h"

enum class BackendKind { SX, RMX };

class DualRuntimeController : public QObject {
    Q_OBJECT
public:
    explicit DualRuntimeController(QObject* parent=nullptr);
    bool connectBackend(BackendKind b, const QString& endpoint, int baud);
    void disconnectBackend(BackendKind b);
    bool send(BackendKind b, int bus, int adr, int val);
signals:
    void connectedChanged(BackendKind b, bool on);
    void frameReceived(BackendKind b, int bus, int adr, int val);
    void trackUpdated(BackendKind b, int track);
    void status(BackendKind b, const QString& text);
private:
    SxRuntime sx;
    SxRuntime rmx;
    bool sxOn{false};
    bool rmxOn{false};
    QTimer pollTimer;
};
