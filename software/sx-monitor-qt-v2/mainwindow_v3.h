#pragma once
#include <QMainWindow>
#include "dual_runtime_controller.h"
class ConnectionPanel; class QTextEdit;

class MainWindowV3 : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindowV3(QWidget* parent=nullptr);
private:
    ConnectionPanel* sxPanel{};
    ConnectionPanel* rmxPanel{};
    QTextEdit* log{};
    DualRuntimeController ctrl;
};
