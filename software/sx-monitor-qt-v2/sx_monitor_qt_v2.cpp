#include <QApplication>
#include <QMainWindow>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QHeaderView>
#include <QComboBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QTextEdit>
#include <QDateTime>
#include <QPalette>
#include <QCheckBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPainter>
#include <QtMath>
#include <QPixmap>
#include <QImage>
#include <QTabWidget>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QProcess>
#include <QRandomGenerator>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static const char* APP_VERSION = "2026-05-09-main";

static QString detectGitVersion(){
    QProcess p;
    p.start("git", {"rev-parse", "--abbrev-ref", "HEAD"});
    if(!p.waitForFinished(400)) return QString(APP_VERSION);
    QString branch = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if(branch.isEmpty()) return QString(APP_VERSION);

    QProcess p2;
    p2.start("git", {"rev-parse", "--short", "HEAD"});
    if(!p2.waitForFinished(400)) return branch;
    QString sha = QString::fromUtf8(p2.readAllStandardOutput()).trimmed();
    if(sha.isEmpty()) return branch;

    return QString("%1@%2").arg(branch, sha);
}

static bool set_serial(int fd, int baud){
    termios tty{};
    if(tcgetattr(fd,&tty)!=0) return false;
    speed_t spd=B57600;
    if(baud==9600) spd=B9600; else if(baud==19200) spd=B19200; else if(baud==38400) spd=B38400; else if(baud==57600) spd=B57600; else if(baud==115200) spd=B115200;
    cfsetospeed(&tty,spd); cfsetispeed(&tty,spd);
    tty.c_cflag=(tty.c_cflag & ~CSIZE)|CS8;
    tty.c_iflag=IGNPAR; tty.c_oflag=0; tty.c_lflag=0;
    tty.c_cc[VMIN]=0; tty.c_cc[VTIME]=1;
    tty.c_cflag|=(CLOCAL|CREAD);
    tty.c_cflag&=~(PARENB|PARODD|CSTOPB|CRTSCTS);
    tty.c_iflag&=~(IXON|IXOFF|IXANY);
    tcflush(fd,TCIOFLUSH);
    return tcsetattr(fd,TCSANOW,&tty)==0;
}
static bool wr2(int fd, uint8_t a, uint8_t d){ uint8_t b[2]={a,d}; return write(fd,b,2)==2; }

static QString autodetectSelectrixPort(){
    QDir byid("/dev/serial/by-id");
    if(byid.exists()){
        auto list = byid.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot);
        QString fallback;
        for(const QFileInfo &fi : list){
            const QString n = fi.fileName();
            const QString full = fi.absoluteFilePath();
            if(n.contains("FTF8NBF0", Qt::CaseInsensitive)) return full;
            if(n.contains("USB_Serial_Converter", Qt::CaseInsensitive)) fallback = full;
        }
        if(!fallback.isEmpty()) return fallback;
    }
    if(QFileInfo::exists("/dev/ttyUSB2")) return "/dev/ttyUSB2";
    if(QFileInfo::exists("/dev/ttyUSB1")) return "/dev/ttyUSB1";
    return "/dev/ttyUSB0";
}

static QString autodetectArduinoPort(){
    QDir byid("/dev/serial/by-id");
    if(byid.exists()){
        auto list = byid.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot);
        QString fallback;
        for(const QFileInfo &fi : list){
            const QString n = fi.fileName();
            const QString full = fi.absoluteFilePath();
            if(n.contains("A50285BI", Qt::CaseInsensitive)) return full;
            if(n.contains("FT232R_USB_UART", Qt::CaseInsensitive)) fallback = full;
        }
        if(!fallback.isEmpty()) return fallback;
    }
    if(QFileInfo::exists("/dev/ttyUSB0")) return "/dev/ttyUSB0";
    if(QFileInfo::exists("/dev/ttyUSB1")) return "/dev/ttyUSB1";
    return "/dev/ttyUSB0";
}

static QString displayPortWithHint(const QString &path){
    QFileInfo fi(path);
    if(path.startsWith("/dev/serial/by-id/") && fi.exists()){
        QString real = fi.symLinkTarget();
        if(!real.isEmpty()) return QString("%1 (%2)").arg(real, path);
    }
    return path;
}

static QString extractActualPort(const QString &display){
    int p1 = display.indexOf('(');
    int p2 = display.lastIndexOf(')');
    if(p1>0 && p2>p1){
        QString inner = display.mid(p1+1, p2-p1-1).trimmed();
        if(inner.startsWith("/dev/")) return inner;
    }
    return display.section(' ',0,0).trimmed();
}

static QString bits8(int v, bool bit1Left){
    QString s;
    if(bit1Left){ // optisch: Bit1..Bit8 von links nach rechts
        for(int i=0;i<8;++i) s += ((v>>i)&1)?'1':'0';
    } else { // technisch: Bit8..Bit1 von links nach rechts
        for(int i=7;i>=0;--i) s += ((v>>i)&1)?'1':'0';
    }
    return s;
}

class ServoArmWidget : public QWidget {
public:
    explicit ServoArmWidget(QWidget* parent=nullptr): QWidget(parent) {
        setMinimumSize(150,150);
        QString base = QCoreApplication::applicationDirPath() + "/../assets/";
        body = QPixmap(base + "servo_body_blue.png");
        arm = QPixmap(base + "servo_arm_new.png");
        // normalized pivot points from assets
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

        // compose in virtual canvas first, then fit to widget -> no clipping
        QRectF view = rect().adjusted(6, 6, -6, -6);
        double base = 1000.0;
        QRectF bodyR(0, 0, base, base);
        QPointF c(bodyHubX*base, bodyHubY*base);
        double armSide = base * 1.20;
        QRectF armR(c.x() - armHubX*armSide, c.y() - armHubY*armSide, armSide, armSide);

        // conservative bounds for rotated square arm
        double halfDiag = (armSide * 0.5) * 1.41421356237;
        QRectF armBound(c.x()-halfDiag, c.y()-halfDiag, 2*halfDiag, 2*halfDiag);
        QRectF scene = bodyR.united(armBound);

        double s = qMin(view.width()/scene.width(), view.height()/scene.height());
        QPointF t(view.center().x() - s*scene.center().x(),
                  view.center().y() - s*scene.center().y());

        p.save();
        p.translate(t);
        p.scale(s, s);

        if(!body.isNull()) p.drawPixmap(bodyR.toRect(), body);

        if(!arm.isNull()){
            p.save();
            p.translate(c);
            p.rotate((double)-angle - 90.0);
            p.translate(-c);
            p.drawPixmap(armR.toRect(), arm);
            p.restore();
        }
        p.restore();

        p.setPen(QPen(QColor(30,30,30),1));
        p.drawText(QRect(0,0,width(),20), Qt::AlignCenter, QString("%1°").arg(angle));
    }
private:
    int angle = 0;
    QPixmap body, arm;
    double bodyHubX = 0.5;
    double bodyHubY = 0.3;
    double armHubX = 0.5;
    double armHubY = 0.5;
};

