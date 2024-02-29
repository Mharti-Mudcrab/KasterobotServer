#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include "ClientHandler.h"
#include "DashboardClient.h"
#include "GripperClient.h"
#include <iostream>
#include <cmath>
#include <QTcpSocket>
#include <QTcpServer>
#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QTime>

class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

signals:
    void sendToRobotSignal(QByteArray data);
    void sendToCameraSignal(QByteArray data);

    void newConnectionSignal(qintptr handle);
    void logInDatabaseSignal(QueryType type, QVariantList data);

public slots:
    void gripperStateChangedSlot(GripperState state);
    void robotStateChangedSlot(RobotState state);
    void cameraConnectedSlot();

    void getThrowParametersFromCalculation(double x, double y);
    void getNewBallPosSlot(QByteArray ballPos);
    void throwTimeOutSlot();

    void goButtonOnGUISlot();

protected:
    void incomingConnection(qintptr handle);

    bool mIsProgramEnabled;
    ClientHandler mClientHandler;
    GripperClient mGripper;
    DashboardClient mDashboard;
    QTimer mTimer;
    qint64 mTimestamps[2];

    QVariantList mThrowParameters;
    QVariantList mTargetPos;
};

#endif // MYTCPSERVER_H

