#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include "DatabaseConnector.h"
#include "GripperClient.h"
#include <QObject>
#include <QTcpSocket>
#include <QtDebug>
#include <QThread>
#include <iostream>
#include <queue>
#include <sstream>

enum RobotState
{// Robot waiting for data          Robot doing something
    ROBOT_DISCONNECTED,
                                    ROBOT_MOVING_TO_HOME,
    ROBOT_WAITING_TO_GET_LOCATION,  ROBOT_MOVING_TO_LOCATION,
    ROBOT_WAITING_FOR_OPEN,         ROBOT_MOVING_DOWN_TO_PART,
    ROBOT_WAITING_FOR_GRIP,         ROBOT_MOVING_UP_WITH_PART,
    ROBOT_WAITING_TO_THROW,         ROBOT_MOVING_TO_THROW,
                                    ROBOT_THROWING,
};

class ClientHandler : public QObject
{
        Q_OBJECT
public:
    ClientHandler(QObject *parent = nullptr);

    std::string getRobotStateString() const;
    RobotState getRobotState() const;
    void setRobotState(RobotState state);

signals:
    // Going to GUI's, Robot or Camera
    void sendDataToRobotSignal(QByteArray data);
    void sendDataToGUISignal(QByteArray data);
    void sendDataToCameraSignal(QByteArray data);

    // Going to Server
    void guiSendDataSignal(QByteArray data);
    void robotStateChangedSignal(RobotState state);
    void cameraConnectedSignal();
    void goButtonOnGUISignal();

    // Camera related
    void newBallPosSignal(QByteArray data);
    void newTargetPosSignal(double x, double y);

    // General class functionality
    void queryForDatabaseSignal(QByteArray data);
    void killSwitchSignal(QByteArray data);

public slots:
    // Generelt for: GUI, Robot, Camera
    void newConnectionSlot(qintptr handle);
    void disconnectedSlot();
    void newDataFromClientSlot();

    // Individual
    void sendToRobotSlot(QByteArray data);
    void sendToGUISlot(QByteArray data);
    void sendToCameraSlot(QByteArray data);
    void cameraTimerTimeoutSlot();

    void logInDatabaseSlot(QueryType type, QVariantList data);

private:
    RobotState mRobotState;
    bool isCameraConnected;
    short mConnected;

    DatabaseConnector mDbCon;
    QTimer mCameraTimer;
    QByteArray mPendingCameraCommand;

    std::queue<std::array<double, 2>> mTargetPos;
};

#endif // CLIENTHANDLER_H
