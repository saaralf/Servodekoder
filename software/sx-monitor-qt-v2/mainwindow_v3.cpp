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
#include <QHeaderView>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QWidget>
#include <array>
#include <memory>

static QString bits8(int v){
    QString s; s.reserve(8);
    for(int i=7;i>=0;--i) s.append((v & (1<<i)) ? '1' : '0');
    return s;
}

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
    auto* sxBusSel = new QComboBox; sxBusSel->addItems({"SX0","SX1"});
    auto* rmxBusSel = new QComboBox; rmxBusSel->addItems({"RMX0","RMX1"});
    auto* sxTable = new QTableWidget(28,12);
    auto* rmxTable = new QTableWidget(28,12);
    sxTable->setHorizontalHeaderLabels({"Adr","Wert","Bits","Adr","Wert","Bits","Adr","Wert","Bits","Adr","Wert","Bits"});
    rmxTable->setHorizontalHeaderLabels({"Adr","Wert","Bits","Adr","Wert","Bits","Adr","Wert","Bits","Adr","Wert","Bits"});
    for(int adr=0; adr<112; ++adr){
        int row = adr % 28;
        int blk = adr / 28;
        int base = blk * 3;
        sxTable->setItem(row,base+0,new QTableWidgetItem(QString::number(adr)));
        sxTable->setItem(row,base+1,new QTableWidgetItem("-"));
        sxTable->setItem(row,base+2,new QTableWidgetItem("--------"));
        rmxTable->setItem(row,base+0,new QTableWidgetItem(QString::number(adr)));
        rmxTable->setItem(row,base+1,new QTableWidgetItem("-"));
        rmxTable->setItem(row,base+2,new QTableWidgetItem("--------"));
    }
    for(auto* t : {sxTable, rmxTable}){
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        t->verticalHeader()->setVisible(false);
        t->setAlternatingRowColors(true);
        t->verticalHeader()->setDefaultSectionSize(22);
        for(int adr=0; adr<112; ++adr){
            int row = adr % 28;
            int blk = adr / 28;
            int base = blk * 3;
            for(int c=base; c<base+3; ++c){
                t->item(row,c)->setForeground(QBrush(Qt::black));
                t->item(row,c)->setBackground(QBrush(Qt::white));
            }
        }
        for(int blk=0; blk<4; ++blk){
            int base=blk*3;
            t->setColumnWidth(base+0, 42);
            t->setColumnWidth(base+1, 48);
            t->setColumnWidth(base+2, 86);
        }
        t->setStyleSheet(
            "QTableWidget { background: white; color: black; gridline-color: #cfcfcf; alternate-background-color: #fafafa; }"
            "QHeaderView::section { background: #f2f2f2; color: black; padding: 4px; font-weight: 600; }"
        );
    }
    sxTabL->addWidget(sxTable);
    rmxTabL->addWidget(rmxTable);
    sxTabL->insertWidget(0, sxBusSel);
    rmxTabL->insertWidget(0, rmxBusSel);
    tabs->addTab(sxTab, "SX Monitor");
    tabs->addTab(rmxTab, "RMX Monitor");

    auto sxVals = std::make_shared<std::array<std::array<int,112>,2>>();
    auto rmxVals = std::make_shared<std::array<std::array<int,112>,2>>();
    for(auto &b:*sxVals) b.fill(-1);
    for(auto &b:*rmxVals) b.fill(-1);
    auto repaintTable = [sxTable,rmxTable,sxBusSel,rmxBusSel,sxVals,rmxVals](BackendKind bk){
        auto* t = (bk==BackendKind::SX)?sxTable:rmxTable;
        int bus = (bk==BackendKind::SX)?sxBusSel->currentIndex():rmxBusSel->currentIndex();
        auto& vals = (bk==BackendKind::SX)?*sxVals:*rmxVals;
        for(int adr=0; adr<=111; ++adr){
            int row=adr%28, blk=adr/28, base=blk*3;
            int v=vals[bus][adr];
            t->item(row,base+1)->setText(v<0?"-":QString::number(v));
            t->item(row,base+2)->setText(v<0?"--------":bits8(v));
        }
    };
    connect(sxBusSel, qOverload<int>(&QComboBox::currentIndexChanged), this, [repaintTable](int){ repaintTable(BackendKind::SX); });
    connect(rmxBusSel, qOverload<int>(&QComboBox::currentIndexChanged), this, [repaintTable](int){ repaintTable(BackendKind::RMX); });

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
    connect(&ctrl,&DualRuntimeController::frameReceived,this,[this,rx126Only,sxTable,rmxTable,sxVals,rmxVals,sxBusSel,rmxBusSel](BackendKind b,int bus,int adr,int val){
        if(rx126Only->isChecked() && !(adr==126 || adr==127)) return;
        log->append(QString("RX %1 b%2 a%3 v%4")
            .arg(b==BackendKind::SX?"SX":"RMX")
            .arg(bus).arg(adr).arg(val));
        if(adr>=0 && adr<=111 && (bus==0 || bus==1)){
            QTableWidget* t = (b==BackendKind::SX)?sxTable:rmxTable;
            auto vals = (b==BackendKind::SX)?sxVals:rmxVals;
            (*vals)[bus][adr] = val;
            int row = adr % 28;
            int blk = adr / 28;
            int base = blk*3;
            int selBus = (b==BackendKind::SX)?sxBusSel->currentIndex():rmxBusSel->currentIndex();
            if(selBus != bus) return;
            QString nv = QString::number(val);
            if(t->item(row,base+1)->text() != nv){
                t->item(row,base+0)->setBackground(QColor(255,245,170));
                t->item(row,base+1)->setBackground(QColor(255,245,170));
                t->item(row,base+2)->setBackground(QColor(255,245,170));
            }
            t->item(row,base+1)->setText(nv);
            t->item(row,base+2)->setText(bits8(val));
        }
    });
}
