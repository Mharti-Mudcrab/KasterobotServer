#ifndef GRIPPER_H
#define GRIPPER_H

#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QTimer>
#include <QTextStream>

enum GripperState
{// Gripper waiting         Gripper doing something

    GRIPPER_HOMING,         GRIPPER_HOME,
    GRIPPER_GRIPPING,       GRIPPER_HOLDING,
    GRIPPER_OPENING,        GRIPPER_OPEN,
    GRIPPER_RELEASING,      GRIPPER_RELEASED,

    // Gripper Errors
    GRIPPER_IDLE,       GRIPPER_DISCONNECTED,
    GRIPPER_STOPPED,    GRIPPER_FASTSTOPPED,
    GRIPPER_FAILED_GRIP
};

class GripperClient : QTcpSocket
{
    Q_OBJECT
public:
    GripperClient(QObject *parent = nullptr, QString IP = "192.168.1.23", quint16 PORT = 1000);

    static QString getStateString();
    GripperState getState() const;

signals:
    void gripperStateChangedSignal(GripperState state);

public slots:
    void connectToGripperSlot();
    void acknowledgeTimoutSlot();

    void readyReadSlot();

    void homeSlot();
    void gripSlot();
    void moveSlot();
    void releaseSlot();

    void stopGripperSlot();
    void fastStopGripperSlot();
    void reenableGripperSlot();

private:
    static GripperState mGripperState;
    QString mIP;
    quint16 mPORT;
    QTimer mACKTimer;
    int mTimeoutCount;
    QByteArray mLastCmd;
};
#endif // GRIPPER_H
