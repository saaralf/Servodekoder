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

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

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
static QString bits8(int v, bool bit1Left){
    QString s;
    if(bit1Left){ // optisch: Bit1..Bit8 von links nach rechts
        for(int i=0;i<8;++i) s += ((v>>i)&1)?'1':'0';
    } else { // technisch: Bit8..Bit1 von links nach rechts
        for(int i=7;i>=0;--i) s += ((v>>i)&1)?'1':'0';
    }
    return s;
}

class MainWin : public QMainWindow {
    Q_OBJECT
public:
    MainWin(){
        setWindowTitle("SX Monitor – SLX852 (SX0/SX1)");
        resize(1280, 860);

        auto *cw = new QWidget; setCentralWidget(cw);
        auto *root = new QVBoxLayout(cw);

        auto *cfg = new QGroupBox("Interface / Zentrale (nach SX1-Doku)");
        auto *cfgL = new QHBoxLayout(cfg);
        ifaceBox = new QComboBox; ifaceBox->addItems({"SLX852"});
        busBox = new QComboBox; busBox->addItems({"SX0","SX1","SX0+SX1"});
        portEdit = new QLineEdit("/dev/ttyUSB0");
        baudEdit = new QLineEdit("57600");
        bitOrderBox = new QCheckBox("Bit 1 links / Bit 8 rechts (SX-Optik)");
        bitOrderBox->setChecked(true);
        connectBtn = new QPushButton("Connect");
        disconnectBtn = new QPushButton("Disconnect"); disconnectBtn->setEnabled(false);
        statusLbl = new QLabel("offline");
        cfgL->addWidget(new QLabel("Interface:")); cfgL->addWidget(ifaceBox);
        cfgL->addWidget(new QLabel("Busse:")); cfgL->addWidget(busBox);
        cfgL->addWidget(new QLabel("Port:")); cfgL->addWidget(portEdit);
        cfgL->addWidget(new QLabel("Baud:")); cfgL->addWidget(baudEdit);
        cfgL->addWidget(bitOrderBox);
        cfgL->addWidget(connectBtn); cfgL->addWidget(disconnectBtn);
        cfgL->addWidget(statusLbl);
        root->addWidget(cfg);

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
        progParamId = new QComboBox; progParamId->addItems({"1 zeroPhys","2 relMin (v+90)","3 relMax (v+90)","4 divLeft"});
        progParamVal = new QSpinBox; progParamVal->setRange(0,255);
        progCommitBtn = new QPushButton("Commit (K6=1)");
        progSaveBtn = new QPushButton("Save EEPROM (K7=1)");
        progDefaultsBtn = new QPushButton("Defaults (K7=2)");
        r2->addWidget(new QLabel("K3 Servo")); r2->addWidget(progServoIdx);
        r2->addWidget(new QLabel("K4 Param")); r2->addWidget(progParamId);
        r2->addWidget(new QLabel("K5 Wert")); r2->addWidget(progParamVal);
        r2->addWidget(progCommitBtn); r2->addWidget(progSaveBtn); r2->addWidget(progDefaultsBtn);
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
            connect(bMid,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,100,s); sendSX(bus,101,2); appendLog(QString("TEST Mitte S%1").arg(s)); });
            connect(bG,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,100,s); sendSX(bus,101,3); appendLog(QString("TEST Gerade S%1").arg(s)); });
            connect(bA,&QPushButton::clicked,this,[this,s](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,100,s); sendSX(bus,101,4); appendLog(QString("TEST Abzweig S%1").arg(s)); });
            connect(bC,&QPushButton::clicked,this,[this,s](){ commitServoRow(s); });
        }
        progL->addWidget(servoTable);

        auto *r3 = new QHBoxLayout;
        progCommitAllBtn = new QPushButton("Alle Commit");
        r3->addWidget(progCommitAllBtn);
        progL->addLayout(r3);

        auto *sx1Hint = new QLabel("SX1-Stil: Änderungen werden sofort wirksam. Programmiermodus mit Track AUS aktiv, Ende per Track EIN oder Prog-Taste.");
        progL->addWidget(sx1Hint);
        progStatusLbl = new QLabel("Status K8: -");
        progL->addWidget(progStatusLbl);

        tabs->addTab(progTab, "Servo-Programmer");
        root->addWidget(tabs);

        auto *sendBox = new QGroupBox("SX senden");
        auto *sendL = new QHBoxLayout(sendBox);
        sendBusBox = new QComboBox; sendBusBox->addItems({"SX0","SX1"});
        sendAdr = new QSpinBox; sendAdr->setRange(0,111);
        sendVal = new QSpinBox; sendVal->setRange(0,255);
        sendBtn = new QPushButton("Senden");
        quick0Btn = new QPushButton("0");
        quick1Btn = new QPushButton("1");
        quick255Btn = new QPushButton("255");
        confirmBox = new QCheckBox("Senden bestätigen");
        bitButtonsBox = new QComboBox;
        bitButtonsBox->addItems({"Bitbuttons aus","Bitbuttons ein"});

        sendL->addWidget(new QLabel("Bus:")); sendL->addWidget(sendBusBox);
        sendL->addWidget(new QLabel("Adresse:")); sendL->addWidget(sendAdr);
        sendL->addWidget(new QLabel("Wert:")); sendL->addWidget(sendVal);
        sendL->addWidget(quick0Btn); sendL->addWidget(quick1Btn); sendL->addWidget(quick255Btn);
        sendL->addWidget(sendBtn);
        sendL->addWidget(confirmBox);
        root->addWidget(sendBox);

        infoLbl = new QLabel("Änderungen gelb markiert • FE A0 bei Connect");
        root->addWidget(infoLbl);

        for(int i=0;i<112;i++){ sx0[i]=-1; sx1[i]=-1; }

        timer = new QTimer(this);
        connect(timer,&QTimer::timeout,this,&MainWin::pollSerial);
        connect(connectBtn,&QPushButton::clicked,this,&MainWin::doConnect);
        connect(disconnectBtn,&QPushButton::clicked,this,&MainWin::doDisconnect);
        connect(sendBtn,&QPushButton::clicked,this,&MainWin::sendValue);
        connect(quick0Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(0); sendValue(); });
        connect(quick1Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(1); sendValue(); });
        connect(quick255Btn,&QPushButton::clicked,this,[this](){ sendVal->setValue(255); sendValue(); });
        connect(table,&QTableWidget::cellDoubleClicked,this,&MainWin::openSwitchPanel);

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
        connect(progCommitBtn,&QPushButton::clicked,this,[this](){
            int pid = progParamId->currentIndex()+1;
            int bus = (sendBusBox->currentText()=="SX1") ? 1 : 0;
            sendSX(bus, 3, progServoIdx->value()-1);
            sendSX(bus, 4, pid);
            sendSX(bus, 5, progParamVal->value());
            sendSX(bus, 6, 1);
            appendLog(QString("PROG COMMIT: S%1 PID%2 VAL%3").arg(progServoIdx->value()).arg(pid).arg(progParamVal->value()));
        });
        connect(progSaveBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,7,1); appendLog("PROG SAVE EEPROM"); });
        connect(progDefaultsBtn,&QPushButton::clicked,this,[this](){ int bus=(sendBusBox->currentText()=="SX1")?1:0; sendSX(bus,7,2); appendLog("PROG DEFAULTS"); });
        connect(progCommitAllBtn,&QPushButton::clicked,this,[this](){ for(int s=0;s<16;++s) commitServoRow(s); appendLog("PROG ALLE COMMIT"); });
    }
    ~MainWin(){ doDisconnect(); }

