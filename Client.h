#ifndef CLIENT_H
#define CLIENT_H

#include <QTcpSocket>
#include <QtDebug>
#include <iostream>

enum ClientType
{
    CLIENT_GUI,
    CLIENT_ROBOT,
    CLIENT_CAMERA
};

class Client : public QTcpSocket
{
    Q_OBJECT
public:
    Client(QObject *parent, qintptr handle);
    qintptr getId() const;
    ClientType getClientType() const;

signals:
    void newDataSignal(QByteArray data);

public slots:
    void writeDataOutSlot(QByteArray data);

private:
    qintptr mId;
    ClientType mClientType;
};

#endif // CLIENT_H
