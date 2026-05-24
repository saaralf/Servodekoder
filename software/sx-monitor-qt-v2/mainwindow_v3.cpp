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
#include <QGroupBox>
#include <QGridLayout>
#include <QProgressBar>
#include <vector>
#include <array>
#include <memory>
#include <QPainter>
#include <QPixmap>
#include <QCoreApplication>
#include <QScrollArea>

class ServoArmWidget : public QWidget {
public:
    explicit ServoArmWidget(QWidget* parent=nullptr): QWidget(parent) {
        setMinimumSize(150,150);
        QString base = QCoreApplication::applicationDirPath() + "/../assets/";
        body = QPixmap(base + "servo_body_blue.png");
        arm = QPixmap(base + "servo_arm_new.png");
        bodyHubX = 624.7 / 1254.0;
        bodyHubY = 361.0 / 1254.0;
        armHubX  = 0.5;
        armHubY  = 0.5;
    }
    void setAngleDeg(int a){ angle = qBound(-90, a, 90); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing,true);
        p.fillRect(rect(), palette().color(QPalette::Window));
        QRectF view = rect().adjusted(6, 6, -6, -6);
        double base = 1000.0;
        QRectF bodyR(0, 0, base, base);
        QPointF c(bodyHubX*base, bodyHubY*base);
        double armSide = base * 1.20;
        QRectF armR(c.x() - armHubX*armSide, c.y() - armHubY*armSide, armSide, armSide);
        double halfDiag = (armSide * 0.5) * 1.41421356237;
        QRectF armBound(c.x()-halfDiag, c.y()-halfDiag, 2*halfDiag, 2*halfDiag);
        QRectF scene = bodyR.united(armBound);
        double s = qMin(view.width()/scene.width(), view.height()/scene.height());
        QPointF t(view.center().x() - s*scene.center().x(), view.center().y() - s*scene.center().y());
        p.save(); p.translate(t); p.scale(s, s);
        if(!body.isNull()) p.drawPixmap(bodyR.toRect(), body);
        if(!arm.isNull()){
            p.save(); p.translate(c); p.rotate((double)-angle - 90.0); p.translate(-c);
            p.drawPixmap(armR.toRect(), arm); p.restore();
        }
        p.restore();
        p.setPen(QPen(QColor(30,30,30),1));
        p.drawText(QRect(0,0,width(),20), Qt::AlignCenter, QString("%1°").arg(angle));
    }
private:
    int angle = 0;
    QPixmap body, arm;
    double bodyHubX = 0.5, bodyHubY = 0.3, armHubX = 0.5, armHubY = 0.5;
};

static QString bits8(int v){
    QString s; s.reserve(8);
    for(int i=7;i>=0;--i) s.append((v & (1<<i)) ? '1' : '0');
    return s;
}

