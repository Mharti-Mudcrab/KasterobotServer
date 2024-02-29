#include "DashboardClient.h"
#include <QTime>

DashboardClient::DashboardClient(QObject *parent, QString IP, quint16 PORT)
    : QTcpSocket(parent), mTimer{this}, mIP{IP}, mPORT{PORT}, mStateQuery{"robotmode\n"}, mDashState{DASH_IDLE}
{
    qDebug() << "\nInitializing DashboardClient";

    mTimer.setInterval(500);
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(timeoutSlot()));
    connect(this, SIGNAL(readyRead()), this, SLOT(readyRead()));

    qDebug() << "Connecting to Dashboard Server using IP:" << mIP << "and PORT:" << mPORT;
    connectToDashboardSlot();
}

void DashboardClient::connectToDashboardSlot()
{
    connectToHost(mIP, mPORT);
    waitForConnected();

    if(state() == QTcpSocket::ConnectedState)
    {
        qDebug() << "Connected to Dashboard Server port: 29999";
        mTimer.setInterval(500);
        mTimer.start();
        switch (mDashState) {
        case DASH_IDLE:
            break;
        case DASH_STOP_PROGRAM:
            sendToDashboard(mStateQuery);
            break;
        case DASH_START_PROGRAM:
            sendToDashboard("stop\n");
        }
    }
    else
    {
        qDebug() << "Could not make connection to Dashboard Server port: 29999. Starting thread to establish connection";
        ConnectionTester* con = new ConnectionTester(this, mIP, mPORT, SLOT(connectToDashboardSlot()));
    }
}

void DashboardClient::timeoutSlot()
{
    sendToDashboard(mStateQuery);
}

void DashboardClient::readyRead()
{
    QByteArray data = readAll();
    //qDebug() << "Data received: " << data;
    if (data.startsWith("Robotmode: POWER_OFF") || data.startsWith("Robotmode: IDLE"))
    {
        sendToDashboard("brake release\n");
    }
    else if (data.startsWith("Robotmode: RUNNING"))
    {
        mStateQuery = "programstate\n";
    }
    else if (data.startsWith("STOPPED Final2"))
    {
        if (mDashState == DASH_START_PROGRAM)
        {
            sendToDashboard("play\n");
        }
        else
        {
            mTimer.stop();
            qDebug() << "\nDashboard finished making sure program is stopped";
        }
    }
    else if (data.startsWith("STOPPED"))
    {
        if (mDashState == DASH_START_PROGRAM)
        {
            sendToDashboard("load /programs/G3/Final2_program.urp\n");
        }
        else
        {
            mTimer.stop();
            qDebug() << "\nDashboard finished making sure program is stopped";
        }
    }
    else if (data.startsWith("Loading program: /programs/G3/Final2_program.urp"))
    {
        sendToDashboard("play\n");
    }
    else if (data.startsWith("PLAYING"))
    {
        if (mDashState == DASH_START_PROGRAM)
        {
            mTimer.stop();
            qDebug() << "\nDashboard finished starting program";
        }
        else
        {
            sendToDashboard("close popup\n");
            sendToDashboard("stop\n");
        }
    }

    else if (data.startsWith("Failed to execute: stop"))
    {

    }
    else if (data.startsWith("Failed to execute: play"))
    {
        qDebug() << "Could not finish starting of program, it needs to be set in start possition";
        mTimer.setInterval(2000);
        return;
    }
    mTimer.setInterval(500);
}
void DashboardClient::sendToDashboard(QByteArray data)
{
    write(data);
}

void DashboardClient::startProgramSlot()
{
    mStateQuery = "robotmode\n";
    mDashState = DASH_START_PROGRAM;
    mTimer.setInterval(500);
    mTimer.start();
    sendToDashboard(mStateQuery);
}
void DashboardClient::stopProgram()
{
    mStateQuery = "programstate\n";
    mDashState = DASH_STOP_PROGRAM;
    mTimer.setInterval(500);
    mTimer.start();
    sendToDashboard(mStateQuery);
}