private slots:
    void doConnect(){
        doDisconnect();
        if(ifaceBox->currentText() == "SLX852") baudEdit->setText("57600");

        fd = open(portEdit->text().toUtf8().constData(), O_RDWR|O_NOCTTY|O_SYNC);
        if(fd<0){ statusLbl->setText("open failed"); return; }
        int baud = baudEdit->text().toInt();
        if(!set_serial(fd, baud)){ statusLbl->setText("serial cfg failed"); ::close(fd); fd=-1; return; }

        wr2(fd, 0xFE, 0xA0); usleep(20000);   // Monitor+Feedback
        wr2(fd, 0xFE, 0xB0); usleep(10000);   // Start mit Bus 0 selektiert
        rtbsBus1 = false;
        pending=-1;
        timer->start(25);
        statusLbl->setText("online");
        connectBtn->setEnabled(false); disconnectBtn->setEnabled(true);
        appendLog("Connect ok, FE A0 gesendet.");
    }

    void doDisconnect(){
        timer->stop();
        if(fd>=0){ ::close(fd); fd=-1; }
        connectBtn->setEnabled(true); disconnectBtn->setEnabled(false);
        statusLbl->setText("offline");
    }

    bool sendSX(int bus, int adr, int val){
        if(fd<0) return false;
        bool ok = true;
        if(bus==0){
            if(rtbsBus1){ ok = ok && wr2(fd, 0xFE, 0xB0); usleep(4000); rtbsBus1=false; }
        } else {
            if(!rtbsBus1){ ok = ok && wr2(fd, 0xFE, 0xB1); usleep(4000); rtbsBus1=true; }
        }
        uint8_t cmd = (uint8_t)(0x80 | (adr & 0x7F));
        for(int i=0;i<3;i++){ ok = ok && wr2(fd, cmd, (uint8_t)(val & 0xFF)); usleep(3000); }
        return ok;
    }

    void sendValue(){
        if(fd<0){ appendLog("Senden fehlgeschlagen: offline"); return; }
        if(confirmBox->isChecked()){
            auto r = QMessageBox::question(this, "Senden", "Wert wirklich auf SX-Bus senden?");
            if(r != QMessageBox::Yes) return;
        }
        int adr = sendAdr->value() & 0x7F;
        int val = sendVal->value() & 0xFF;
        int bus = (sendBusBox->currentText()=="SX1") ? 1 : 0;

        // srcpd/selectrix.c kompatibel:
        // selRautenhaus(bus) -> write(SXwrite + addr) -> write(data)
        // Buswahl über RautenhsCC (FE): B0/B1
        bool ok = true;
        if(bus==0){
            if(rtbsBus1){ ok = ok && wr2(fd, 0xFE, 0xB0); usleep(4000); rtbsBus1=false; }
        } else {
            if(!rtbsBus1){ ok = ok && wr2(fd, 0xFE, 0xB1); usleep(4000); rtbsBus1=true; }
        }

        uint8_t cmd = (uint8_t)(0x80 | adr); // SXwrite + adr
        for(int i=0;i<4;i++){
            ok = ok && wr2(fd, cmd, (uint8_t)val);
            usleep(3000);
        }

        if(!ok){
            appendLog("Senden fehlgeschlagen: write error");
            return;
        }

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
        if(fd<0) return;
        uint8_t buf[512];
        int n = ::read(fd, buf, sizeof(buf));
        if(n<=0) return;

        for(int i=0;i<n;i++){
            uint8_t b=buf[i];
            if(pending<0){ pending=b; continue; }
            uint8_t adr_raw = (uint8_t)pending;
            pending=-1;
            int adr = adr_raw & 0x7F;
            int bus = (adr_raw & 0x80) ? 1 : 0;
            int d = (int)b;
            if(adr<0 || adr>=112) continue;

            if(bus==0 && adr==8){
                QString st="idle";
                if(d==1) st="ok"; else if(d==2) st="range-error"; else if(d==3) st="index-error"; else if(d==4) st="busy";
                progStatusLbl->setText(QString("Status: %1 (%2)").arg(st).arg(d));
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

private:
    void commitServoRow(int s){
        if(!servoTable) return;
        bool ok1=false,ok2=false,ok3=false,ok4=false;
        int zero=servoTable->item(s,1)->text().toInt(&ok1);
        int rmin=servoTable->item(s,2)->text().toInt(&ok2);
        int rmax=servoTable->item(s,3)->text().toInt(&ok3);
        int div=servoTable->item(s,4)->text().toInt(&ok4);
        if(!(ok1&&ok2&&ok3&&ok4)){ appendLog(QString("S%1: ungültige Werte").arg(s)); return; }
        zero=qBound(0,zero,180); rmin=qBound(0,rmin,90); rmax=qBound(90,rmax,180); div=div?1:0;
        sendSX(0,3,s);
        sendSX(0,4,1); sendSX(0,5,zero); sendSX(0,6,1);
        sendSX(0,4,2); sendSX(0,5,rmin); sendSX(0,6,1);
        sendSX(0,4,3); sendSX(0,5,rmax); sendSX(0,6,1);
        sendSX(0,4,4); sendSX(0,5,div);  sendSX(0,6,1);
        appendLog(QString("COMMIT ROW S%1 zero=%2 rmin=%3 rmax=%4 div=%5").arg(s).arg(zero).arg(rmin).arg(rmax).arg(div));
    }

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

    QComboBox *ifaceBox{}, *busBox{};
    QCheckBox *bitOrderBox{};
    QLineEdit *portEdit{}, *baudEdit{};
    QPushButton *connectBtn{}, *disconnectBtn{}, *sendBtn{};
    QComboBox *sendBusBox{}, *bitButtonsBox{};
    QSpinBox *sendAdr{}, *sendVal{};
    QPushButton *quick0Btn{}, *quick1Btn{}, *quick255Btn{};
    QCheckBox *confirmBox{};
    QSpinBox *progAddrA{}, *progAddrB{}, *progServoIdx{}, *progParamVal{};
    QComboBox *progParamId{};
    QPushButton *progOnBtn{}, *progOffBtn{}, *progCommitBtn{}, *progSaveBtn{}, *progDefaultsBtn{}, *progCommitAllBtn{};
    QLabel *statusLbl{}, *infoLbl{}, *progStatusLbl{};
    QTableWidget *servoTable{};
    QTableWidget *table{};
    QTextEdit *logView{};
    QTimer *timer{};

    int fd=-1, pending=-1;
    bool rtbsBus1=false;
    int sx0[112], sx1[112];
};

#include "sx_monitor_qt.moc"

int main(int argc,char**argv){
    QApplication app(argc,argv);
    MainWin w; w.show();
    return app.exec();
}