class MainWin : public QMainWindow {
    Q_OBJECT
public:
    MainWin(){
        appVersion = detectGitVersion();
        setWindowTitle(QString("SX Monitor – SLX852 (SX0/SX1) | %1").arg(appVersion));
        resize(1280, 860);
        uptime.start();

        auto *cw = new QWidget; setCentralWidget(cw);
        auto *root = new QVBoxLayout(cw);

        auto *cfg = new QGroupBox("Interface / Zentrale (nach SX1-Doku)");
        auto *cfgL = new QHBoxLayout(cfg);
        ifaceBox = new QComboBox; ifaceBox->addItems({"SLX852"});
        busBox = new QComboBox; busBox->addItems({"SX0","SX1","SX0+SX1"});
        const QString sxAuto = autodetectSelectrixPort();
        portEdit = new QLineEdit(displayPortWithHint(sxAuto));
        baudBox = new QComboBox;
        baudBox->addItems({"9600","19200","38400","57600","115200"});
        baudBox->setCurrentText("57600");
        bitOrderBox = new QCheckBox("Bit 1 links / Bit 8 rechts (SX-Optik)");
        bitOrderBox->setChecked(true);
        connectBtn = new QPushButton("Connect");
        disconnectBtn = new QPushButton("Disconnect"); disconnectBtn->setEnabled(false);
        statusLbl = new QLabel("offline");
        cfgL->addWidget(new QLabel("Interface:")); cfgL->addWidget(ifaceBox);
        cfgL->addWidget(new QLabel("Busse:")); cfgL->addWidget(busBox);
        cfgL->addWidget(new QLabel("Port:")); cfgL->addWidget(portEdit);
        cfgL->addWidget(new QLabel("Baud:")); cfgL->addWidget(baudBox);
        cfgL->addWidget(bitOrderBox);
        cfgL->addWidget(connectBtn); cfgL->addWidget(disconnectBtn);
        cfgL->addWidget(statusLbl);
        root->addWidget(cfg);

        auto *tCfg = new QGroupBox("Arduino Telemetrie");
        auto *tCfgL = new QHBoxLayout(tCfg);
        telePortEdit = new QLineEdit(autodetectArduinoPort());
        teleBaudBox = new QComboBox;
        teleBaudBox->addItems({"9600","19200","38400","57600","115200"});
        teleBaudBox->setCurrentText("115200");
        teleConnectBtn = new QPushButton("Telemetrie Connect");
        teleDisconnectBtn = new QPushButton("Telemetrie Disconnect");
        teleReqHelloBtn = new QPushButton("HELLO (t)");
        teleReqCfgBtn = new QPushButton("CFG (c)");
        ackCfgFallbackBox = new QCheckBox("ACK CFG-Fallback (Notbetrieb)");
        ackCfgFallbackBox->setChecked(false);
        teleDisconnectBtn->setEnabled(false);
        teleStatusLbl = new QLabel("telemetry offline");
        fwStatusLbl = new QLabel("FW: unbekannt");
        ackModeLbl = new QLabel("ACK-Modus: STRICT");
        tCfgL->addWidget(new QLabel("Port:")); tCfgL->addWidget(telePortEdit);
        tCfgL->addWidget(new QLabel("Baud:")); tCfgL->addWidget(teleBaudBox);
        tCfgL->addWidget(teleConnectBtn); tCfgL->addWidget(teleDisconnectBtn);
        tCfgL->addWidget(teleReqHelloBtn); tCfgL->addWidget(teleReqCfgBtn); tCfgL->addWidget(ackCfgFallbackBox);
        tCfgL->addWidget(teleStatusLbl); tCfgL->addWidget(fwStatusLbl); tCfgL->addWidget(ackModeLbl);
        root->addWidget(tCfg);

        auto *tabs = new QTabWidget;
        table = new QTableWidget(28,12);
        table->setHorizontalHeaderLabels({
            "Adr","Wert","Bits",
            "Adr","Wert","Bits",
            "Adr","Wert","Bits",
            "Adr","Wert","Bits"
        });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setVisible(false);
        table->setAlternatingRowColors(true);
        table->verticalHeader()->setDefaultSectionSize(22);
        for(int adr=0; adr<112; ++adr){
            int row = adr % 28;
            int block = adr / 28;      // 0..3
            int base = block * 3;      // 0,3,6,9
            table->setItem(row, base+0, new QTableWidgetItem(QString::number(adr)));
            table->setItem(row, base+1, new QTableWidgetItem("-"));
            table->setItem(row, base+2, new QTableWidgetItem("--------"));
            for(int c=base; c<base+3; ++c){
                table->item(row,c)->setForeground(QBrush(Qt::black));
                table->item(row,c)->setBackground(QBrush(Qt::white));
            }
        }
        table->setStyleSheet("QTableWidget { background: white; color: black; gridline-color: #cfcfcf; alternate-background-color: #fafafa; }"
                             "QHeaderView::section { background: #f2f2f2; color: black; padding: 4px; font-weight: 600; }");

        // Spaltenbreiten passend zum Inhalt
        for(int b=0; b<4; ++b){
            int base=b*3;
            table->setColumnWidth(base+0, 42);   // Adr
            table->setColumnWidth(base+1, 48);   // Wert
            table->setColumnWidth(base+2, 86);   // Bits
        }
        tabs->addTab(table, "Bus-Monitor Tabelle");

        logView = new QTextEdit; logView->setReadOnly(true);
        tabs->addTab(logView, "Änderungsprotokoll");
        appendLog(QString("SX-Monitor-Qt V2 gestartet | Version: %1").arg(appVersion));

        auto *progTab = new QWidget;
        auto *progL = new QVBoxLayout(progTab);
        auto *r1 = new QHBoxLayout;
        progAddrA = new QSpinBox; progAddrA->setRange(1,111);
        progAddrB = new QSpinBox; progAddrB->setRange(0,111);
        progOnBtn = new QPushButton("Prog EIN (Track=0)");
        progOffBtn = new QPushButton("Prog AUS (Track=1)");
        r1->addWidget(new QLabel("AddrA")); r1->addWidget(progAddrA);
        r1->addWidget(new QLabel("AddrB")); r1->addWidget(progAddrB);
        r1->addWidget(progOnBtn); r1->addWidget(progOffBtn);
        progL->addLayout(r1);

        auto *r2 = new QHBoxLayout;
        progServoIdx = new QSpinBox; progServoIdx->setRange(1,16);
        progStep = new QComboBox; progStep->addItems({"1","2","5","10","20"}); progStep->setCurrentText("5");
        progMoveMinusBtn = new QPushButton("-");
        progMovePlusBtn = new QPushButton("+");
        progMidBtn = new QPushButton("Mitte");
        progStoreLBtn = new QPushButton("L speichern");
        progStoreRBtn = new QPushButton("R speichern");
        progStartBtn = new QPushButton("Setup START (K10=1)");
        progSaveBtn = new QPushButton("Setup SAVE+ENDE (K10=3)");
        progAbortBtn = new QPushButton("Setup ABBRUCH (K10=2)");
        r2->addWidget(new QLabel("Servo")); r2->addWidget(progServoIdx);
        r2->addWidget(new QLabel("Schritt")); r2->addWidget(progStep);
        r2->addWidget(progMoveMinusBtn); r2->addWidget(progMovePlusBtn); r2->addWidget(progMidBtn);
        r2->addWidget(progStoreLBtn); r2->addWidget(progStoreRBtn);
        r2->addWidget(progStartBtn); r2->addWidget(progSaveBtn); r2->addWidget(progAbortBtn);
        progL->addLayout(r2);

        servoTable = new QTableWidget(16,6);
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
            connect(bMid,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,11,s); sendSX(bus,13,3); appendLog(QString("SETUP Mitte S%1").arg(s+1)); });
            connect(bG,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,11,s); sendSX(bus,14,1); appendLog(QString("SETUP L speichern S%1").arg(s+1)); });
            connect(bA,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,11,s); sendSX(bus,14,2); appendLog(QString("SETUP R speichern S%1").arg(s+1)); });
            connect(bC,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,11,s); appendLog(QString("SETUP Servo selektiert S%1").arg(s+1)); });
        }
        progL->addWidget(servoTable);

        auto *r3 = new QHBoxLayout;
        progCommitAllBtn = new QPushButton("Servo uebernehmen (K11)");
        r3->addWidget(progCommitAllBtn);
        progL->addLayout(r3);

        auto *sx1Hint = new QLabel("Wizard: K10 Start/Save/Abort, K11 Servo, K12 Schritt, K13 Move, K14 L/R speichern");
        progL->addWidget(sx1Hint);
        progStatusLbl = new QLabel("Status K15: -");
        progL->addWidget(progStatusLbl);

        tabs->addTab(progTab, "Servo-Programmer");

        auto *visualTab = new QWidget;
        auto *visualL = new QVBoxLayout(visualTab);
        visualAddrA = new QSpinBox; visualAddrA->setRange(1,111); visualAddrA->setValue(progAddrA->value()); visualAddrA->setFixedWidth(90);
        visualAddrB = new QSpinBox; visualAddrB->setRange(0,111); visualAddrB->setValue(progAddrB->value()); visualAddrB->setFixedWidth(90);
        visualBitOrder = new QCheckBox("Bits links->rechts (Bit1 links)"); visualBitOrder->setChecked(true);
        auto *busRow = new QHBoxLayout;
        busRow->setContentsMargins(0,0,0,0);
        busRow->setSpacing(6);
        busRow->addWidget(new QLabel("SX-Bus: zentral über 'SX Senden'"));
        progPathBox = new QComboBox;
        progPathBox->addItem("Serial-Wizard (empfohlen)");
        progPathBox->addItem("SX-Wizard (experimentell)");
        busRow->addWidget(new QLabel("Programmierweg:"));
        busRow->addWidget(progPathBox);
        busRow->addStretch(1);
        visualL->addLayout(busRow);

        auto *addrArow = new QHBoxLayout;
        addrArow->setContentsMargins(0,0,0,0);
        addrArow->setSpacing(6);
        visualSetupRequestBtn = new QPushButton("Progmodus anfordern (Taste drücken)");
        visualSetupSaveBtn = new QPushButton("Setup Ende (K10=3)");
        visualSetupAbortBtn = new QPushButton("Setup Abbruch (K10=2)");
        addrArow->addWidget(new QLabel("Adresse 1 (obere Reihe):"));
        addrArow->addWidget(visualAddrA);
        addrArow->addSpacing(10);
        addrArow->addWidget(visualBitOrder);
        addrArow->addSpacing(10);
        visualProgStateLbl = new QLabel("Progstatus: unbekannt (Taste am Arduino drücken)");
        addrArow->addWidget(visualSetupRequestBtn);
        addrArow->addWidget(visualSetupSaveBtn);
        addrArow->addWidget(visualSetupAbortBtn);
        addrArow->addWidget(visualProgStateLbl);
        addrArow->addStretch(1);
        visualL->addLayout(addrArow);

        auto pulseMove = [this](int servo, int move){ sendVisualWizardMove(servo, move); };
        auto pulseStore = [this](int servo, int store){ sendVisualWizardStore(servo, store); };

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(4);
        grid->setVerticalSpacing(4);
        auto mkHdr = [this](int servoIdx)->QLabel*{
            int bit = (servoIdx % 8) + 1;
            int shown = (visualBitOrder && visualBitOrder->isChecked()) ? bit : (9-bit);
            int adr = (servoIdx < 8) ? visualAddrA->value() : visualAddrB->value();
            auto *l = new QLabel(QString("Servo %1\nAdresse %2\nBit %3").arg(servoIdx+1).arg(adr).arg(shown));
            l->setAlignment(Qt::AlignCenter);
            return l;
        };
        for(int c=0;c<8;++c) grid->addWidget(mkHdr(c), 0, c);
        auto *addrBline = new QWidget;
        auto *addrBL = new QHBoxLayout(addrBline);
        addrBL->setContentsMargins(0,0,0,0);
        addrBL->setContentsMargins(0,0,0,0);
        addrBL->setSpacing(6);
        addrBL->addWidget(new QLabel("Adresse 2 (untere Reihe):"));
        addrBL->addWidget(visualAddrB);
        addrBL->addStretch(1);
        grid->addWidget(addrBline, 2, 0, 1, 8);
        for(int c=0;c<8;++c) grid->addWidget(mkHdr(8+c), 3, c);

        auto *limitLine = new QWidget;
        auto *limitL = new QHBoxLayout(limitLine);
        limitL->setContentsMargins(0,0,0,0);
        limitL->setSpacing(6);
        limitL->addWidget(new QLabel("V2 Limit ±:"));
        visualLimitSpin = new QSpinBox;
        visualLimitSpin->setRange(30,45);
        visualLimitSpin->setValue(40);
        limitL->addWidget(visualLimitSpin);
        limitL->addWidget(new QLabel("(nur GUI-Bedienlimit)"));
        limitL->addStretch(1);
        grid->addWidget(limitLine, 5, 0, 1, 8);

        for(int s=0; s<16; ++s){
            servoArmPos[s] = 0;
            armLiveValid[s] = false;
            auto *box = new QGroupBox(QString("Servo %1").arg(s+1));
            visualServoBoxes[s] = box;
            auto *bl = new QVBoxLayout(box);
            auto *arm = new ServoArmWidget();
            servoArmWidgets[s] = arm;
            bl->addWidget(arm, 1);

            auto *row1 = new QHBoxLayout;
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
            auto *bL = new QPushButton("Links speichern");
            auto *bR = new QPushButton("Rechts speichern");
            row2->addWidget(bL); row2->addWidget(bR);
            bl->addLayout(row2);

            auto moveBy = [this,s,pulseMove](int delta){
                int lim = visualLimitSpin ? visualLimitSpin->value() : 40;
                int oldPos = servoArmPos[s];
                int targetPos = std::clamp(oldPos + delta, -lim, lim);
                int steps = std::abs(targetPos - oldPos);
                if(steps == 0){
                    appendLog(QString("V2 S%1 an Grenze (±%2)").arg(s+1).arg(lim));
                    return;
                }
                int dir = (targetPos > oldPos) ? 1 : -1;
                if(moveQueue[s] != 0){
                    int qdir = (moveQueue[s] > 0) ? 1 : -1;
                    if(qdir != dir) moveQueue[s] = 0; // Richtungswechsel: alte Queue verwerfen
                }
                moveQueue[s] += dir * steps;
                if(moveQueue[s] > lim) moveQueue[s] = lim;
                if(moveQueue[s] < -lim) moveQueue[s] = -lim;
                appendLog(QString("V2 S%1 queue=%2 (delta=%3, target=%4)").arg(s+1).arg(moveQueue[s]).arg(delta).arg(targetPos));
                if(ackPendingType[s].isEmpty()){
                    int cmd = (moveQueue[s] > 0) ? 2 : 1;
                    moveQueue[s] += (moveQueue[s] > 0) ? -1 : +1;
                    pulseMove(s, cmd);
                }
                updateVisualTitles();
            };
            connect(bMinus2,&QPushButton::clicked,this,[moveBy](){ moveBy(-10); });
            connect(bMinus,&QPushButton::clicked,this,[moveBy](){ moveBy(-1); });
            connect(bPlus,&QPushButton::clicked,this,[moveBy](){ moveBy(+1); });
            connect(bPlus2,&QPushButton::clicked,this,[moveBy](){ moveBy(+10); });
            connect(bMid,&QPushButton::clicked,this,[this,s,pulseMove](){ pulseMove(s,3); appendLog(QString("V2 S%1 Mitte (warte ACK)").arg(s+1)); });
            connect(bL,&QPushButton::clicked,this,[this,s,pulseStore](){ pulseStore(s,1); appendLog(QString("V2 S%1 Links speichern").arg(s+1)); });
            connect(bR,&QPushButton::clicked,this,[this,s,pulseStore](){ pulseStore(s,2); appendLog(QString("V2 S%1 Rechts speichern").arg(s+1)); });

            int c = s%8;
            int r = (s<8) ? 1 : 4;
            grid->addWidget(box, r, c);
        }
        visualL->addLayout(grid);
        tabs->addTab(visualTab, "Servo-Bildansicht V2");
        root->addWidget(tabs);

        auto *sendBox = new QGroupBox("SX senden");
        auto *sendL = new QHBoxLayout(sendBox);
        sendBusBox = busBox; // zentrale, einzige Busauswahl oben
        sendAdr = new QSpinBox; sendAdr->setRange(0,111);
        sendVal = new QSpinBox; sendVal->setRange(0,255);
        sendBtn = new QPushButton("Senden");
        quick0Btn = new QPushButton("0");
        quick1Btn = new QPushButton("1");
        quick255Btn = new QPushButton("255");
        confirmBox = new QCheckBox("Senden bestätigen");
        rxPauseBox = new QCheckBox("SX-Monitor RX pausieren (nur TX+Telemetrie)");
        rxPauseBox->setChecked(true);
        bitButtonsBox = new QComboBox;
        bitButtonsBox->addItems({"Bitbuttons aus","Bitbuttons ein"});

        sendL->addWidget(new QLabel("Adresse:")); sendL->addWidget(sendAdr);
        sendL->addWidget(new QLabel("Wert:")); sendL->addWidget(sendVal);
        sendL->addWidget(quick0Btn); sendL->addWidget(quick1Btn); sendL->addWidget(quick255Btn);
        sendL->addWidget(sendBtn);
        sendL->addWidget(confirmBox);
        sendL->addWidget(rxPauseBox);
        root->addWidget(sendBox);

        infoLbl = new QLabel("Änderungen gelb markiert • FE A0 bei Connect");
        root->addWidget(infoLbl);

        for(int i=0;i<112;i++){ sx0[i]=-1; sx1[i]=-1; }

        timer = new QTimer(this);
        connect(timer,&QTimer::timeout,this,&MainWin::pollSerial);
        connect(connectBtn,&QPushButton::clicked,this,&MainWin::doConnect);
        connect(disconnectBtn,&QPushButton::clicked,this,&MainWin::doDisconnect);
        connect(teleConnectBtn,&QPushButton::clicked,this,&MainWin::doTeleConnect);
        connect(teleDisconnectBtn,&QPushButton::clicked,this,&MainWin::doTeleDisconnect);
        connect(teleReqHelloBtn,&QPushButton::clicked,this,[this](){ if(teleFd>=0){ ::write(teleFd,"t\n",2); appendLog("TEL TX: t"); } });
        connect(teleReqCfgBtn,&QPushButton::clicked,this,[this](){ if(teleFd>=0){ ::write(teleFd,"c\n",2); appendLog("TEL TX: c"); } });
        connect(ackCfgFallbackBox, &QCheckBox::toggled, this, [this](bool on){
            ackModeLbl->setText(on ? "ACK-Modus: NOTBETRIEB (CFG-Fallback)" : "ACK-Modus: STRICT");
            ackModeLbl->setStyleSheet(on ? "QLabel { color: #c97a00; font-weight: bold; }" : "QLabel { color: #0a8a0a; font-weight: bold; }");
        });
        ackModeLbl->setStyleSheet("QLabel { color: #0a8a0a; font-weight: bold; }");
        connect(sendBtn,&QPushButton::clicked,this,&MainWin::sendValue);
        connect(quick0Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(0); sendValue(); });
        connect(quick1Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(1); sendValue(); });
        connect(quick255Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(255); sendValue(); });
        connect(table,&QTableWidget::cellDoubleClicked,this,&MainWin::openSwitchPanel);
        connect(visualAddrA, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){ progAddrA->setValue(v); visualSetupStarted=false; for(int i=0;i<16;++i) moveQueue[i]=0; updateVisualTitles(); });
        connect(visualAddrB, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){ progAddrB->setValue(v); visualSetupStarted=false; for(int i=0;i<16;++i) moveQueue[i]=0; updateVisualTitles(); });
        connect(visualBitOrder,&QCheckBox::toggled,this,[this](bool){ updateVisualTitles(); });
        connect(sendBusBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int){ visualSetupStarted=false; for(int i=0;i<16;++i) moveQueue[i]=0; updateVisualTitles(); });
        connect(tabs,&QTabWidget::currentChanged,this,[sendBox](int idx){ sendBox->setVisible(idx != 1); });
        connect(visualSetupRequestBtn,&QPushButton::clicked,this,[this](){
            visualSetupArmed=true;
            visualSetupStarted=false;
            for(int i=0;i<16;++i) moveQueue[i]=0;
            wizardLockedServo = -1;
            int bus=(sendBusBox->currentText()=="SX1")?1:0;
            wizardPulseK10(bus,1,"V2 SETUP START (K10=1 Impuls)");
            if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: angefordert (K10=1 gesendet), warte auf ACK_SETUP_*");
            appendLog("V2: Setup angefordert. Lokal Taste ist optional, primär startet K10=1 den Wizard.");
            updateVisualTitles();
        });
        connect(visualSetupSaveBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardPulseK10(bus,3,"V2 SETUP ENDE (K10=3 Impuls)"); visualSetupStarted=false; visualSetupArmed=false; for(int i=0;i<16;++i) moveQueue[i]=0; if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: beendet (Save)"); updateVisualTitles(); });
        connect(visualSetupAbortBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardPulseK10(bus,2,"V2 SETUP ABBRUCH (K10=2 Impuls)"); visualSetupStarted=false; visualSetupArmed=false; for(int i=0;i<16;++i) moveQueue[i]=0; if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: beendet (Abort)"); updateVisualTitles(); });
        updateVisualTitles();

        connect(progOnBtn,&QPushButton::clicked,this,[this](){
            int bus = (sendBusBox->currentText()=="SX1") ? 1 : 0;
            sendSX(bus, 1, progAddrA->value());
            sendSX(bus, 2, progAddrB->value());
            sendSX(bus, 0, 0); // TrackBit 0 (über Adr0)
            appendLog(QString("PROG EIN (%1): AddrA/AddrB gesetzt, Track=0").arg(bus?"SX1":"SX0"));
        });
        connect(progOffBtn,&QPushButton::clicked,this,[this](){
            int bus = (sendBusBox->currentText()=="SX1") ? 1 : 0;
            sendSX(bus, 0, 1); // TrackBit 1
            appendLog(QString("PROG AUS (%1): Track=1").arg(bus?"SX1":"SX0"));
        });
        connect(progStartBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardPulseK10(bus,1,"SETUP START (K10=1 Impuls)"); });
        connect(progSaveBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardPulseK10(bus,3,"SETUP SAVE+ENDE (K10=3 Impuls)"); });
        connect(progAbortBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardPulseK10(bus,2,"SETUP ABBRUCH (K10=2 Impuls)"); });
        connect(progMoveMinusBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardMove(bus, progAddrA->value(), progAddrB->value(), progServoIdx->value()-1, progStep->currentText().toInt(), 1, "SETUP MOVE - (Impuls)"); });
        connect(progMovePlusBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardMove(bus, progAddrA->value(), progAddrB->value(), progServoIdx->value()-1, progStep->currentText().toInt(), 2, "SETUP MOVE + (Impuls)"); });
        connect(progMidBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardMove(bus, progAddrA->value(), progAddrB->value(), progServoIdx->value()-1, progStep->currentText().toInt(), 3, "SETUP MITTE (Impuls)"); });
        connect(progStoreLBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardStore(bus, progAddrA->value(), progAddrB->value(), progServoIdx->value()-1, 1, "SETUP L speichern (Impuls)"); });
        connect(progStoreRBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; wizardStore(bus, progAddrA->value(), progAddrB->value(), progServoIdx->value()-1, 2, "SETUP R speichern (Impuls)"); });
        connect(progCommitAllBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,11,progServoIdx->value()-1); appendLog("SETUP Servo uebernommen"); });
    }
    ~MainWin(){ doDisconnect(); }

