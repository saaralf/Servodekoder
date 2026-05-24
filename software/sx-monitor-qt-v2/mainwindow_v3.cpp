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
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QTimer>
#include <QCursor>
#include <algorithm>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QToolButton>
#include <array>

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

static const char* kSxService = "sxbusd2.service";
static const char* kRmxService = "sxbusd-dual-rmx.service";
static const char* kSxDaemonBinary = "/opt/programme/selectrix/Servodekoder/software/sx-bus-core/sx_bus_daemon2";
static const char* kRmxDaemonBinary = "/opt/programme/selectrix/Servodekoder/software/sx-bus-core/sx_bus_daemon_dual";
static const char* kSxSocket = "/run/user/1000/sxbusd.sock";
static const char* kRmxSocket = "/tmp/sxbusd_rmx.sock";

static bool hasValidOverridePort(const QString& service){
    const QString path = QDir::homePath() + QString("/.config/systemd/user/%1.d/override.conf").arg(service);
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString txt = QString::fromUtf8(f.readAll());
    return txt.contains("/dev/serial/by-id/") || txt.contains("/dev/ttyUSB") || txt.contains("/dev/ttyACM");
}

void MainWindowV3::applyRmxCentralStyle(){
    setStyleSheet(R"RMX(
        QMainWindow, QScrollArea, QWidget {
            background: #66738d;
            color: #ffffff;
            font-family: Arial, Helvetica, sans-serif;
            font-size: 12px;
        }
        QMenuBar {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #78849b, stop:0.45 #4b5368, stop:1 #252936);
            color: #ffffff;
            border-bottom: 2px solid #05070d;
            padding: 2px 6px;
        }
        QMenuBar::item { background: transparent; padding: 5px 10px; }
        QMenuBar::item:selected { background: #1d3d70; border: 1px solid #9fb8de; }
        QMenu {
            background: #2f3648;
            color: #ffffff;
            border: 1px solid #05070d;
        }
        QMenu::item { padding: 5px 24px; }
        QMenu::item:selected { background: #2f6fb7; }
        QGroupBox {
            background: #66738d;
            color: #ffffff;
            font-weight: bold;
            border: 2px solid #11141c;
            border-top: 22px solid #11141c;
            margin-top: 24px;
            padding: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 8px;
            top: 3px;
            color: #ffffff;
            font-size: 15px;
            font-weight: bold;
        }
        QLabel { color: #ffffff; }
        QLineEdit, QSpinBox, QComboBox {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f7f8fb, stop:1 #c8d0df);
            color: #0b1020;
            border: 1px solid #101522;
            min-height: 20px;
            padding: 1px 4px;
        }
        QPushButton {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f4f5f8, stop:0.15 #777f90, stop:0.55 #222837, stop:1 #05070d);
            color: #ffffff;
            border: 1px solid #000000;
            border-radius: 1px;
            padding: 4px 10px;
            font-weight: bold;
            min-height: 20px;
        }
        QPushButton:hover { border: 1px solid #b8d6ff; background: #2f6fb7; }
        QPushButton:pressed { background: #0d1d38; }
        QPushButton:disabled { color: #9ca4b2; background: #d8dbe0; }
        QTabWidget::pane { border: 2px solid #11141c; background: #66738d; }
        QTabBar::tab {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #808aa0, stop:1 #202637);
            color: #ffffff;
            border: 1px solid #05070d;
            padding: 6px 14px;
            font-weight: bold;
        }
        QTabBar::tab:selected { background: #3d74a7; }
        QTextEdit, QPlainTextEdit {
            background: #111723;
            color: #bfe2ff;
            border: 2px solid #05070d;
            selection-background-color: #2f6fb7;
            font-family: "DejaVu Sans Mono", Consolas, monospace;
        }
        QHeaderView::section {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #e7ebf2, stop:1 #8f9aae);
            color: #111827;
            border: 1px solid #303846;
            padding: 3px;
            font-weight: bold;
        }
        QTableWidget {
            background: #e8ecf3;
            alternate-background-color: #d7dde8;
            color: #0b1020;
            gridline-color: #7d8798;
            border: 2px solid #11141c;
        }
        QTableWidget::item:selected { background: #2f6fb7; color: white; }
        QCheckBox { color: #ffffff; spacing: 6px; }
        QProgressBar {
            border: 1px solid #11141c;
            background: #d8dbe0;
            color: #111827;
            text-align: center;
        }
        QProgressBar::chunk { background: #2f6fb7; }
    )RMX");
}

bool MainWindowV3::systemctlUser(const QStringList& args, QString* output){
    QProcess p;
    QStringList fullArgs;
    fullArgs << "--user";
    fullArgs << args;
    p.start("systemctl", fullArgs);
    if(!p.waitForFinished(8000)){
        p.kill();
        p.waitForFinished(1000);
        if(output) *output = "timeout";
        return false;
    }
    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromLocal8Bit(p.readAllStandardError()).trimmed();
    if(output) *output = out + (err.isEmpty() ? QString() : QString("\n") + err);
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

bool MainWindowV3::serviceActive(const QString& service){
    QString out;
    const bool ok = systemctlUser({"is-active", "--quiet", service}, &out);
    Q_UNUSED(out);
    return ok;
}

void MainWindowV3::daemonServiceAction(const QString& service, const QString& action, const QString& label){
    QString out;
    const bool ok = systemctlUser({action, service}, &out);
    if(log){
        log->append(QString("Daemon %1 %2: %3%4")
            .arg(label, action, ok ? "OK" : "FAIL", out.isEmpty() ? QString() : QString(" — ") + out));
    }
}

void MainWindowV3::startRequiredDaemonsIfNeeded(){
    const struct Item { const char* service; const char* label; bool requirePort; } items[] = {
        {kSxService, "SX", true},
        {kRmxService, "RMX", true},
    };
    for(const auto& item : items){
        if(item.requirePort && !hasValidOverridePort(item.service)){
            if(log) log->append(QString("Daemon %1 übersprungen — kein USB-Port konfiguriert (%2)").arg(item.label, item.service));
            continue;
        }
        if(serviceActive(item.service)){
            if(log) log->append(QString("Daemon %1 läuft bereits (%2)").arg(item.label, item.service));
        } else {
            if(log) log->append(QString("Daemon %1 läuft nicht — starte %2").arg(item.label, item.service));
            daemonServiceAction(item.service, "start", item.label);
        }
    }
}

void MainWindowV3::connectBackendFromPanel(BackendKind backend){
    ConnectionPanel* panel = (backend==BackendKind::SX) ? sxPanel : rmxPanel;
    const char* label = (backend==BackendKind::SX) ? "SX" : "RMX";
    if(panel->endpoint().startsWith("daemon://")){
        const char* service = backend==BackendKind::SX ? kSxService : kRmxService;
        if(!hasValidOverridePort(service)){
            log->append(QString("%1 connect übersprungen — kein USB-Port konfiguriert. Bitte Konfiguration → SX/RMX USB-Port auswählen...").arg(label));
            return;
        }
        daemonServiceAction(service, "start", label);
    }
    bool ok = ctrl.connectBackend(backend, panel->endpoint(), panel->baud());
    log->append(QString("%1 connect %2").arg(label, ok?"OK":"FAIL"));
    if(!ok && panel->endpoint().startsWith("daemon://")){
        QTimer::singleShot(1000, this, [this, backend, panel, label]{
            bool retryOk = ctrl.connectBackend(backend, panel->endpoint(), panel->baud());
            log->append(QString("%1 connect retry nach Daemon-Start %2").arg(label, retryOk?"OK":"FAIL"));
        });
    }
}

void MainWindowV3::disconnectBackend(BackendKind backend){
    ctrl.disconnectBackend(backend);
    log->append(QString("%1 disconnect OK").arg(backend==BackendKind::SX ? "SX" : "RMX"));
}

QStringList MainWindowV3::scanUsbSerialPorts(QString* details) const{
    QStringList ports;
    QStringList lines;
    QDir byId("/dev/serial/by-id");
    const QFileInfoList infos = byId.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
    for(const QFileInfo& fi : infos){
        const QString path = fi.absoluteFilePath();
        const QString target = fi.symLinkTarget();
        ports << path;
        QString hint = "USB-Serial";
        const QString low = path.toLower();
        if(low.contains("rmx") || low.contains("rautenhaus")) hint = "RMX-Kandidat";
        else if(low.contains("bg02sg7m")) hint = "SX-Kandidat (einziger verbliebener Adapter im aktuellen Test)";
        else if(low.contains("a50285bi")) hint = "Arduino/Programmer-Kandidat (historisch)";
        else if(low.contains("ftf8nbf0")) hint = "SX-Kandidat (historisch)";
        lines << QString("%1 -> %2    [%3]").arg(path, target, hint);
    }
    if(ports.isEmpty()){
        QDir dev("/dev");
        const QFileInfoList devs = dev.entryInfoList({"ttyUSB*", "ttyACM*"}, QDir::System, QDir::Name);
        for(const QFileInfo& fi : devs){
            ports << fi.absoluteFilePath();
            lines << QString("%1    [Fallback ohne by-id]").arg(fi.absoluteFilePath());
        }
    }
    if(details) *details = lines.isEmpty() ? "Keine ttyUSB/ttyACM-Adapter gefunden." : lines.join('\n');
    return ports;
}

bool MainWindowV3::writeDaemonOverride(BackendKind backend, const QString& serialPath, QString* output){
    const bool sx = backend == BackendKind::SX;
    const QString service = sx ? kSxService : kRmxService;
    const QString dirPath = QDir::homePath() + QString("/.config/systemd/user/%1.d").arg(service);
    QDir().mkpath(dirPath);
    QFile f(dirPath + "/override.conf");
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)){
        if(output) *output = f.errorString();
        return false;
    }
    const QString exec = sx
        ? QString("%1 %2 19200 %3 slx825_sx0_stream").arg(kSxDaemonBinary, serialPath, kSxSocket)
        : QString("%1 %2 57600 %3 rmx").arg(kRmxDaemonBinary, serialPath, kRmxSocket);
    const QString content = QString("[Service]\nExecStart=\nExecStart=%1\n").arg(exec);
    f.write(content.toUtf8());
    f.close();
    QString out;
    bool ok = systemctlUser({"daemon-reload"}, &out);
    if(ok) ok = systemctlUser({"restart", service}, &out);
    if(output) *output = QString("%1\n%2").arg(content.trimmed(), out.trimmed()).trimmed();
    return ok;
}

void MainWindowV3::showUsbPortConfigDialog(){
    QString details;
    QStringList ports = scanUsbSerialPorts(&details);
    QDialog dlg(this);
    dlg.setWindowTitle("SX/RMX USB-Port konfigurieren");
    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout;
    auto* sxCombo = new QComboBox;
    auto* rmxCombo = new QComboBox;
    sxCombo->setEditable(true);
    rmxCombo->setEditable(true);
    sxCombo->addItems(ports);
    rmxCombo->addItems(ports);
    int sxIdx = 0;
    int rmxIdx = -1;
    for(int i=0;i<ports.size();++i){
        const QString low = ports[i].toLower();
        if(low.contains("bg02sg7m")) sxIdx = i;
        if(low.contains("rmx") || low.contains("rautenhaus")) rmxIdx = i;
    }
    if(sxCombo->count()>0) sxCombo->setCurrentIndex(sxIdx);
    if(rmxIdx >= 0) rmxCombo->setCurrentIndex(rmxIdx);
    else rmxCombo->setEditText(ports.isEmpty() ? QString() : QString("nicht angeschlossen / leer lassen"));
    form->addRow("SX / SLX825 Port:", sxCombo);
    form->addRow("RMX Port:", rmxCombo);
    layout->addLayout(form);
    auto* info = new QPlainTextEdit(details);
    info->setReadOnly(true);
    info->setMinimumHeight(130);
    layout->addWidget(new QLabel("Gefundene Adapter und Vorschläge:"));
    layout->addWidget(info);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* rescan = buttons->addButton("Neu suchen", QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(rescan, &QPushButton::clicked, &dlg, [&]{
        details.clear(); ports = scanUsbSerialPorts(&details);
        sxCombo->clear(); rmxCombo->clear(); sxCombo->addItems(ports); rmxCombo->addItems(ports);
        info->setPlainText(details);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if(dlg.exec() != QDialog::Accepted) return;

    const QString sxPort = sxCombo->currentText().trimmed();
    const QString rmxPort = rmxCombo->currentText().trimmed();
    QString out;
    if(!sxPort.isEmpty() && !sxPort.contains("nicht angeschlossen")){
        const bool ok = writeDaemonOverride(BackendKind::SX, sxPort, &out);
        log->append(QString("SX-Port-Konfig %1: %2").arg(ok?"OK":"FAIL", sxPort));
        if(!out.isEmpty()) log->append(out);
    }
    if(!rmxPort.isEmpty() && !rmxPort.contains("nicht angeschlossen")){
        const bool ok = writeDaemonOverride(BackendKind::RMX, rmxPort, &out);
        log->append(QString("RMX-Port-Konfig %1: %2").arg(ok?"OK":"FAIL", rmxPort));
        if(!out.isEmpty()) log->append(out);
    } else {
        log->append("RMX-Port-Konfig übersprungen (nicht angeschlossen/leer).");
    }
    QMessageBox::information(this, "USB-Port-Konfig", "Konfiguration geschrieben. Daemon wurde neu geladen/gestartet. Danach bitte neu verbinden oder Menü Verbindung → Alle verbinden nutzen.");
}

void MainWindowV3::setupDaemonMenu(){
    auto* configMenu = menuBar()->addMenu("Konfiguration");
    QAction* usbConfig = configMenu->addAction("SX/RMX USB-Port auswählen...");
    connect(usbConfig, &QAction::triggered, this, [this]{ showUsbPortConfigDialog(); });

    auto* daemonMenu = menuBar()->addMenu("Daemons");
    auto addAction = [this, daemonMenu](const QString& text, const QString& service, const QString& action, const QString& label){
        QAction* a = daemonMenu->addAction(text);
        connect(a, &QAction::triggered, this, [this, service, action, label]{ daemonServiceAction(service, action, label); });
    };
    addAction("SX-Daemon starten", kSxService, "start", "SX");
    addAction("SX-Daemon stoppen", kSxService, "stop", "SX");
    addAction("SX-Daemon neu starten", kSxService, "restart", "SX");
    daemonMenu->addSeparator();
    addAction("RMX-Daemon starten", kRmxService, "start", "RMX");
    addAction("RMX-Daemon stoppen", kRmxService, "stop", "RMX");
    addAction("RMX-Daemon neu starten", kRmxService, "restart", "RMX");
    daemonMenu->addSeparator();
    QAction* startBoth = daemonMenu->addAction("Alle Daemons starten");
    connect(startBoth, &QAction::triggered, this, [this]{ startRequiredDaemonsIfNeeded(); });
    QAction* restartBoth = daemonMenu->addAction("Alle Daemons neu starten");
    connect(restartBoth, &QAction::triggered, this, [this]{
        daemonServiceAction(kSxService, "restart", "SX");
        daemonServiceAction(kRmxService, "restart", "RMX");
    });
    QAction* stopBoth = daemonMenu->addAction("Alle Daemons stoppen");
    connect(stopBoth, &QAction::triggered, this, [this]{
        daemonServiceAction(kSxService, "stop", "SX");
        daemonServiceAction(kRmxService, "stop", "RMX");
    });

    auto* connMenu = menuBar()->addMenu("Verbindung");
    QAction* connectSx = connMenu->addAction("SX verbinden");
    connect(connectSx, &QAction::triggered, this, [this]{ connectBackendFromPanel(BackendKind::SX); });
    QAction* disconnectSx = connMenu->addAction("SX trennen");
    connect(disconnectSx, &QAction::triggered, this, [this]{ disconnectBackend(BackendKind::SX); });
    connMenu->addSeparator();
    QAction* connectRmx = connMenu->addAction("RMX verbinden");
    connect(connectRmx, &QAction::triggered, this, [this]{ connectBackendFromPanel(BackendKind::RMX); });
    QAction* disconnectRmx = connMenu->addAction("RMX trennen");
    connect(disconnectRmx, &QAction::triggered, this, [this]{ disconnectBackend(BackendKind::RMX); });
    connMenu->addSeparator();
    QAction* connectBoth = connMenu->addAction("Alle verbinden");
    connect(connectBoth, &QAction::triggered, this, [this]{ connectBackendFromPanel(BackendKind::SX); connectBackendFromPanel(BackendKind::RMX); });
    QAction* disconnectBoth = connMenu->addAction("Alle trennen");
    connect(disconnectBoth, &QAction::triggered, this, [this]{ disconnectBackend(BackendKind::SX); disconnectBackend(BackendKind::RMX); });
}

MainWindowV3::MainWindowV3(QWidget* parent): QMainWindow(parent){
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    setCentralWidget(scroll);
    auto* c = new QWidget;
    scroll->setWidget(c);
    auto* l = new QVBoxLayout(c);
    setWindowTitle("SX/RMX Monitor Qt V3 — RMX-Zentrale Stil");
    applyRmxCentralStyle();
    sxPanel = new ConnectionPanel("SX", "daemon:///run/user/1000/sxbusd.sock", 19200);
    rmxPanel = new ConnectionPanel("RMX", "daemon:///tmp/sxbusd_rmx.sock", 57600);
    log = new QTextEdit; log->setReadOnly(true);
    setupDaemonMenu();
    QTimer::singleShot(0, this, [this]{ startRequiredDaemonsIfNeeded(); });

    auto* trackPanel = new QWidget;
    auto* trackPanelL = new QHBoxLayout(trackPanel);
    trackPanelL->setContentsMargins(4, 4, 4, 4);
    auto* trackStartBtn = new QPushButton("START");
    trackStartBtn->setMinimumSize(78, 54);
    trackStartBtn->setStyleSheet("QPushButton{font-size:18px; font-weight:900; color:white; background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #4f5c75,stop:0.45 #111827,stop:1 #05070d); border:2px solid #0b0f18;} QPushButton:hover{border:2px solid #dbeafe; background:#1d4f91;}");
    auto* trackStatusBox = new QLabel;
    trackStatusBox->setFixedSize(64, 54);
    trackStatusBox->setToolTip("Gleisstatus: rot=AUS, grün=AN, grau=unbekannt");
    auto* trackText = new QLabel("Gleis: ?");
    trackText->setMinimumWidth(120);
    auto trackState = std::make_shared<int>(-1);
    auto updateTrackPanel = [trackStatusBox, trackText, trackStartBtn, trackState](int t){
        *trackState = t;
        if(t == 1){
            trackStatusBox->setStyleSheet("QLabel{background:#10c020; border:2px solid #041006;}");
            trackText->setText("Gleis: AN");
            trackStartBtn->setText("STOP");
        } else if(t == 0){
            trackStatusBox->setStyleSheet("QLabel{background:#e00000; border:2px solid #160000;}");
            trackText->setText("Gleis: AUS");
            trackStartBtn->setText("START");
        } else {
            trackStatusBox->setStyleSheet("QLabel{background:#606878; border:2px solid #10141c;}");
            trackText->setText("Gleis: ?");
            trackStartBtn->setText("START");
        }
    };
    updateTrackPanel(-1);
    trackPanelL->addWidget(trackStartBtn);
    trackPanelL->addWidget(trackStatusBox);
    trackPanelL->addWidget(trackText);
    trackPanelL->addStretch(1);

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
    auto* sxBitOrderSwitch = new QCheckBox("Bitreihenfolge umkehren");
    sxBitOrderSwitch->setChecked(false); // false: links=Bit7 (klassisch)
    auto* rmxBusSel = new QComboBox; rmxBusSel->addItems({"RMX0","RMX1"});
    auto* rmxBitOrderSwitch = new QCheckBox("Bitreihenfolge umkehren");
    rmxBitOrderSwitch->setChecked(false);
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
        sxTable->setItem(row,base+2,new QTableWidgetItem(""));
        rmxTable->setItem(row,base+0,new QTableWidgetItem(QString::number(adr)));
        rmxTable->setItem(row,base+1,new QTableWidgetItem("-"));
        rmxTable->setItem(row,base+2,new QTableWidgetItem("--------"));
    }
    auto openBitSendDialog = [this, sxBusSel, sxTable](int adr){
        if(adr < 0 || adr > 111) return;
        int row = adr % 28;
        int blk = adr / 28;
        int base = blk * 3;
        QTableWidgetItem* valItem = sxTable->item(row, base+1);
        int current = valItem ? valItem->text().toInt() : 0;

        QDialog dlg(this);
        dlg.setWindowTitle(QString("SX Bit-Senden ADR %1").arg(adr));
        auto* v = new QVBoxLayout(&dlg);
        v->addWidget(new QLabel(QString("Adresse %1 (SX%2)").arg(adr).arg(sxBusSel->currentIndex())));
        auto* bitsRow = new QHBoxLayout;
        std::array<QCheckBox*,8> bits{};
        for(int i=7;i>=0;--i){
            auto* cb = new QCheckBox(QString::number(i));
            cb->setChecked((current >> i) & 0x1);
            bits[i] = cb;
            bitsRow->addWidget(cb);
        }
        v->addLayout(bitsRow);
        auto* valueLbl = new QLabel(QString("Wert: %1").arg(current));
        v->addWidget(valueLbl);
        auto recompute = [bits, valueLbl](){
            int v=0; for(int i=0;i<8;++i) if(bits[i] && bits[i]->isChecked()) v |= (1<<i);
            valueLbl->setText(QString("Wert: %1").arg(v));
        };
        for(int i=0;i<8;++i) if(bits[i]) QObject::connect(bits[i], &QCheckBox::toggled, &dlg, recompute);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText("Senden");
        v->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if(dlg.exec() != QDialog::Accepted) return;

        int out=0; for(int i=0;i<8;++i) if(bits[i] && bits[i]->isChecked()) out |= (1<<i);
        int bus = sxBusSel->currentIndex();
        bool ok = ctrl.send(BackendKind::SX, bus, adr, out);
        log->append(QString("BITSEND SX b%1 a%2 v%3 -> %4").arg(bus).arg(adr).arg(out).arg(ok?"OK":"FAIL"));
    };

    auto createBitButtonsCell = [this](BackendKind kind, QComboBox* busSel, QTableWidget* table, QCheckBox* bitOrderSwitch, int adr, int value){
        auto* host = new QWidget;
        auto* hl = new QHBoxLayout(host);
        hl->setContentsMargins(1,1,1,1);
        hl->setSpacing(2);

        for(int bit=7; bit>=0; --bit){
            auto* b = new QToolButton(host);
            b->setAutoRaise(false);
            b->setProperty("adr", adr);
            b->setProperty("bit", bit);
            bool on = (value & (1<<bit)) != 0;
            b->setText(on ? "1" : "0");
            b->setStyleSheet(on
                ? "QToolButton{min-width:16px; min-height:18px; font-weight:700; color:#08120a; background:#22c55e; border:1px solid #0b3d17; border-radius:2px;}"
                : "QToolButton{min-width:16px; min-height:18px; font-weight:700; color:#f8fafc; background:#475569; border:1px solid #1e293b; border-radius:2px;}");
            hl->addWidget(b);
            QObject::connect(b, &QToolButton::clicked, host, [this, kind, busSel, table, bitOrderSwitch, b]{
                int adr = b->property("adr").toInt();
                int shownBit = b->property("bit").toInt();
                int bit = (bitOrderSwitch && bitOrderSwitch->isChecked()) ? (7 - shownBit) : shownBit;
                if((adr >= 0 && adr <= 3) || (adr >= 104 && adr <= 111)){
                    log->append(QString("BITSEND BLOCKIERT a%1 (reserviert)").arg(adr));
                    return;
                }
                int row = adr % 28;
                int blk = adr / 28;
                int base = blk * 3;
                auto* valItem = table->item(row, base+1);
                if(!valItem) return;
                bool okVal=false;
                int cur = valItem->text().toInt(&okVal);
                if(!okVal) return;
                int out = cur ^ (1 << bit);
                int bus = busSel->currentIndex();
                bool ok = ctrl.send(kind, bus, adr, out);
                log->append(QString("BITBTN %1 b%2 a%3 bit%4 %5->%6 -> %7")
                    .arg(kind==BackendKind::SX?"SX":"RMX").arg(bus).arg(adr).arg(bit).arg(cur).arg(out).arg(ok?"OK":"FAIL"));
            });
        }
        return host;
    };

    for(int adr=0; adr<112; ++adr){
        int row = adr % 28;
        int blk = adr / 28;
        int base = blk * 3;
        sxTable->setCellWidget(row, base+2, createBitButtonsCell(BackendKind::SX, sxBusSel, sxTable, sxBitOrderSwitch, adr, 0));
        rmxTable->setCellWidget(row, base+2, createBitButtonsCell(BackendKind::RMX, rmxBusSel, rmxTable, rmxBitOrderSwitch, adr, 0));
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
            t->setColumnWidth(base+2, 150);
        }
        t->setStyleSheet(
            "QTableWidget { background: #e8ecf3; color: #0b1020; gridline-color: #7d8798; alternate-background-color: #d7dde8; border: 2px solid #11141c; }"
            "QHeaderView::section { background: #8f9aae; color: #111827; padding: 4px; font-weight: 700; border: 1px solid #303846; }"
        );
        t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const int tableH = t->horizontalHeader()->height() + t->rowCount() * t->verticalHeader()->defaultSectionSize() + 8;
        t->setMinimumHeight(tableH);
        t->setMaximumHeight(tableH);
    }
    auto* sxTopRow = new QHBoxLayout;
    sxTopRow->setContentsMargins(0,0,0,0);
    sxTopRow->addWidget(sxBusSel);
    sxTopRow->addSpacing(8);
    sxTopRow->addWidget(sxBitOrderSwitch);
    sxTopRow->addStretch(1);
    sxTabL->addLayout(sxTopRow);
    sxTabL->addWidget(sxTable);
    auto* rmxTopRow = new QHBoxLayout;
    rmxTopRow->setContentsMargins(0,0,0,0);
    rmxTopRow->addWidget(rmxBusSel);
    rmxTopRow->addSpacing(8);
    rmxTopRow->addWidget(rmxBitOrderSwitch);
    rmxTopRow->addStretch(1);
    rmxTabL->addLayout(rmxTopRow);
    rmxTabL->addWidget(rmxTable);
    tabs->addTab(sxTab, "SX Monitor");
    tabs->addTab(rmxTab, "RMX Monitor");

    connect(sxTable, &QTableWidget::cellDoubleClicked, this, [this, sxTable, openBitSendDialog](int row, int column){
        if(column % 3 != 2) return; // nur Bits-Spalte
        QTableWidgetItem* adrItem = sxTable->item(row, column-2);
        if(!adrItem) return;
        bool okAdr=false;
        int adr = adrItem->text().toInt(&okAdr);
        if(!okAdr) return;
        if((adr >= 0 && adr <= 3) || (adr >= 104 && adr <= 111)) return; // reserviert
        openBitSendDialog(adr);
    });

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
        QTableWidget* t = (bk==BackendKind::SX)?sxTable:rmxTable;
        int bus = (bk==BackendKind::SX)?sxBusSel->currentIndex():rmxBusSel->currentIndex();
        auto& vals = (bk==BackendKind::SX)?*sxVals:*rmxVals;
        for(int adr=0; adr<112; ++adr){
            int row=adr%28, base=(adr/28)*3;
            int v=vals[bus][adr];
            t->item(row,base+1)->setText(v<0?"-":QString::number(v));
            t->item(row,base+2)->setText(v<0?"--------":bits8(v));
        }
    };
    auto rebuildSxBitWidgets = [sxTable, createBitButtonsCell, sxVals, sxBusSel, sxBitOrderSwitch](){
        int selBus = sxBusSel->currentIndex();
        for(int adr=0; adr<112; ++adr){
            int row = adr % 28;
            int blk = adr / 28;
            int base = blk * 3;
            int v = (*sxVals)[selBus][adr];
            if(v < 0) v = 0;
            sxTable->setCellWidget(row, base+2, createBitButtonsCell(BackendKind::SX, sxBusSel, sxTable, sxBitOrderSwitch, adr, v));
        }
    };
    auto rebuildRmxBitWidgets = [rmxTable, createBitButtonsCell, rmxVals, rmxBusSel, rmxBitOrderSwitch](){
        int selBus = rmxBusSel->currentIndex();
        for(int adr=0; adr<112; ++adr){
            int row = adr % 28;
            int blk = adr / 28;
            int base = blk * 3;
            int v = (*rmxVals)[selBus][adr];
            if(v < 0) v = 0;
            rmxTable->setCellWidget(row, base+2, createBitButtonsCell(BackendKind::RMX, rmxBusSel, rmxTable, rmxBitOrderSwitch, adr, v));
        }
    };
    connect(sxBitOrderSwitch, &QCheckBox::toggled, this, [rebuildSxBitWidgets](bool){ rebuildSxBitWidgets(); });
    connect(sxBusSel, qOverload<int>(&QComboBox::currentIndexChanged), this, [repaintTable,rebuildSxBitWidgets](int){ repaintTable(BackendKind::SX); rebuildSxBitWidgets(); });

    connect(rmxBusSel, qOverload<int>(&QComboBox::currentIndexChanged), this, [repaintTable,rebuildRmxBitWidgets](int){ repaintTable(BackendKind::RMX); rebuildRmxBitWidgets(); });
    connect(rmxBitOrderSwitch, &QCheckBox::toggled, this, [rebuildRmxBitWidgets](bool){ rebuildRmxBitWidgets(); });

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

    l->addWidget(trackPanel);
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
    connect(trackStartBtn,&QPushButton::clicked,this,[this,trackState]{
        const int target = (*trackState == 1) ? 0 : 1;
        bool ok = ctrl.send(BackendKind::SX, 0, 0, target);
        log->append(QString("GLEIS %1: WRITE SX0 ADR0=%2 -> %3")
            .arg(target ? "START/AN" : "STOP/AUS")
            .arg(target)
            .arg(ok ? "OK" : "FAIL"));
        ctrl.readAdr(BackendKind::SX, 0, 127);
    });

    connect(sxPanel,&ConnectionPanel::connectRequested,this,[this](const QString&,int){ connectBackendFromPanel(BackendKind::SX); });
    connect(rmxPanel,&ConnectionPanel::connectRequested,this,[this](const QString&,int){ connectBackendFromPanel(BackendKind::RMX); });
    connect(sxPanel,&ConnectionPanel::disconnectRequested,this,[this]{ disconnectBackend(BackendKind::SX); });
    connect(rmxPanel,&ConnectionPanel::disconnectRequested,this,[this]{ disconnectBackend(BackendKind::RMX); });

    connect(&ctrl,&DualRuntimeController::connectedChanged,this,[this](BackendKind b,bool on){
        ConnectionPanel* panel = (b==BackendKind::SX) ? sxPanel : rmxPanel;
        panel->setConnected(on);
        if(on && panel->endpoint().startsWith("daemon://")) panel->setHardwareWarning(true);
    });
    connect(&ctrl,&DualRuntimeController::trackUpdated,this,[this,updateTrackPanel](BackendKind b,int t){
        ConnectionPanel* panel = (b==BackendKind::SX) ? sxPanel : rmxPanel;
        panel->setTrackState(t);
        if(b==BackendKind::SX) updateTrackPanel(t);
        if(t >= 0) panel->setHardwareWarning(false);
        else if(panel->endpoint().startsWith("daemon://")) panel->setHardwareWarning(true);
    });
    connect(&ctrl,&DualRuntimeController::status,this,[this](BackendKind b,const QString& s){
        log->append(QString("%1: %2").arg(b==BackendKind::SX?"SX":"RMX", s));
        ConnectionPanel* panel = (b==BackendKind::SX) ? sxPanel : rmxPanel;
        if(panel->endpoint().startsWith("daemon://")){
            if(s.contains("SX_HW ONLINE", Qt::CaseInsensitive)) panel->setHardwareWarning(false);
            else if(s.contains("SX_HW SEARCHING", Qt::CaseInsensitive) || s.contains("SX_HW OFFLINE", Qt::CaseInsensitive)) panel->setHardwareWarning(true);
            else if(s.contains("ERR", Qt::CaseInsensitive) || s.contains("track unknown", Qt::CaseInsensitive)) panel->setHardwareWarning(true);
            else if(s.contains("daemon ack: OK", Qt::CaseInsensitive)) panel->setHardwareWarning(false);
        }
    });
    connect(&ctrl,&DualRuntimeController::frameReceived,this,[this,rx126Only,sxTable,rmxTable,sxVals,rmxVals,sxBusSel,rmxBusSel,visualAddrA,visualAddrB,visualBitOrder,visualArm,visualAngle,visualLimitSpin](BackendKind b,int bus,int adr,int val){
        if(b==BackendKind::SX) sxPanel->setHardwareWarning(false); else rmxPanel->setHardwareWarning(false);
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
            if(b==BackendKind::SX){
                auto* w = sxTable->cellWidget(row, base+2);
                if(w){
                    auto btns = w->findChildren<QToolButton*>();
                    for(auto* bt : btns){
                        int bit = bt->property("bit").toInt();
                        bool on = (val & (1<<bit)) != 0;
                        bt->setText(on ? "1" : "0");
                        bt->setStyleSheet(on
                            ? "QToolButton{min-width:16px; min-height:18px; font-weight:700; color:#08120a; background:#22c55e; border:1px solid #0b3d17; border-radius:2px;}"
                            : "QToolButton{min-width:16px; min-height:18px; font-weight:700; color:#f8fafc; background:#475569; border:1px solid #1e293b; border-radius:2px;}");
                    }
                }
            } else {
                t->item(row,base+2)->setText(bits8(val));
            }

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
