#pragma once
#include <QMainWindow>
#include <QString>
#include "dual_runtime_controller.h"
class ConnectionPanel; class QTextEdit;

class MainWindowV3 : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindowV3(QWidget* parent=nullptr);
private:
    void setupDaemonMenu();
    bool systemctlUser(const QStringList& args, QString* output=nullptr);
    bool serviceActive(const QString& service);
    void daemonServiceAction(const QString& service, const QString& action, const QString& label);
    void startRequiredDaemonsIfNeeded();
    void connectBackendFromPanel(BackendKind backend);
    void disconnectBackend(BackendKind backend);

    ConnectionPanel* sxPanel{};
    ConnectionPanel* rmxPanel{};
    QTextEdit* log{};
    DualRuntimeController ctrl;
};
