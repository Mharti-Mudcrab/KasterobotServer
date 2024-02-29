#include "GripperClient.h"
#include "connectiontester.h"
#include <QTime>

// Instantiation of static member function.
GripperState GripperClient::mGripperState = GRIPPER_DISCONNECTED;

GripperClient::GripperClient(QObject *parent, QString IP, quint16 PORT  )
    : QTcpSocket (parent), mIP{IP}, mPORT{PORT}, mACKTimer{this}, mTimeoutCount{0}
{
    qDebug() << "\nInitializing Gripper";

    mGripperState = GRIPPER_HOMING;
    connect(this, SIGNAL(disconnected()), this, SLOT(connectToGripperSlot()));
    connect(this, SIGNAL(readyRead()), this, SLOT(readyReadSlot()));
    connect(this, SIGNAL(gripperStateChangedSignal(GripperState)), parent, SLOT(gripperStateChangedSlot(GripperState)));

    mACKTimer.setTimerType(Qt::CoarseTimer);
    mACKTimer.setInterval(2000);
    connect(&mACKTimer, SIGNAL(timeout()), this, SLOT(acknowledgeTimoutSlot()));

    connectToGripperSlot();
}

QString GripperClient::getStateString()
{
    static std::map<GripperState, std::string> strings;
    if (strings.size() == 0){
#define INSERT_ELEMENT(p) strings[p] = #p
        INSERT_ELEMENT(GRIPPER_HOMING);
        INSERT_ELEMENT(GRIPPER_HOME);
        INSERT_ELEMENT(GRIPPER_GRIPPING);
        INSERT_ELEMENT(GRIPPER_HOLDING);
        INSERT_ELEMENT(GRIPPER_OPENING);
        INSERT_ELEMENT(GRIPPER_OPEN);
        INSERT_ELEMENT(GRIPPER_RELEASING);
        INSERT_ELEMENT(GRIPPER_RELEASED);
        INSERT_ELEMENT(GRIPPER_IDLE);
        INSERT_ELEMENT(GRIPPER_DISCONNECTED);
        INSERT_ELEMENT(GRIPPER_STOPPED);
        INSERT_ELEMENT(GRIPPER_FASTSTOPPED);
 #undef INSERT_ELEMENT
    }
    return QString(strings[GripperClient::mGripperState].c_str());
}
GripperState GripperClient::getState() const
{
    return mGripperState;
}

void GripperClient::connectToGripperSlot()
{
    if (mGripperState == GRIPPER_HOMING)      // Hvis det er første gang vi connector
    {
        qDebug() << "Connecting to Gripper using IP:" << mIP << "and PORT:" << mPORT;
    }
    else                                        // Hvis gripperen pludselig disconnecter.
    {
        if (++mTimeoutCount < 1)
            qDebug() << "Gripper disconnected. It's the" << mTimeoutCount << "\b. time in a row. Attempting reconnect using IP: " << mIP << ", and PORT: " << mPORT;
        else
            qDebug() << "Gripper disconnected. Attempting reconnect using IP: " << mIP << ", and PORT: " << mPORT;

        mGripperState = GRIPPER_DISCONNECTED;
        emit gripperStateChangedSignal(mGripperState);
    }

    connectToHost(mIP, mPORT);
    waitForConnected();

    if(state() == QTcpSocket::ConnectedState)
    {
        qDebug() << "Connection made to Gripper";
        qDebug() << "Executing HOME() ";
        write("VERBOSE=1\nHOME()\n");
        mGripperState = GRIPPER_HOMING;
    }
    else
    {
        qDebug() << "Could not make connection to Gripper. Starting thread to establish connection";
        ConnectionTester* con = new ConnectionTester(this, mIP, mPORT, SLOT(connectToGripperSlot()));
    }
}

void GripperClient::acknowledgeTimoutSlot()
{
    qDebug() << "Acknowledge from gripper timer timed out, timeout cont:" << ++mTimeoutCount << "Command:" << mLastCmd;
    qDebug() << "Trying to call the command again";
    write(mLastCmd);
}

