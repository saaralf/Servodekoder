#include "mainwindow_v3.h"
#include "connection_panel.h"
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QWidget>

MainWindowV3::MainWindowV3(QWidget* parent): QMainWindow(parent){
    auto* c = new QWidget; setCentralWidget(c);
    auto* l = new QVBoxLayout(c);
    setWindowTitle("SX/RMX Monitor Qt V3");
    sxPanel = new ConnectionPanel("SX", "daemon:///tmp/sxbusd_sx.sock", 19200);
    rmxPanel = new ConnectionPanel("RMX", "daemon:///tmp/sxbusd_rmx.sock", 57600);
    log = new QTextEdit; log->setReadOnly(true);

    auto* sendRow = new QWidget;
    auto* sendL = new QHBoxLayout(sendRow);
    auto* beBox = new QComboBox; beBox->addItems({"SX","RMX"});
    auto* busBox = new QComboBox; busBox->addItems({"0","1"});
    auto* adr = new QSpinBox; adr->setRange(0,127);
    auto* val = new QSpinBox; val->setRange(0,255);
    auto* sendBtn = new QPushButton("Send Test");
    sendL->addWidget(new QLabel("Send:"));
    sendL->addWidget(new QLabel("Backend")); sendL->addWidget(beBox);
    sendL->addWidget(new QLabel("Bus")); sendL->addWidget(busBox);
    sendL->addWidget(new QLabel("Adr")); sendL->addWidget(adr);
    sendL->addWidget(new QLabel("Wert")); sendL->addWidget(val);
    sendL->addWidget(sendBtn);
    l->addWidget(sxPanel);
    l->addWidget(rmxPanel);
    l->addWidget(sendRow);
    l->addWidget(log,1);

    connect(sendBtn,&QPushButton::clicked,this,[this,beBox,busBox,adr,val]{
        BackendKind b = (beBox->currentText()=="SX") ? BackendKind::SX : BackendKind::RMX;
        bool ok = ctrl.send(b, busBox->currentText().toInt(), adr->value(), val->value());
        log->append(QString("SEND %1 b%2 a%3 v%4 -> %5")
            .arg(beBox->currentText())
            .arg(busBox->currentText())
            .arg(adr->value())
            .arg(val->value())
            .arg(ok?"OK":"FAIL"));
    });

    connect(sxPanel,&ConnectionPanel::connectRequested,this,[this](const QString& ep,int b){
        bool ok = ctrl.connectBackend(BackendKind::SX, ep, b);
        log->append(QString("SX connect %1").arg(ok?"OK":"FAIL"));
    });
    connect(rmxPanel,&ConnectionPanel::connectRequested,this,[this](const QString& ep,int b){
        bool ok = ctrl.connectBackend(BackendKind::RMX, ep, b);
        log->append(QString("RMX connect %1").arg(ok?"OK":"FAIL"));
    });
    connect(sxPanel,&ConnectionPanel::disconnectRequested,this,[this]{ ctrl.disconnectBackend(BackendKind::SX); });
    connect(rmxPanel,&ConnectionPanel::disconnectRequested,this,[this]{ ctrl.disconnectBackend(BackendKind::RMX); });

    connect(&ctrl,&DualRuntimeController::connectedChanged,this,[this](BackendKind b,bool on){
        if(b==BackendKind::SX) sxPanel->setConnected(on); else rmxPanel->setConnected(on);
    });
    connect(&ctrl,&DualRuntimeController::trackUpdated,this,[this](BackendKind b,int t){
        if(b==BackendKind::SX) sxPanel->setTrackState(t); else rmxPanel->setTrackState(t);
    });
    connect(&ctrl,&DualRuntimeController::status,this,[this](BackendKind b,const QString& s){
        log->append(QString("%1: %2").arg(b==BackendKind::SX?"SX":"RMX", s));
    });
}