MainWindowV3::MainWindowV3(QWidget* parent): QMainWindow(parent){
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    setCentralWidget(scroll);
    auto* c = new QWidget;
    scroll->setWidget(c);
    auto* l = new QVBoxLayout(c);
    setWindowTitle("SX/RMX Monitor Qt V3");
    sxPanel = new ConnectionPanel("SX", "daemon:///run/user/1000/sxbusd.sock", 19200);
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
    auto* servoTab = new QWidget;
    auto* visualTab = new QWidget;
    auto* sxTabL = new QVBoxLayout(sxTab);
    auto* rmxTabL = new QVBoxLayout(rmxTab);
    auto* servoL = new QVBoxLayout(servoTab);
    auto* visualL = new QVBoxLayout(visualTab);
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
        t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const int tableH = t->horizontalHeader()->height() + t->rowCount() * t->verticalHeader()->defaultSectionSize() + 8;
        t->setMinimumHeight(tableH);
        t->setMaximumHeight(tableH);
    }
    sxTabL->addWidget(sxTable);
    rmxTabL->addWidget(rmxTable);
    sxTabL->insertWidget(0, sxBusSel);
    rmxTabL->insertWidget(0, rmxBusSel);
    tabs->addTab(sxTab, "SX Monitor");
    tabs->addTab(rmxTab, "RMX Monitor");

    auto* r1 = new QHBoxLayout;
    auto* progAddrA = new QSpinBox; progAddrA->setRange(1,111);
    auto* progAddrB = new QSpinBox; progAddrB->setRange(0,111);
    auto* progOnBtn = new QPushButton("Prog EIN (Track=0)");
    auto* progOffBtn = new QPushButton("Prog AUS (Track=1)");
    r1->addWidget(new QLabel("AddrA")); r1->addWidget(progAddrA);
    r1->addWidget(new QLabel("AddrB")); r1->addWidget(progAddrB);
    r1->addWidget(progOnBtn); r1->addWidget(progOffBtn);
    servoL->addLayout(r1);

    auto* r2 = new QHBoxLayout;
    auto* progServoIdx = new QSpinBox; progServoIdx->setRange(1,16);
    auto* progStep = new QComboBox; progStep->addItems({"1","2","5","10","20"}); progStep->setCurrentText("5");
    auto* progMoveMinusBtn = new QPushButton("-");
    auto* progMovePlusBtn = new QPushButton("+");
    auto* progMidBtn = new QPushButton("Mitte");
    auto* progStoreLBtn = new QPushButton("L speichern");
    auto* progStoreRBtn = new QPushButton("R speichern");
    auto* progStartBtn = new QPushButton("Setup START (K10=1)");
    auto* progSaveBtn = new QPushButton("Setup SAVE+ENDE (K10=3)");
    auto* progAbortBtn = new QPushButton("Setup ABBRUCH (K10=2)");
    r2->addWidget(new QLabel("Servo")); r2->addWidget(progServoIdx);
    r2->addWidget(new QLabel("Schritt")); r2->addWidget(progStep);
    r2->addWidget(progMoveMinusBtn); r2->addWidget(progMovePlusBtn); r2->addWidget(progMidBtn);
    r2->addWidget(progStoreLBtn); r2->addWidget(progStoreRBtn);
    r2->addWidget(progStartBtn); r2->addWidget(progSaveBtn); r2->addWidget(progAbortBtn);
    servoL->addLayout(r2);

    auto* servoTable = new QTableWidget(16,6);
    servoTable->setHorizontalHeaderLabels({"Servo","zero","relMin(v+90)","relMax(v+90)","divLeft","Action"});
    servoTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    servoTable->setColumnWidth(0,52); servoTable->setColumnWidth(1,64); servoTable->setColumnWidth(2,92);
    servoTable->setColumnWidth(3,92); servoTable->setColumnWidth(4,64); servoTable->setColumnWidth(5,280);
    for(int s=0;s<16;++s){
        servoTable->setItem(s,0,new QTableWidgetItem(QString::number(s+1)));
        servoTable->setItem(s,1,new QTableWidgetItem("90"));
        servoTable->setItem(s,2,new QTableWidgetItem("50"));
        servoTable->setItem(s,3,new QTableWidgetItem("130"));
        servoTable->setItem(s,4,new QTableWidgetItem("1"));
        auto *w = new QWidget; auto *hl = new QHBoxLayout(w); hl->setContentsMargins(0,0,0,0);
        auto *bMid=new QPushButton("Mitte"); auto *bG=new QPushButton("Gerade"); auto *bA=new QPushButton("Abzweig"); auto *bC=new QPushButton("Commit");
        hl->addWidget(bMid); hl->addWidget(bG); hl->addWidget(bA); hl->addWidget(bC);
        servoTable->setCellWidget(s,5,w);
    }
    servoL->addWidget(servoTable);
    servoL->addWidget(new QLabel("Wizard: K10 Start/Save/Abort, K11 Servo, K12 Schritt, K13 Move, K14 L/R speichern"));
    tabs->addTab(servoTab, "Servo-Programmer");

    auto* visualAddrA = new QSpinBox; visualAddrA->setRange(1,111); visualAddrA->setValue(progAddrA->value()); visualAddrA->setFixedWidth(90);
    auto* visualAddrB = new QSpinBox; visualAddrB->setRange(0,111); visualAddrB->setValue(progAddrB->value()); visualAddrB->setFixedWidth(90);
    auto* visualBitOrder = new QCheckBox("Bits links->rechts (Bit1 links)"); visualBitOrder->setChecked(true);
    auto* visualLimitSpin = new QSpinBox; visualLimitSpin->setRange(30,45); visualLimitSpin->setValue(40);

    auto* addrArow = new QHBoxLayout;
    addrArow->setContentsMargins(0,0,0,0);
    addrArow->setSpacing(6);
    addrArow->addWidget(new QLabel("Adresse 1 (obere Reihe):"));
    addrArow->addWidget(visualAddrA);
    addrArow->addSpacing(10);
    addrArow->addWidget(visualBitOrder);
    addrArow->addStretch(1);
    visualL->addLayout(addrArow);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    std::vector<QLabel*> visualInfo(16, nullptr);
    std::vector<ServoArmWidget*> visualArm(16, nullptr);
    auto visualAngle = std::make_shared<std::array<int,16>>();
    visualAngle->fill(0);

    auto mkHdr = [visualBitOrder,visualAddrA,visualAddrB](int servoIdx)->QLabel*{
        int bit = (servoIdx % 8) + 1;
        int shown = (visualBitOrder && visualBitOrder->isChecked()) ? bit : (9-bit);
        int adr = (servoIdx < 8) ? visualAddrA->value() : visualAddrB->value();
        auto *l = new QLabel(QString("Servo %1\nAdresse %2\nBit %3").arg(servoIdx+1).arg(adr).arg(shown));
        l->setAlignment(Qt::AlignCenter);
        return l;
    };
    for(int c=0;c<8;++c){
        auto* hdr = mkHdr(c);
        visualInfo[c] = hdr;
        grid->addWidget(hdr, 0, c);
    }

    auto *addrBline = new QWidget;
    auto *addrBL = new QHBoxLayout(addrBline);
    addrBL->setContentsMargins(0,0,0,0);
    addrBL->setSpacing(6);
    addrBL->addWidget(new QLabel("Adresse 2 (untere Reihe):"));
    addrBL->addWidget(visualAddrB);
    addrBL->addStretch(1);
    grid->addWidget(addrBline, 2, 0, 1, 8);
    for(int c=0;c<8;++c){
        auto* hdr = mkHdr(8+c);
        visualInfo[8+c] = hdr;
        grid->addWidget(hdr, 3, c);
    }

    auto *limitLine = new QWidget;
    auto *limitL = new QHBoxLayout(limitLine);
    limitL->setContentsMargins(0,0,0,0);
    limitL->setSpacing(6);
    limitL->addWidget(new QLabel("V2 Limit ±:"));
    limitL->addWidget(visualLimitSpin);
    limitL->addWidget(new QLabel("(nur GUI-Bedienlimit)"));
    limitL->addStretch(1);
    grid->addWidget(limitLine, 5, 0, 1, 8);

    for(int s=0; s<16; ++s){
        auto *box = new QGroupBox(QString("Servo %1").arg(s+1));
        auto *bl = new QVBoxLayout(box);
        bl->setContentsMargins(4,4,4,4);
        bl->setSpacing(3);
        auto *arm = new ServoArmWidget();
        visualArm[s] = arm;
        bl->addWidget(arm, 1);

        auto *row1 = new QHBoxLayout;
        row1->setContentsMargins(0,0,0,0);
        row1->setSpacing(2);
        auto *bMinus2 = new QPushButton("--");
        auto *bMinus = new QPushButton("-");
        auto *bMid = new QPushButton("Mitte");
        auto *bPlus = new QPushButton("+");
        auto *bPlus2 = new QPushButton("++");
        bMinus2->setFixedWidth(34); bPlus2->setFixedWidth(34);
        bMinus->setFixedWidth(30);  bPlus->setFixedWidth(30);
        row1->addWidget(bMinus2); row1->addWidget(bMinus); row1->addWidget(bMid); row1->addWidget(bPlus); row1->addWidget(bPlus2);
        bl->addLayout(row1);

        auto *row2 = new QHBoxLayout;
        row2->setContentsMargins(0,0,0,0);
        row2->setSpacing(2);
        auto *bL = new QPushButton("Links speichern");
        auto *bR = new QPushButton("Rechts speichern");
        row2->addWidget(bL); row2->addWidget(bR);
        bl->addLayout(row2);

        auto sendWizard = [this,beBox,busBox](int adr, int val){ BackendKind b=(beBox->currentText()=="SX")?BackendKind::SX:BackendKind::RMX; int bus=busBox->currentText().toInt(); return ctrl.send(b,bus,adr,val); };
        auto stepArm = [visualAngle,visualLimitSpin,arm,s](int delta){
            int lim = visualLimitSpin->value();
            int next = qBound(-lim, (*visualAngle)[s] + delta, lim);
            (*visualAngle)[s] = next;
            arm->setAngleDeg(next);
            return next;
        };
        connect(bMinus2,&QPushButton::clicked,this,[this,s,sendWizard,stepArm](){ int a=stepArm(-5); sendWizard(11,s); sendWizard(12,5); sendWizard(13,1); log->append(QString("V2 S%1 -- Winkel %2").arg(s+1).arg(a)); });
        connect(bMinus,&QPushButton::clicked,this,[this,s,sendWizard,stepArm](){ int a=stepArm(-1); sendWizard(11,s); sendWizard(12,1); sendWizard(13,1); log->append(QString("V2 S%1 - Winkel %2").arg(s+1).arg(a)); });
        connect(bPlus,&QPushButton::clicked,this,[this,s,sendWizard,stepArm](){ int a=stepArm(1); sendWizard(11,s); sendWizard(12,1); sendWizard(13,2); log->append(QString("V2 S%1 + Winkel %2").arg(s+1).arg(a)); });
        connect(bPlus2,&QPushButton::clicked,this,[this,s,sendWizard,stepArm](){ int a=stepArm(5); sendWizard(11,s); sendWizard(12,5); sendWizard(13,2); log->append(QString("V2 S%1 ++ Winkel %2").arg(s+1).arg(a)); });
        connect(bMid,&QPushButton::clicked,this,[this,s,sendWizard,arm,visualAngle](){ (*visualAngle)[s]=0; arm->setAngleDeg(0); sendWizard(11,s); sendWizard(13,3); log->append(QString("V2 S%1 Mitte Winkel 0").arg(s+1)); });
        connect(bL,&QPushButton::clicked,this,[this,s,sendWizard](){ sendWizard(11,s); sendWizard(14,1); log->append(QString("V2 S%1 Links speichern").arg(s+1)); });
        connect(bR,&QPushButton::clicked,this,[this,s,sendWizard](){ sendWizard(11,s); sendWizard(14,2); log->append(QString("V2 S%1 Rechts speichern").arg(s+1)); });

        int c = s%8;
        int r = (s<8) ? 1 : 4;
        grid->addWidget(box, r, c);
    }

    auto refreshVisual = [visualInfo,visualAddrA,visualAddrB,visualBitOrder](){
        for(int s=0;s<16;++s){
            if(!visualInfo[s]) continue;
            int bit=(s%8)+1;
            int shown=visualBitOrder->isChecked()?bit:(9-bit);
            int adr=(s<8)?visualAddrA->value():visualAddrB->value();
            visualInfo[s]->setText(QString("Servo %1\nAdresse %2\nBit %3").arg(s+1).arg(adr).arg(shown));
        }
    };
    connect(visualAddrA, qOverload<int>(&QSpinBox::valueChanged), this, [refreshVisual](int){ refreshVisual(); });
    connect(visualAddrB, qOverload<int>(&QSpinBox::valueChanged), this, [refreshVisual](int){ refreshVisual(); });
    connect(visualBitOrder, &QCheckBox::toggled, this, [refreshVisual](bool){ refreshVisual(); });
    visualL->addLayout(grid);
    tabs->addTab(visualTab, "Servo-Bildansicht V2");

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

    auto sendWizard = [this,beBox,busBox](int adr, int val){
        BackendKind b = (beBox->currentText()=="SX") ? BackendKind::SX : BackendKind::RMX;
        int bus = busBox->currentText().toInt();
        return ctrl.send(b, bus, adr, val);
    };
    connect(progOnBtn,&QPushButton::clicked,this,[this,sendWizard]{
        bool ok = sendWizard(127,0); log->append(QString("PROG EIN -> %1").arg(ok?"OK":"FAIL"));
    });
    connect(progOffBtn,&QPushButton::clicked,this,[this,sendWizard]{
        bool ok = sendWizard(127,128); log->append(QString("PROG AUS -> %1").arg(ok?"OK":"FAIL"));
    });
    connect(progStartBtn,&QPushButton::clicked,this,[this,sendWizard]{ bool ok=sendWizard(10,1); log->append(QString("K10 START -> %1").arg(ok?"OK":"FAIL")); });
    connect(progSaveBtn,&QPushButton::clicked,this,[this,sendWizard]{ bool ok=sendWizard(10,3); log->append(QString("K10 SAVE -> %1").arg(ok?"OK":"FAIL")); });
    connect(progAbortBtn,&QPushButton::clicked,this,[this,sendWizard]{ bool ok=sendWizard(10,2); log->append(QString("K10 ABORT -> %1").arg(ok?"OK":"FAIL")); });
    connect(progMidBtn,&QPushButton::clicked,this,[this,sendWizard,progServoIdx]{
        bool a=sendWizard(11,progServoIdx->value()-1); bool b=sendWizard(13,3);
        log->append(QString("Mitte S%1 -> %2/%3").arg(progServoIdx->value()).arg(a?"OK":"FAIL").arg(b?"OK":"FAIL"));
    });
    connect(progStoreLBtn,&QPushButton::clicked,this,[this,sendWizard,progServoIdx]{
        bool a=sendWizard(11,progServoIdx->value()-1); bool b=sendWizard(14,1);
        log->append(QString("L speichern S%1 -> %2/%3").arg(progServoIdx->value()).arg(a?"OK":"FAIL").arg(b?"OK":"FAIL"));
    });
    connect(progStoreRBtn,&QPushButton::clicked,this,[this,sendWizard,progServoIdx]{
        bool a=sendWizard(11,progServoIdx->value()-1); bool b=sendWizard(14,2);
        log->append(QString("R speichern S%1 -> %2/%3").arg(progServoIdx->value()).arg(a?"OK":"FAIL").arg(b?"OK":"FAIL"));
    });
    connect(progMoveMinusBtn,&QPushButton::clicked,this,[this,sendWizard,progStep]{ bool a=sendWizard(12,progStep->currentText().toInt()); bool b=sendWizard(13,1); log->append(QString("MOVE - -> %1/%2").arg(a?"OK":"FAIL").arg(b?"OK":"FAIL")); });
    connect(progMovePlusBtn,&QPushButton::clicked,this,[this,sendWizard,progStep]{ bool a=sendWizard(12,progStep->currentText().toInt()); bool b=sendWizard(13,2); log->append(QString("MOVE + -> %1/%2").arg(a?"OK":"FAIL").arg(b?"OK":"FAIL")); });

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
    const int tabsH = sxTable->maximumHeight() + sxBusSel->sizeHint().height() + 18;
    tabs->setMinimumHeight(tabsH);
    l->addWidget(tabs);
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
    connect(&ctrl,&DualRuntimeController::frameReceived,this,[this,rx126Only,sxTable,rmxTable,sxVals,rmxVals,sxBusSel,rmxBusSel,visualAddrA,visualAddrB,visualBitOrder,visualArm,visualAngle,visualLimitSpin](BackendKind b,int bus,int adr,int val){
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

            int adrA = visualAddrA->value();
            int adrB = visualAddrB->value();
            for(int s=0;s<16;++s){
                int sa = (s<8)?adrA:adrB;
                if(sa != adr) continue;
                int bit = (s%8)+1;
                int bitIdx = visualBitOrder->isChecked() ? (bit-1) : (8-bit);
                int on = (val >> bitIdx) & 0x1;
                int lim = visualLimitSpin->value();
                int angle = on ? lim : -lim;
                (*visualAngle)[s] = angle;
                if(visualArm[s]) visualArm[s]->setAngleDeg(angle);
            }
        }
    });

    c->adjustSize();
    const QSize wanted = c->sizeHint() + QSize(36, 36);
    resize(wanted.expandedTo(QSize(1400, 900)));
    setMinimumSize(900, 650);
}
