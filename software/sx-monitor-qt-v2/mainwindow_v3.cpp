#include "mainwindow_v3.h"
#include "connection_panel.h"
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QTabWidget>
#include <QTableWidget>
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

    auto* sendBlock = new QWidget;
    auto* sendBlockL = new QVBoxLayout(sendBlock);
    auto* sendRow = new QWidget;
    auto* sendL = new QHBoxLayout(sendRow);
    auto* opsRow = new QWidget;
    auto* opsL = new QHBoxLayout(opsRow);
    auto* beBox = new QComboBox; beBox->addItems({"SX","RMX"});
    auto* busBox = new QComboBox; busBox->addItems({"0","1"});
    auto* adr = new QSpinBox; adr->setRange(0,127);
    auto* val = new QSpinBox; val->setRange(0,255);
    auto* sendBtn = new QPushButton("Send Test");
    auto* readBtn = new QPushButton("Read Test");
    auto* autoBtn = new QPushButton("Auto Read 126/127");
    auto* autoWrBtn = new QPushButton("Auto Write+Read 126");
    auto* sxWrBtn = new QPushButton("SX WR126");
    auto* rmxWrBtn = new QPushButton("RMX WR126");
    auto* pSet128Btn = new QPushButton("Preset A126=128");
    auto* pSet0Btn = new QPushButton("Preset A126=0");
    auto* pRead126Btn = new QPushButton("Preset Read126");
    auto* pRead127Btn = new QPushButton("Preset Read127");
    auto* rx126Only = new QCheckBox("RX nur 126/127");
    auto* clearLogBtn = new QPushButton("Log löschen");
    auto* tabs = new QTabWidget;
    auto* sxTab = new QWidget;
    auto* rmxTab = new QWidget;
    auto* sxTabL = new QVBoxLayout(sxTab);
    auto* rmxTabL = new QVBoxLayout(rmxTab);
    auto* sxTable = new QTableWidget(128,3);
    auto* rmxTable = new QTableWidget(128,3);
    sxTable->setHorizontalHeaderLabels({"Adr","SX0","SX1"});
    rmxTable->setHorizontalHeaderLabels({"Adr","RMX0","RMX1"});
    for(int a=0;a<128;++a){
        sxTable->setItem(a,0,new QTableWidgetItem(QString::number(a)));
        sxTable->setItem(a,1,new QTableWidgetItem("-"));
        sxTable->setItem(a,2,new QTableWidgetItem("-"));
        rmxTable->setItem(a,0,new QTableWidgetItem(QString::number(a)));
        rmxTable->setItem(a,1,new QTableWidgetItem("-"));
        rmxTable->setItem(a,2,new QTableWidgetItem("-"));
    }
    sxTabL->addWidget(sxTable);
    rmxTabL->addWidget(rmxTable);
    tabs->addTab(sxTab, "SX Monitor");
    tabs->addTab(rmxTab, "RMX Monitor");

    sendL->addWidget(new QLabel("Send:"));
    sendL->addWidget(new QLabel("Backend")); sendL->addWidget(beBox);
    sendL->addWidget(new QLabel("Bus")); sendL->addWidget(busBox);
    sendL->addWidget(new QLabel("Adr")); sendL->addWidget(adr);
    sendL->addWidget(new QLabel("Wert")); sendL->addWidget(val);
    sendL->addWidget(sendBtn);
    sendL->addWidget(readBtn);

    opsL->addWidget(new QLabel("Auto:"));
    opsL->addWidget(autoBtn);
    opsL->addWidget(autoWrBtn);
    opsL->addSpacing(8);
    opsL->addWidget(new QLabel("Single:"));
    opsL->addWidget(sxWrBtn);
    opsL->addWidget(rmxWrBtn);
    opsL->addSpacing(8);
    opsL->addWidget(new QLabel("Preset:"));
    opsL->addWidget(pSet128Btn);
    opsL->addWidget(pSet0Btn);
    opsL->addWidget(pRead126Btn);
    opsL->addWidget(pRead127Btn);
    opsL->addStretch(1);
    opsL->addWidget(rx126Only);
    opsL->addWidget(clearLogBtn);

    sendBlockL->addWidget(sendRow);
    sendBlockL->addWidget(opsRow);

    l->addWidget(sxPanel);
    l->addWidget(rmxPanel);
    l->addWidget(sendBlock);
    l->addWidget(tabs,1);
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
    connect(readBtn,&QPushButton::clicked,this,[this,beBox,busBox,adr]{
        BackendKind b = (beBox->currentText()=="SX") ? BackendKind::SX : BackendKind::RMX;
        bool ok = ctrl.readAdr(b, busBox->currentText().toInt(), adr->value());
        log->append(QString("READ %1 b%2 a%3 -> %4")
            .arg(beBox->currentText())
            .arg(busBox->currentText())
            .arg(adr->value())
            .arg(ok?"OK":"FAIL"));
    });
    connect(autoBtn,&QPushButton::clicked,this,[this]{
        struct T{BackendKind b; const char* n;};
        T backends[]={{BackendKind::SX,"SX"},{BackendKind::RMX,"RMX"}};
        for(const auto& x: backends){
            bool ok126 = ctrl.readAdr(x.b, 0, 126);
            bool ok127 = ctrl.readAdr(x.b, 0, 127);
            log->append(QString("AUTO %1 READ b0 a126=%2 a127=%3")
                .arg(x.n)
                .arg(ok126?"OK":"FAIL")
                .arg(ok127?"OK":"FAIL"));
        }
    });
    connect(autoWrBtn,&QPushButton::clicked,this,[this]{
        struct T{BackendKind b; const char* n;};
        T backends[]={{BackendKind::SX,"SX"},{BackendKind::RMX,"RMX"}};
        for(const auto& x: backends){
            bool w1 = ctrl.send(x.b, 0, 126, 128);
            bool r1 = ctrl.readAdr(x.b, 0, 126);
            bool w0 = ctrl.send(x.b, 0, 126, 0);
            bool r0 = ctrl.readAdr(x.b, 0, 126);
            log->append(QString("AUTO %1 WR126 set128=%2 read=%3 reset0=%4 read=%5")
                .arg(x.n)
                .arg(w1?"OK":"FAIL")
                .arg(r1?"OK":"FAIL")
                .arg(w0?"OK":"FAIL")
                .arg(r0?"OK":"FAIL"));
        }
    });
    connect(sxWrBtn,&QPushButton::clicked,this,[this]{
        bool w1 = ctrl.send(BackendKind::SX, 0, 126, 128);
        bool r1 = ctrl.readAdr(BackendKind::SX, 0, 126);
        bool w0 = ctrl.send(BackendKind::SX, 0, 126, 0);
        bool r0 = ctrl.readAdr(BackendKind::SX, 0, 126);
        log->append(QString("SX WR126 set128=%1 read=%2 reset0=%3 read=%4")
            .arg(w1?"OK":"FAIL").arg(r1?"OK":"FAIL").arg(w0?"OK":"FAIL").arg(r0?"OK":"FAIL"));
    });
    connect(rmxWrBtn,&QPushButton::clicked,this,[this]{
        bool w1 = ctrl.send(BackendKind::RMX, 0, 126, 128);
        bool r1 = ctrl.readAdr(BackendKind::RMX, 0, 126);
        bool w0 = ctrl.send(BackendKind::RMX, 0, 126, 0);
        bool r0 = ctrl.readAdr(BackendKind::RMX, 0, 126);
        log->append(QString("RMX WR126 set128=%1 read=%2 reset0=%3 read=%4")
            .arg(w1?"OK":"FAIL").arg(r1?"OK":"FAIL").arg(w0?"OK":"FAIL").arg(r0?"OK":"FAIL"));
    });
    connect(pSet128Btn,&QPushButton::clicked,this,[adr,val]{ adr->setValue(126); val->setValue(128); });
    connect(pSet0Btn,&QPushButton::clicked,this,[adr,val]{ adr->setValue(126); val->setValue(0); });
    connect(pRead126Btn,&QPushButton::clicked,this,[adr]{ adr->setValue(126); });
    connect(pRead127Btn,&QPushButton::clicked,this,[adr]{ adr->setValue(127); });
    connect(clearLogBtn,&QPushButton::clicked,this,[this]{ log->clear(); });

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
    connect(&ctrl,&DualRuntimeController::frameReceived,this,[this,rx126Only,sxTable,rmxTable](BackendKind b,int bus,int adr,int val){
        if(rx126Only->isChecked() && !(adr==126 || adr==127)) return;
        log->append(QString("RX %1 b%2 a%3 v%4")
            .arg(b==BackendKind::SX?"SX":"RMX")
            .arg(bus).arg(adr).arg(val));
        if(adr>=0 && adr<128 && (bus==0 || bus==1)){
            QTableWidget* t = (b==BackendKind::SX)?sxTable:rmxTable;
            t->item(adr,bus+1)->setText(QString::number(val));
        }
    });
}
