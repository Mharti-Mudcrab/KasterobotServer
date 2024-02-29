#ifndef DATABASECONNECTOR_H
#define DATABASECONNECTOR_H

#include "Client.h"
#include <QObject>
#include <QtDebug>
#include <queue>
// QSql
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QSqlError>
#include <QVariant>
#include <QVariantList>
// QJson
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

enum QueryType
{
    LOG_DYNAMIK,    GET_DYNAMIK,
    LOG_ROBOT,      GET_ROBOT,
    LOG_CUPPOS,     GET_CUPPOS,
    LOG_BALLPOS,    GET_BALLPOS,
    LOG_TCPPOS,     GET_TCPPOS,

    GET_ROBOT_UPDATE_ALL,
};

class DatabaseConnector : public QObject
{
    Q_OBJECT
public:
    DatabaseConnector(QObject *parent, QString dbName = "semesterprojekt3");
    std::string getQueryTypeString(QueryType type) const;

signals:
    void updateForGUIsSignal(QByteArray);

public slots:
    void updateNewGUIConnection(Client* gui);
    void logData(QueryType type, QVariantList data);
    int getData(QueryType type, Client* gui = nullptr);

private:
    QSqlDatabase mDatabase;
    QSqlQuery mQuery;
};

#endif // DATABASECONNECTOR_H
