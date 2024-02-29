#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "ConnectionTester.h"
#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QTimer>

enum DashboardFunktion
{
    DASH_IDLE,
    DASH_START_PROGRAM,
    DASH_STOP_PROGRAM
};

class DashboardClient : public QTcpSocket
{
    Q_OBJECT
public:
    DashboardClient(QObject *parent = nullptr, QString IP = "192.168.1.49", quint16 PORT = 29999);

public slots:
    void connectToDashboardSlot();
    void startProgramSlot();
    void stopProgram();

    void readyRead();
    void sendToDashboard(QByteArray data);

private slots:
    void timeoutSlot();

private:
    QTimer mTimer;
    QString mIP;
    quint16 mPORT;
    QByteArray mStateQuery;
    DashboardFunktion mDashState;

};

#endif // DASHBOARD_H