private:
    void wizardPrime(int bus, int addrA, int addrB, int servo, int step){
        sendSX(bus,9,sxWizardSessionId);
        usleep(8000);
        sendSX(bus,1,addrA);
        usleep(12000);
        sendSX(bus,2,addrB);
        usleep(12000);
        sendSX(bus,11,servo);
        usleep(12000);
        sendSX(bus,12,step);
        usleep(12000);
        sendSX(bus,15,1); // Setup-Freigabe aktiv halten
        usleep(18000);
    }
    bool useSerialWizard() const { return progPathBox && progPathBox->currentText().startsWith("Serial-Wizard"); }
    bool sendSerialWizardChar(char c, const QString &tag){
        if(teleFd<0){ appendLog("WARN: Serial-Wizard: Telemetrie-Port nicht verbunden"); return false; }
        char out[2]={c,'\n'}; ::write(teleFd,out,2);
        appendLog(QString("SER TX(%1): %2").arg(tag).arg(QChar(c)));
        return true;
    }
    void wizardPulseK10(int bus, int val, const QString &msg){
        if(useSerialWizard()){
            if(val==1){
                if(visualSetupStarted){
                    appendLog("WARN: Setup START ignoriert (Serial-Wizard bereits aktiv)");
                    return;
                }
                sxWizardSessionId = 1;
                visualSetupStarted = true;
                sendSerialWizardChar('s', "start");
            } else if(val==3){
                sendSerialWizardChar('w', "save-end");
                visualSetupStarted = false;
                visualSetupArmed = false;
                for(int i=0;i<16;++i) moveQueue[i]=0;
            } else if(val==2){
                sendSerialWizardChar('x', "abort");
                visualSetupStarted = false;
                visualSetupArmed = false;
                for(int i=0;i<16;++i) moveQueue[i]=0;
            }
            if(val==3 || val==2) sxWizardSessionId=0;
            updateVisualTitles();
            appendLog(msg + " [via SERIAL]");
            return;
        }
        if(val==1){
            static const uint8_t sidPool[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};
            sxWizardSessionId = sidPool[QRandomGenerator::global()->bounded((int)(sizeof(sidPool)/sizeof(sidPool[0])) )];
            appendLog(QString("V2 Session-ID gesetzt: %1").arg((int)sxWizardSessionId));
        }
        sendSX(bus,9,sxWizardSessionId); usleep(8000); sendSX(bus,15,1); usleep(15000); sendSX(bus,10,val); usleep(120000); sendSX(bus,10,0); usleep(25000);
        if(val==3 || val==2){ sendSX(bus,9,0); sxWizardSessionId = 0; }
        appendLog(msg);
    }
    void wizardMove(int bus, int addrA, int addrB, int servo, int step, int move, const QString &msg){
        if(useSerialWizard()){
            if(step==1) sendSerialWizardChar('1', "step"); else if(step==2) sendSerialWizardChar('2', "step"); else if(step==5) sendSerialWizardChar('5', "step");
            sendSerialWizardChar(move==1?'-':(move==2?'+':'0'), "move");
            usleep(160000); // Serial-Wizard: kurze Ruhezeit, damit ACK/Statuszeilen nicht mit Folgekommandos kollidieren
            appendLog(msg + " [via SERIAL]");
            return;
        }
        wizardPrime(bus, addrA, addrB, servo, step); sendSX(bus,13,move); usleep(120000); sendSX(bus,13,0); usleep(25000);
        appendLog(msg);
        appendLog(QString("DBG MOVE bus=%1 K1(addrA)=%2 K2(addrB)=%3 K15=1 K11(servo)=%4 K12(step)=%5 K13(move)=%6").arg(bus?"SX1":"SX0").arg(addrA).arg(addrB).arg(servo).arg(step).arg(move));
    }
    void wizardStore(int bus, int addrA, int addrB, int servo, int store, const QString &msg){
        if(useSerialWizard()){
            sendSerialWizardChar(store==1?'l':'r', "store");
            appendLog(msg + " [via SERIAL]");
            return;
        }
        wizardPrime(bus, addrA, addrB, servo, 5); sendSX(bus,14,store); usleep(120000); sendSX(bus,14,0); usleep(25000); appendLog(msg);
    }
    void sendVisualWizardMove(int servo, int move){
        if(!useSerialWizard() && sxWizardSessionId==0){
            appendLog("WARN: MOVE blockiert (keine aktive Session-ID). Erst Setup START ausführen.");
            return;
        }
        if(wizardLockedServo<0) wizardLockedServo = servo;
        if(servo != wizardLockedServo){ appendLog(QString("WARN: MOVE auf S%1 ignoriert (Wizard-Lock auf S%2)").arg(servo+1).arg(wizardLockedServo+1)); return; }
        int bus=(sendBusBox->currentText()=="SX1")?1:0;
        wizardMove(bus, visualAddrA->value(), visualAddrB->value(), servo, progStep->currentText().toInt(), move,
                   QString("V2 MOVE s=%1 cmd=%2 bus=%3").arg(servo+1).arg(move).arg(bus==1?"SX1":"SX0"));
        setAckPending(servo, "move", 1);
        if(teleFd>=0 && !cfgImportInProgress && !useSerialWizard()){
            moveAutoCfgCounter++;
            if((moveAutoCfgCounter % 4) == 0 || moveQueue[servo]==0){
                usleep(100000);
                ::write(teleFd, "c\n", 2);
                appendLog("TEL TX(auto): c (Move-Verifikation, gedrosselt)");
            }
        }
    }
    void sendVisualWizardStore(int servo, int store){
        if(!useSerialWizard() && sxWizardSessionId==0){
            appendLog("WARN: STORE blockiert (keine aktive Session-ID). Erst Setup START ausführen.");
            return;
        }
        if(wizardLockedServo<0) wizardLockedServo = servo;
        if(servo != wizardLockedServo){ appendLog(QString("WARN: STORE auf S%1 ignoriert (Wizard-Lock auf S%2)").arg(servo+1).arg(wizardLockedServo+1)); return; }
        int bus=(sendBusBox->currentText()=="SX1")?1:0;
        wizardStore(bus, visualAddrA->value(), visualAddrB->value(), servo, store,
                    QString("V2 STORE s=%1 cmd=%2 bus=%3").arg(servo+1).arg(store).arg(bus==1?"SX1":"SX0"));
        setAckPending(servo, "store");
        if(teleFd>=0){
            usleep(120000);
            ::write(teleFd, "c\n", 2);
            appendLog("TEL TX(auto): c (Store-Verifikation)");
        }
    }

