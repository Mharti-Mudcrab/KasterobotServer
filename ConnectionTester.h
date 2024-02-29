#ifndef CONNECTIONTESTER_H
#define CONNECTIONTESTER_H

#include <QTimer>
#include <QThread>
#include <QTcpSocket>

class ConnectionTester : public QTcpSocket
{
    Q_OBJECT
public:
    ConnectionTester(QObject* parent, QString IP, quint16 PORT,
                    const char* slot, int timeIntInMillis = 5000)
        : mIP{IP}, mPORT{PORT}, mInterval{timeIntInMillis}
    {
        QThread* thread = new QThread();
        this->moveToThread(thread);
        connect(thread, SIGNAL(started()), this, SLOT(process()));
        connect(this, SIGNAL(finished()), thread, SLOT(quit()));
        connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
        connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
        connect(thread, SIGNAL(finished()), parent, slot);
        thread->start();
    }

signals:
    void finished();

protected slots:
    void process()
    {
        mTimer = new QTimer();
        mTimer->start(mInterval);
        connect(mTimer, SIGNAL(timeout()), this, SLOT(timeoutSlot()));
    }
    void timeoutSlot()
    {
        mTimer->stop();
        connectToHost(mIP, mPORT);
        waitForConnected();

        if (state() == QTcpSocket::ConnectedState)
        {
            emit finished();
            // Kalder deleteLater() på timer, da alt andet slettes af sig selv, men ikke timer,
            // da den ikke har en parent, da den er oprettet i en anden Thread.
            mTimer->deleteLater();
            return;
        }
        mTimer->start();
    }

protected:

    QString mIP;
    quint16 mPORT;
    int mInterval;
    QTimer* mTimer;
};

#endif // CONNECTIONTESTER_H
