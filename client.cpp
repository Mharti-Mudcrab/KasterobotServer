#include "client.h"

using namespace std;

Client::Client(QObject *parent, qintptr handle)
    : QTcpSocket (parent), mId{handle}, mClientType{CLIENT_GUI}
{
    setSocketDescriptor(handle);
    connect(this, SIGNAL(disconnected()), this, SLOT(deleteLater()));
    connect(this, SIGNAL(disconnected()), parent, SLOT(disconnectedSlot()));

    if (waitForReadyRead(100))
    {
        QByteArray data = readAll();
        if (data.startsWith("Robot"))
        {
            qDebug() << "\nNew connection turned out to be the Robot";
            mClientType = CLIENT_ROBOT;
            connect(parent, SIGNAL(sendDataToRobotSignal(QByteArray)), this, SLOT(writeDataOutSlot(QByteArray)));
        }
        else if (data.startsWith("Camera"))
        {
            qDebug() << "\nNew connection turned out to be the Camera";
            mClientType = CLIENT_CAMERA;
            connect(parent, SIGNAL(sendDataToCameraSignal(QByteArray)), this, SLOT(writeDataOutSlot(QByteArray)));
        }
        else if (data.startsWith("GUI"))
        {
            qDebug() << "\nNew connection turned out to be a GUI";
            mClientType = CLIENT_GUI;
            connect(parent, SIGNAL(sendDataToGUISignal(QByteArray)), this, SLOT(writeDataOutSlot(QByteArray)));
            write("},ok\n");
        }
    }
    else
    {
        connect(parent, SIGNAL(sendDataToGUISignal(QByteArray)), this, SLOT(writeDataOutSlot(QByteArray)));
        qDebug() << "\nSomething went wrong, Client has not been identified\n"
                    "Instantiating it with type CLIENT_GUI";
    }
    connect(this, SIGNAL(readyRead()), parent, SLOT(newDataFromClientSlot()));
    if (bytesAvailable()) emit readyRead();
}

qintptr Client::getId() const
{
    return mId;
}
ClientType Client::getClientType() const
{
    return mClientType;
}

void Client::writeDataOutSlot(QByteArray data)
{
    write(data);
}