private slots:
    void doConnect(){
        doDisconnect();
        const QString sxPortActual = extractActualPort(portEdit->text());
        fd = open(sxPortActual.toUtf8().constData(), O_RDWR|O_NOCTTY|O_SYNC);
        if(fd<0){ statusLbl->setText("open failed"); QMessageBox::warning(this, "SX Connect", QString("Port konnte nicht geöffnet werden:\n%1").arg(sxPortActual)); return; }
        int baud = baudBox->currentText().toInt();
        if(!set_serial(fd, baud)){ statusLbl->setText("serial cfg failed"); ::close(fd); fd=-1; return; }

        bool okA0 = wr2(fd, 0xFE, 0xA0); usleep(20000);   // Monitor+Feedback
        bool okB0 = wr2(fd, 0xFE, 0xB0); usleep(10000);   // Start mit Bus 0 selektiert
        if(!okA0 || !okB0){ statusLbl->setText("connect test failed"); ::close(fd); fd=-1; return; }

        int rxCount = 0;
        for(int i=0; i<40; ++i){
            uint8_t b=0;
            int r = ::read(fd, &b, 1);
            if(r==1) rxCount++;
            else usleep(10000);
        }
        if(rxCount < 4){
            statusLbl->setText("connect failed: kein SX-Bus erkannt");
            appendLog(QString("Connect abgebrochen: kein SX-Bus-Datenverkehr erkannt (rx=%1)").arg(rxCount));
            QMessageBox::warning(this, "SX Connect", QString("Kein SX-Bus erkannt.\nPort: %1\nEmpfangene Bytes im Handshake: %2").arg(sxPortActual).arg(rxCount));
            ::close(fd); fd=-1;
            connectBtn->setEnabled(true); disconnectBtn->setEnabled(false);
            connectBtn->setStyleSheet("");
            return;
        }

        rtbsBus1 = false;
        visualSetupStarted = false;
        pending=-1;
        sendBusBox->setCurrentText("SX1");
        rxWarmupUntilMs = uptime.elapsed() + 1200; // Initialrauschen nach Connect unterdruecken
        timer->start(25);
        statusLbl->setText("online");
        connectBtn->setEnabled(false); disconnectBtn->setEnabled(true);
        connectBtn->setStyleSheet("QPushButton { background:#1f7a1f; color:white; font-weight:600; }");
        appendLog("Connect ok, SX-Interface erkannt (FE A0/B0 + Datenverkehr). Bus für V2 auf SX1 gesetzt.");
    }

    void doDisconnect(){
        timer->stop();
        if(fd>=0){ ::close(fd); fd=-1; }
        connectBtn->setEnabled(true); disconnectBtn->setEnabled(false);
        connectBtn->setStyleSheet("");
        statusLbl->setText("offline");
    }

    void doTeleConnect(){
        doTeleDisconnect();
        teleFd = open(telePortEdit->text().toUtf8().constData(), O_RDWR|O_NOCTTY|O_SYNC);
        if(teleFd<0){ teleStatusLbl->setText("tele open failed"); return; }
        int baud = teleBaudBox->currentText().toInt();
        if(!set_serial(teleFd, baud)){ teleStatusLbl->setText("tele serial cfg failed"); ::close(teleFd); teleFd=-1; return; }
        teleBinaryScore = 0;
        teleAsciiScore = 0;
        teleStatusLbl->setText("telemetry online");
        teleConnectBtn->setEnabled(false); teleDisconnectBtn->setEnabled(true);
        appendLog("Telemetrie-Port verbunden.");
    }

    void doTeleDisconnect(){
        if(teleFd>=0){ ::close(teleFd); teleFd=-1; }
        teleConnectBtn->setEnabled(true); teleDisconnectBtn->setEnabled(false);
        teleStatusLbl->setText("telemetry offline");
        teleLineBuf.clear();
    }

    bool sendSX(int bus, int adr, int val){
        if(fd<0) return false;
        const int targetBus = (bus==1) ? 1 : 0;
        bool ok = true;

        // Immer genau EINEN expliziten Bus senden (kein SX0/SX1-Mitsenden)
        ok = ok && wr2(fd, 0xFE, (targetBus==0)?0xB0:0xB1);
        usleep(4000);
        rtbsBus1 = (targetBus==1);

        uint8_t cmd = (uint8_t)(0x80 | (adr & 0x7F));
        uint8_t data = (uint8_t)(val & 0xFF);

        // Mehrfach senden fuer robuste Uebernahme am Interface
        for(int i=0;i<4;i++){
            ok = ok && wr2(fd, cmd, data);
            usleep(3000);
        }

        appendLog(QString("TX SX%1 ADR %2 cmd=%3 DATA=%4")
                  .arg(targetBus)
                  .arg(adr & 0x7F)
                  .arg(QString("0x%1").arg(cmd,2,16,QChar('0')).toUpper())
                  .arg((int)data));
        return ok;
    }

    void sendValue(){
        if(useSerialWizard() && visualSetupStarted){
            appendLog("WARN: SX Senden blockiert: Serial-Wizard aktiv (erst Setup Ende/Abort)");
            return;
        }
        if(fd<0){ appendLog("Senden fehlgeschlagen: offline"); return; }
        if(confirmBox->isChecked()){
            auto r = QMessageBox::question(this, "Senden", "Wert wirklich auf SX-Bus senden?");
            if(r != QMessageBox::Yes) return;
        }
        int adr = sendAdr->value() & 0x7F;
        int val = sendVal->value() & 0xFF;
        int bus = (sendBusBox->currentText()=="SX1") ? 1 : 0;

        bool ok = sendSX(bus, adr, val);
        uint8_t cmd = (uint8_t)(0x80 | adr);
        if(!ok){ appendLog("Senden fehlgeschlagen: write error"); return; }

        appendLog(QString("TX SX%1 ADR %2 cmd=%3 DATA=%4 bits=%5")
                  .arg(bus).arg(adr)
                  .arg(QString("0x%1").arg(cmd,2,16,QChar('0')).toUpper())
                  .arg(val)
                  .arg(bits8(val, bitOrderBox->isChecked())));
    }

    void openSwitchPanel(int row,int col){
        if(!(col==0 || col==1 || col==2 || col==3 || col==4 || col==5 || col==6 || col==7 || col==8 || col==9 || col==10 || col==11)) return;
        int block = col/3;
        int adr = block*28 + row;
        if(adr<0 || adr>111) return;
        int valCol = block*3+1;
        bool ok=false;
        int current = table->item(row,valCol)->text().toInt(&ok);
        if(!ok) current=0;

        QDialog dlg(this);
        dlg.setWindowTitle(QString("Weichenstellpult ADR %1").arg(adr));
        auto *vl = new QVBoxLayout(&dlg);
        auto *bitsL = new QHBoxLayout;
        QVector<QCheckBox*> boxes;
        for(int i=0;i<8;i++){
            auto *cb = new QCheckBox(QString("B%1").arg(i+1));
            cb->setChecked((current>>i)&1);
            boxes.push_back(cb); bitsL->addWidget(cb);
        }
        auto *busSelL = new QHBoxLayout;
        auto *busSel = new QComboBox;
        busSel->addItems({"SX0","SX1"});
        busSel->setCurrentText(sendBusBox->currentText());
        busSelL->addWidget(new QLabel("Bus:")); busSelL->addWidget(busSel);
        auto *valLbl = new QLabel(QString("Wert(Byte): %1").arg(current));
        vl->addLayout(bitsL); vl->addLayout(busSelL); vl->addWidget(valLbl);
        auto recalc=[&](){ int v=0; for(int i=0;i<8;i++) if(boxes[i]->isChecked()) v|=(1<<i); valLbl->setText(QString("Wert(Byte): %1").arg(v)); };
        for(auto *cb: boxes) connect(cb,&QCheckBox::toggled,&dlg,recalc);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText("Senden");
        vl->addWidget(bb);
        connect(bb,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);
        connect(bb,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
        if(dlg.exec()!=QDialog::Accepted) return;
        int v=0; for(int i=0;i<8;i++) if(boxes[i]->isChecked()) v|=(1<<i);
        sendBusBox->setCurrentText(busSel->currentText());
        sendAdr->setValue(adr);
        sendVal->setValue(v);
        sendValue();
    }

    void pollSerial(){
        pollTelemetry();
        checkAckTimeouts();
        if(fd<0) return;
        if(rxPauseBox && rxPauseBox->isChecked()) return;
        uint8_t buf[512];
        int n = ::read(fd, buf, sizeof(buf));
        if(n<=0) return;

        for(int i=0;i<n;i++){
            uint8_t b=buf[i];
            if(pending<0){
                // Nur gueltige SX-Adressbytes als Frame-Start akzeptieren
                // (ASCII/Binaermuell soll nicht als SX-Frame fehlinterpretiert werden)
                if((b & 0x80)==0) continue;
                pending=b;
                continue;
            }
            uint8_t adr_raw = (uint8_t)pending;
            pending=-1;
            int adr = adr_raw & 0x7F;
            int bus = (adr_raw & 0x80) ? 1 : 0;
            int d = (int)b;
            if(adr<0 || adr>=112) continue;
            if(uptime.elapsed() < rxWarmupUntilMs) continue; // Connect-Start-Rauschen ignorieren

            if(adr==15){
                QString st="idle";
                if(d==1) st="ok"; else if(d==2) st="error"; else if(d==3) st="busy";
                progStatusLbl->setText(QString("Status K15 (SX%1): %2 (%3)").arg(bus).arg(st).arg(d));
            }

            if(bus==0){
                if(sx0[adr]==d) continue;
                int old=sx0[adr]; sx0[adr]=d;
                if(busBox->currentText()=="SX0" || busBox->currentText()=="SX0+SX1") updateRow(adr,0,d);
                logChange(adr,0,old,d);
            } else {
                if(sx1[adr]==d) continue;
                int old=sx1[adr]; sx1[adr]=d;
                if(busBox->currentText()=="SX1" || busBox->currentText()=="SX0+SX1") updateRow(adr,1,d);
                logChange(adr,1,old,d);
            }
        }
    }

    void pollTelemetry(){
        if(teleFd<0) return;
        char buf[512];
        int n = ::read(teleFd, buf, sizeof(buf));
        if(n<=0) return;
        for(int i=0;i<n;++i){
            unsigned char uc = (unsigned char)buf[i];
            bool isAscii = (uc==9 || uc==10 || uc==13 || (uc>=32 && uc<=126));
            if(isAscii) teleAsciiScore++; else teleBinaryScore++;

            if(teleBinaryScore > 60 && teleBinaryScore > (teleAsciiScore*2 + 20)){
                appendLog("WARN: Telemetrie-Port liefert überwiegend Binärdaten. Vermutlich falscher Port (SLX statt Arduino). Trenne Telemetrie.");
                doTeleDisconnect();
                return;
            }

            char c = (char)uc;
            if(c=='\r') continue;
            if(c=='\n'){
                QString line = QString::fromUtf8(teleLineBuf).trimmed();
                teleLineBuf.clear();
                if(!line.isEmpty()) parseTelemetryLine(line);
            } else if(isAscii) {
                teleLineBuf.append(c);
                if(teleLineBuf.size()>4096) teleLineBuf.clear();
            }
        }
    }

    void parseTelemetryLine(const QString &line){
        if(line=="RX:" || line.startsWith("RX:\uFFFD") || line.startsWith("RX:?")) return;
        bool knownPrefix = line.startsWith("HELLO ") || line.startsWith("ACK_") || line.startsWith("CFG_") || line.startsWith("RX:") || line.startsWith("FW-Version:") || line.startsWith("SX30 ServoDecoder start") || line.startsWith("Setup starten:") || line.startsWith("CFG:");
        if(!knownPrefix){
            appendLog(QString("TEL(noise): %1").arg(line));
            return;
        }
        appendLog(QString("TEL: %1").arg(line));

        if(line.startsWith("HELLO ")){
            QRegularExpression re("decoder=([^\\s]+)\\s+fw=([^\\s]+)\\s+proto=(\\d+)");
            auto m = re.match(line);
            if(m.hasMatch()){
                QString decoder = m.captured(1);
                QString fw = m.captured(2);
                int proto = m.captured(3).toInt();
                bool ok = (decoder=="servodecoder" && proto>=1);
                fwStatusLbl->setText(QString("FW: %1 (%2 p%3)").arg(ok?"OK":"UPDATE NÖTIG").arg(fw).arg(proto));
            }
            return;
        }

        if(line.startsWith("ACK_SETUP_MOVE ")){
            QRegularExpression re("servo=(\\d+)\\s+rel=(-?\\d+)");
            auto m = re.match(line);
            if(m.hasMatch()){
                int s = m.captured(1).toInt()-1;
                int rel = m.captured(2).toInt();
                if(s>=0 && s<16){
                    visualSetupStarted = true;
                    visualSetupArmed = false;
                    if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: AKTIV (ACK_SETUP_MOVE)");
                    servoArmPos[s] = rel;
                    armLiveValid[s] = true;
                    updateServoArmLabel(s);
                    setAckOk(s, "move");
                }
            }
            return;
        }

        if(line.startsWith("ACK_SETUP_STATE ")){
            QRegularExpression re("servo=(\\d+)");
            auto m = re.match(line);
            if(m.hasMatch()){
                int s = m.captured(1).toInt()-1;
                if(s>=0 && s<16){
                    if(wizardLockedServo>=0 && s!=wizardLockedServo && line.contains("action=select")){
                        appendLog(QString("TEL: ACK_SETUP_STATE select S%1 ignoriert (Wizard-Lock S%2)").arg(s+1).arg(wizardLockedServo+1));
                        return;
                    }
                    visualSetupStarted = true;
                    visualSetupArmed = false;
                    if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: AKTIV (ACK_SETUP_STATE)");
                    if(line.contains("action=mid")){
                        servoArmPos[s] = 0;
                        armLiveValid[s] = true;
                        updateServoArmLabel(s);
                        setAckOk(s, "move");
                    } else if(line.contains("action=select")){
                        // Select bestaetigt aktiven Wizard/Servo; offene Move-Wartezustände für diesen Servo freigeben
                        if(!ackPendingType[s].isEmpty() && ackPendingType[s] == "move") setAckOk(s, "move");
                    }
                }
            }
            return;
        }

        if(line.startsWith("ACK_SETUP_STORE ")){
            QRegularExpression re("servo=(\\d+)");
            auto m = re.match(line);
            if(m.hasMatch()){
                int s = m.captured(1).toInt()-1;
                if(s>=0 && s<16) setAckOk(s, "store");
                visualSetupStarted = true;
                visualSetupArmed = false;
                if(visualProgStateLbl) visualProgStateLbl->setText("Progstatus: AKTIV (ACK_SETUP_STORE)");
            }
            return;
        }

        if(line.startsWith("CFG_HDR ")){
            cfgSeenCount = 0;
            cfgImportInProgress = true;
            appendLog("TEL: CFG-Import gestartet");
            return;
        }

        if(line.startsWith("CFG_S ")){
            QRegularExpression re("servo=(\\d+)\\s+zero=(\\d+)\\s+relMin=(-?\\d+)\\s+relMax=(-?\\d+)\\s+divLeft=(\\d+)");
            auto m = re.match(line);
            if(m.hasMatch()){
                int s = m.captured(1).toInt()-1;
                int zero = m.captured(2).toInt();
                int relMin = m.captured(3).toInt();
                int relMax = m.captured(4).toInt();
                int divLeft = m.captured(5).toInt();
                if(s>=0 && s<16){
                    if(!armLiveValid[s]){
                        int mid = (relMin + relMax) / 2;
                        servoArmPos[s] = mid;
                        updateServoArmLabel(s);
                    }
                    if(servoTable){
                        servoTable->item(s,1)->setText(QString::number(zero));
                        servoTable->item(s,2)->setText(QString::number(relMin + 90));
                        servoTable->item(s,3)->setText(QString::number(relMax + 90));
                        servoTable->item(s,4)->setText(QString::number(divLeft));
                    }
                    cfgSeenCount++;
                }
            }
            return;
        }

        if(line.startsWith("CFG_END")){
            cfgImportInProgress = false;
            appendLog(QString("TEL: CFG-Import fertig (%1 Servos)").arg(cfgSeenCount));
            if(cfgSeenCount < 16){
                appendLog("WARN: CFG-Import unvollstaendig");
            } else if(ackCfgFallbackBox && ackCfgFallbackBox->isChecked()){
                for(int s=0; s<16; ++s){
                    if(ackPendingType[s].isEmpty()) continue;
                    ackPendingType[s].clear();
                    ackPendingSinceMs[s] = 0;
                    ackVisualState[s] = "cfg-fallback";
                    setServoTileInputEnabled(s, true);
                    appendLog(QString("V2 ACK cfg-fallback: S%1").arg(s+1));
                }
                updateVisualTitles();
            }
            return;
        }
    }

private:
    void updateRow(int adr,int bus,int d){
        Q_UNUSED(bus);
        int row = adr % 28;
        int block = adr / 28;
        int base = block * 3;
        table->item(row,base+1)->setText(QString::number(d));
        table->item(row,base+2)->setText(bits8(d, bitOrderBox->isChecked()));
        for(int c=base;c<base+3;c++){
            table->item(row,c)->setForeground(QBrush(Qt::black));
            table->item(row,c)->setBackground(QColor(255,245,170));
        }
    }

    void logChange(int adr,int bus,int oldv,int newv){
        QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        appendLog(QString("%1  SX%2 ADR %3  %4 -> %5  bits=%6")
                  .arg(ts).arg(bus).arg(adr).arg(oldv).arg(newv).arg(bits8(newv, bitOrderBox->isChecked())));
    }

    void appendLog(const QString& s){ logView->append(s); }
    void updateServoArmLabel(int s){
        if(s<0 || s>=16 || !servoArmWidgets[s]) return;
        if(servoArmPos[s] < -90) servoArmPos[s] = -90;
        if(servoArmPos[s] > 90) servoArmPos[s] = 90;
        servoArmWidgets[s]->setAngleDeg(servoArmPos[s]);
    }
    void setServoTileInputEnabled(int s, bool enabled){
        if(s<0 || s>=16 || !visualServoBoxes[s]) return;
        visualServoBoxes[s]->setEnabled(enabled);
    }
    void setAckPending(int s, const QString &type, int stepCount=1){
        if(s<0 || s>=16) return;
        ackPendingType[s] = type;
        setServoTileInputEnabled(s, false);
        ackPendingSinceMs[s] = uptime.elapsed();
        qint64 tmo = ackTimeoutBaseMs;
        if(type=="move") tmo += (qint64)std::max(0, stepCount-1) * ackTimeoutPerExtraStepMs;
        ackTimeoutForServoMs[s] = tmo;
        ackVisualState[s] = "pending";
        updateVisualTitles();
    }
    void setAckOk(int s, const QString &type){
        if(s<0 || s>=16) return;
        if(ackPendingType[s] == type){
            ackPendingType[s].clear();
            ackPendingSinceMs[s] = 0;
            ackTimeoutForServoMs[s] = ackTimeoutBaseMs;
            ackVisualState[s] = "ok";
            setServoTileInputEnabled(s, true);
            updateVisualTitles();
            appendLog(QString("V2 ACK ok: S%1 %2").arg(s+1).arg(type));
            if(type=="move" && moveQueue[s] != 0){
                int dir = (moveQueue[s] > 0) ? 2 : 1;
                moveQueue[s] += (moveQueue[s] > 0) ? -1 : +1;
                sendVisualWizardMove(s, dir);
                appendLog(QString("V2 Queue: S%1 Rest=%2").arg(s+1).arg(moveQueue[s]));
            }
        }
    }
    void checkAckTimeouts(){
        const qint64 now = uptime.elapsed();
        for(int s=0; s<16; ++s){
            if(ackPendingType[s].isEmpty()) continue;
            if((now - ackPendingSinceMs[s]) >= ackTimeoutForServoMs[s]){
                appendLog(QString("V2 ACK timeout: S%1 %2 (keine echte Decoder-ACK-Zeile)").arg(s+1).arg(ackPendingType[s]));
                ackPendingType[s].clear();
                ackPendingSinceMs[s] = 0;
                ackVisualState[s] = "timeout";
                setServoTileInputEnabled(s, true);
                if(visualProgStateLbl && visualSetupArmed && !visualSetupStarted){
                    visualProgStateLbl->setText("Progstatus: INAKTIV (kein ACK, bitte lokale Taste/D13 prüfen)");
                }
                updateVisualTitles();
            }
        }
    }
    void updateVisualTitles(){
        int a = visualAddrA ? visualAddrA->value() : 1;
        int b = visualAddrB ? visualAddrB->value() : 0;
        bool bit1Left = visualBitOrder ? visualBitOrder->isChecked() : true;
        bool serialWizardActive = useSerialWizard() && visualSetupStarted;
        for(int s=0; s<16; ++s){
            if(!visualServoBoxes[s]) continue;
            bool tileEnabled = visualSetupStarted || (ackVisualState[s] == "pending");
            visualServoBoxes[s]->setEnabled(tileEnabled);
            int bit = (s % 8) + 1;
            int shown = bit1Left ? bit : (9-bit);
            int adr = (s < 8) ? a : b;
            QString ackTag;
            if(ackVisualState[s] == "pending") ackTag = " | ACK:pending";
            else if(ackVisualState[s] == "ok") ackTag = " | ACK:ok";
            else if(ackVisualState[s] == "cfg-fallback") ackTag = " | ACK:cfg-fallback";
            else if(ackVisualState[s] == "timeout") ackTag = " | ACK:timeout";
            QString qTag;
            if(moveQueue[s] != 0) qTag = QString(" | Q:%1").arg(moveQueue[s]);
            visualServoBoxes[s]->setTitle(QString("S%1 | Adr %2 | Bit %3%4%5").arg(s+1).arg(adr).arg(shown).arg(ackTag).arg(qTag));
        }
        if(sendBtn) sendBtn->setEnabled(!serialWizardActive);
        if(quick0Btn) quick0Btn->setEnabled(!serialWizardActive);
        if(quick1Btn) quick1Btn->setEnabled(!serialWizardActive);
        if(quick255Btn) quick255Btn->setEnabled(!serialWizardActive);
        if(visualSetupRequestBtn) visualSetupRequestBtn->setEnabled(!visualSetupStarted);
        if(visualSetupSaveBtn) visualSetupSaveBtn->setEnabled(visualSetupStarted);
        if(visualSetupAbortBtn) visualSetupAbortBtn->setEnabled(visualSetupStarted);
    }

    QComboBox *ifaceBox{}, *busBox{};
    QCheckBox *bitOrderBox{};
    QLineEdit *portEdit{};
    QComboBox *baudBox{};
    QLineEdit *telePortEdit{};
    QComboBox *teleBaudBox{};
    QPushButton *teleConnectBtn{}, *teleDisconnectBtn{}, *teleReqHelloBtn{}, *teleReqCfgBtn{};
    QCheckBox *ackCfgFallbackBox{};
    QPushButton *connectBtn{}, *disconnectBtn{}, *sendBtn{};
    QComboBox *sendBusBox{}, *bitButtonsBox{};
    QSpinBox *sendAdr{}, *sendVal{};
    QPushButton *quick0Btn{}, *quick1Btn{}, *quick255Btn{};
    QCheckBox *confirmBox{};
    QCheckBox *rxPauseBox{};
    QSpinBox *progAddrA{}, *progAddrB{}, *progServoIdx{};
    QComboBox *progStep{};
    QPushButton *progOnBtn{}, *progOffBtn{}, *progStartBtn{}, *progSaveBtn{}, *progAbortBtn{}, *progCommitAllBtn{}, *progMoveMinusBtn{}, *progMovePlusBtn{}, *progMidBtn{}, *progStoreLBtn{}, *progStoreRBtn{};
    QLabel *statusLbl{}, *infoLbl{}, *progStatusLbl{}, *teleStatusLbl{}, *fwStatusLbl{}, *ackModeLbl{};
    ServoArmWidget* servoArmWidgets[16]{};
    QGroupBox* visualServoBoxes[16]{};
    QSpinBox *visualAddrA{}, *visualAddrB{}, *visualLimitSpin{};
    QCheckBox *visualBitOrder{};
    QComboBox *progPathBox{};
    QPushButton *visualSetupRequestBtn{}, *visualSetupSaveBtn{}, *visualSetupAbortBtn{};
    QLabel *visualProgStateLbl{};
    int servoArmPos[16]{};
    bool armLiveValid[16]{};
    QTableWidget *servoTable{};
    QTableWidget *table{};
    QTextEdit *logView{};
    QTimer *timer{};

    int fd=-1, pending=-1;
    int teleFd=-1;
    QByteArray teleLineBuf;
    int teleBinaryScore = 0;
    int teleAsciiScore = 0;
    bool rtbsBus1=false;
    QElapsedTimer uptime;
    QString appVersion = APP_VERSION;
    qint64 rxWarmupUntilMs = 0;
    int cfgSeenCount = 0;
    bool cfgImportInProgress = false;
    int moveAutoCfgCounter = 0;
    bool visualSetupArmed=false;
    bool visualSetupStarted=false;
    QString ackPendingType[16];
    qint64 ackPendingSinceMs[16]{};
    qint64 ackTimeoutForServoMs[16]{};
    QString ackVisualState[16];
    int moveQueue[16]{};
    uint8_t sxWizardSessionId = 0;
    int wizardLockedServo = -1;
    const qint64 ackTimeoutBaseMs = 1800;
    const qint64 ackTimeoutPerExtraStepMs = 180;
    int sx0[112], sx1[112];
};

#include "sx_monitor_qt_v2.moc"

int main(int argc,char**argv){
    QApplication app(argc,argv);
    MainWin w; w.show();
    return app.exec();
}