void GripperClient::readyReadSlot()
{
    QByteArray data = readAll();
    if (data.startsWith("FIN")) {
        if (data.contains("HOME")) // Can only happen when initialising connection to gripper, or when comming back from FASTSTOP
        {
            mGripperState = GRIPPER_HOME;
        }
        else if (data.contains("GRIP"))
        {
            mGripperState = GRIPPER_HOLDING;
        }
        else if (data.contains("MOVE"))
        {
            mGripperState = GRIPPER_OPEN;
        }
        else if (data.contains("RELEASE"))
        {
            mGripperState = GRIPPER_RELEASED;
        }
        else
        {
            qDebug() << "Some unknown command received in gripper::readyReadSlot() =" << data;
            return;
        }
        mACKTimer.stop();
        emit gripperStateChangedSignal(mGripperState);
    }
    else if (data.startsWith("ACK"))
    {
        mTimeoutCount = 0; // ACK received, Clearing timeout count history
        if (data.contains("HOME")) // Can only happen when initialising connection to gripper
        {
            mGripperState = GRIPPER_HOMING;
        }
        else if (data.contains("GRIP"))
        {
            mGripperState = GRIPPER_GRIPPING;
        }
        else if (data.contains("MOVE"))
        {
            mGripperState = GRIPPER_OPENING;
        }
        else if (data.contains("RELEASE"))
        {
            mGripperState = GRIPPER_RELEASING;
        }
        // Cases bellow are motly for emergensies.
        else if (data.contains("STOP"))
        {
            mGripperState = GRIPPER_STOPPED;
        }
        else if (data.contains("FASTSTOP"))
        {
            mGripperState = GRIPPER_FASTSTOPPED;
        }
        else if (data.contains("FSACK"))
        {
            mGripperState = GRIPPER_IDLE;
            qDebug() << "Gripper sending: HOME()";
            write("HOME()\n");
            mACKTimer.start();
            mLastCmd = "HOME()\n";
        }
        else // Some unknown command received in gripper::readyReadSlot()
        {
            qDebug() << "Some unknown command received in gripper::readyReadSlot() =" << data;
            return;
        }
        mACKTimer.stop();
        emit gripperStateChangedSignal(mGripperState);
    }
    else if (data.startsWith("VERBOSE=1"))
    {
        qDebug() << "";
    }
    else if (data.startsWith("ERR GRIP"))
    {
        mGripperState = GRIPPER_FAILED_GRIP;
        emit gripperStateChangedSignal(mGripperState);
    }
    else
    {
        qDebug() << "ERROR. unknown data received in gripper::readyReadSlot() =" << data << "Might be an errormessage.";
    }
}

void GripperClient::homeSlot()
{
    qDebug() << "Sending to Gripper: HOME()";
    write("HOME()\n"); // 10[N], 40[mm] PART WIDTH, 250 [mm/s] SPEED (MAX SPEED).
    mACKTimer.start();
    mLastCmd = "HOME()\n";
}

void GripperClient::gripSlot()
{
    qDebug() << "Sending to Gripper: GRIP(40, 40, 250)";
    write("GRIP(10, 30, 250)\n"); // 10[N], 40[mm] PART WIDTH, 250 [mm/s] SPEED (MAX SPEED).
    mACKTimer.start();
    mLastCmd = "GRIP(10, 30, 250)\n";
}
void GripperClient::moveSlot()
{
    if (mGripperState == GRIPPER_OPEN || mGripperState == GRIPPER_HOME)
    {
        qDebug() << "Gripper is already OPEN/HOME. Emiting OPEN state";
        mGripperState = GRIPPER_OPEN;
        emit gripperStateChangedSignal(mGripperState);
    }
    else
    {
        qDebug() << "Sending to Gripper: MOVE(80)";
        write("MOVE(80)\n"); // 80[mm] FINGER POS.
        mACKTimer.start();
        mLastCmd = "MOVE(80)\n";
    }
}
void GripperClient::releaseSlot()
{
    qDebug() << "Sending to Gripper: RELEASE(20, 420)";
    write("RELEASE(20, 420)\n"); // 10[mm] PULL BACK DISTANCE, 420[mm/s] SPEED
    mACKTimer.start();
    mLastCmd = "RELEASE(20, 430)\n";
}

// Mostly for Exception handling
void GripperClient::stopGripperSlot() // NOT TESTED
{
    qDebug() << "Sending to Gripper: STOP()";
    write("STOP()\n");
    mACKTimer.start();
    mLastCmd = "STOP()\n";
}
void GripperClient::fastStopGripperSlot() // NOT TESTED
{
    qDebug() << "Sending to Gripper: FASTSTOP()";
    write("FASTSTOP()\n");
    mACKTimer.start();
    mLastCmd = "FASTSTOP()\n";
}
void GripperClient::reenableGripperSlot() // NOT TESTED
{
    qDebug() << "Sending to Gripper: FSACK()";
    write("FSACK()\n");
    mACKTimer.start();
    mLastCmd = "FSACK()\n";
}
