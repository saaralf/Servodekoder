#include "connection_panel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

ConnectionPanel::ConnectionPanel(const QString& title, const QString& defaultEndpoint, int defaultBaud, QWidget* parent)
    : QWidget(parent){
    auto* l = new QHBoxLayout(this);
    titleLbl = new QLabel(title);
    endpointEdit = new QLineEdit(defaultEndpoint);
    baudEdit = new QLineEdit(QString::number(defaultBaud));
    baudEdit->setFixedWidth(80);
    connectBtn = new QPushButton("Connect");
    disconnectBtn = new QPushButton("Disconnect");
    disconnectBtn->setEnabled(false);
    statusLbl = new QLabel("offline");
    trackLbl = new QLabel("Track: ?");
    trackLbl->setMinimumWidth(80);
    l->addWidget(titleLbl); l->addWidget(new QLabel("Endpoint:")); l->addWidget(endpointEdit,1);
    l->addWidget(new QLabel("Baud:")); l->addWidget(baudEdit);
    l->addWidget(connectBtn); l->addWidget(disconnectBtn); l->addWidget(statusLbl); l->addWidget(trackLbl);
    connect(connectBtn,&QPushButton::clicked,this,[this]{ emit connectRequested(endpoint(), baud()); });
    connect(disconnectBtn,&QPushButton::clicked,this,[this]{ emit disconnectRequested(); });
}
QString ConnectionPanel::endpoint() const { return endpointEdit->text().trimmed(); }
int ConnectionPanel::baud() const { return baudEdit->text().toInt(); }
void ConnectionPanel::setConnected(bool on){
    statusLbl->setText(on?"online":"offline");
    connectBtn->setEnabled(!on);
    disconnectBtn->setEnabled(on);
}
void ConnectionPanel::setTrackState(int t){
    if(t<0) trackLbl->setText("Track: ?");
    else if(t==0) trackLbl->setText("Track: AUS");
    else trackLbl->setText("Track: AN");
}
