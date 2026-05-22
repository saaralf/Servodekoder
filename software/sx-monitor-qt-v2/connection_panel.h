#pragma once
#include <QWidget>
class QLabel; class QLineEdit; class QPushButton;

class ConnectionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionPanel(const QString& title, const QString& defaultEndpoint, int defaultBaud, QWidget* parent=nullptr);
    QString endpoint() const;
    int baud() const;
    void setConnected(bool on);
    void setTrackState(int track); // -1 unknown, 0 off, 1 on
signals:
    void connectRequested(const QString& endpoint, int baud);
    void disconnectRequested();
private:
    QLabel* titleLbl{};
    QLineEdit* endpointEdit{};
    QLineEdit* baudEdit{};
    QPushButton* connectBtn{};
    QPushButton* disconnectBtn{};
    QLabel* statusLbl{};
    QLabel* trackLbl{};
};
