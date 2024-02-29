#include "DatabaseConnector.h"

using namespace std;

DatabaseConnector::DatabaseConnector(QObject *parent, QString dbName)
    : QObject (parent)
{
    qDebug() << "\nInitializing DatabaseConnector";
    // Initialise connections
    connect(this, SIGNAL(updateForGUIsSignal(QByteArray)), parent, SLOT(sendToGUISlot(QByteArray)));

    mDatabase = QSqlDatabase::addDatabase("QMYSQL");
    QString hostName("localhost");
    mDatabase.setHostName(hostName);
    mDatabase.setDatabaseName(dbName);
    mDatabase.setUserName("root");
    mDatabase.setPassword("zgnxmc2vm");
    qDebug() << "Connecting to database. HostName:" << hostName << "DatabaseName:" << dbName;
    if (mDatabase.open())
        qDebug() << "Connection to database established";
    else
        qDebug() << "Database could not be opened!";

    QSqlQuery newQuery;
    mQuery = newQuery;
}

string DatabaseConnector::getQueryTypeString(QueryType type) const
{
    static std::map<QueryType, std::string> strings;
    if (strings.size() == 0){
    #define INSERT_ELEMENT(i) strings[i] = #i
            INSERT_ELEMENT(LOG_DYNAMIK);
            INSERT_ELEMENT(GET_DYNAMIK);
            INSERT_ELEMENT(LOG_ROBOT);
            INSERT_ELEMENT(GET_CUPPOS);
            INSERT_ELEMENT(LOG_BALLPOS);
            INSERT_ELEMENT(GET_BALLPOS);
            INSERT_ELEMENT(GET_ROBOT_UPDATE_ALL);
     #undef INSERT_ELEMENT
    }
    return strings[type];
}

void DatabaseConnector::updateNewGUIConnection(Client *gui) // NEEDS CORRECT QUERIES AND METHODNAMES
{
    qDebug() << "\n\nUpdating new GUI";
    if (mDatabase.isOpen())
    {
        queue<QueryType> getCalls;
        getCalls.push(GET_CUPPOS);
        getCalls.push(GET_TCPPOS);
        getCalls.push(GET_BALLPOS);
        getCalls.push(GET_DYNAMIK);
        getCalls.push(GET_ROBOT_UPDATE_ALL);
        QByteArray returnData;

        while (!getCalls.empty())
        {
            getData(getCalls.front(), gui);
            getCalls.pop();
        }
    }
    else
    {
        qDebug() << "\nDatabase not open. failed updateNewGUIConnectionSlot() in DatabaseConnector with gui-id:" << gui->socketDescriptor();
    }
}

void DatabaseConnector::logData(QueryType type, QVariantList data)
{
    if (mDatabase.isOpen())
    {
        QueryType returnType;
        switch (type)
        {
        case LOG_ROBOT:
            mQuery.prepare("INSERT INTO robot (gripper_status, robot_status, command_line) VALUES (?, ?, ?);");
            returnType = GET_ROBOT;
            break;
        case LOG_BALLPOS:
            mQuery.prepare("INSERT INTO boldpos (Bold_posX, Bold_posY, Bold_posZ) VALUES (?, ?, ?);");
            returnType = GET_BALLPOS;
            while (data.length() > 3) data.pop_back();
            break;
        case LOG_CUPPOS:
            mQuery.prepare("INSERT INTO koppos (kop_posx, kop_posy, kop_posz) VALUES (?, ?, ?);");
            returnType = GET_CUPPOS;
            break;

            // Insert into dynamik starts here.
        case LOG_TCPPOS:
            mQuery.prepare("INSERT INTO tcppos (TCP_posX_start, TCP_posY_start, TCP_posZ_start, "
                           "Hastighed_J1, Hastighed_J2, Hastighed_J3, Hastighed_J4, Hastighed_J5, Hastighed_J6) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);");
            returnType = GET_TCPPOS;
            break;
        case LOG_DYNAMIK:
            mQuery.prepare("INSERT INTO dynamik (Kastetid, acceleration, tcp_index, kop_ramt) VALUES (?, ?, ?, ?);");
            returnType = GET_DYNAMIK;
            break;
        default:
            qDebug() << "Wrong QueryType chosen in lodData()" << QString(getQueryTypeString(type).c_str());
            return;
        }

        for (int i = 0; 0 != data.size() && i < 9; ++i)
        {
            mQuery.addBindValue(data[0]);
            data.pop_front();
        }

        if (!mQuery.exec()) {
            qDebug() << "\nSomething went wrong, could not execute batch";
            qDebug() << mQuery.lastError().text();
            qDebug() << "Error message was:" << mQuery.lastQuery();
            return;
        }
        else {
           //qDebug() << "succeded:" << mQuery.lastQuery();
        }

        int return_index = getData(returnType);
        if (return_index == -1)
        {
            qDebug() << "\nSomething went wrong, getData returned -1 in DatabaseConnector";
            return;
        }

        if (type == LOG_TCPPOS)
        {
            data.insert(2, QVariant(return_index));
            logData(LOG_DYNAMIK, data);
        }
    }
    else
    {
        qDebug() << "\nDatabase not open. failed logData() in DatabaseConnector with Querytype:" << getQueryTypeString(type).c_str() << "QVariantList:" << data;
    }
}

int DatabaseConnector::getData(QueryType type, Client* gui)
{
    if (mDatabase.isOpen())
    {
        QByteArray returnData;
        switch (type)
        {
        case GET_ROBOT_UPDATE_ALL:
            mQuery.exec("SELECT * FROM robot");
            returnData.append("},").append("RobotUpdateAll").append(", ");
            break;
        case GET_ROBOT:
            mQuery.exec("SELECT * FROM robot ORDER BY robot_index DESC LIMIT 1");
            returnData.append("},").append("RobotUpdate").append(", ");
            break;
        case GET_BALLPOS:
            mQuery.exec("SELECT * FROM boldpos ORDER BY bold_pos_index DESC LIMIT 1");
            returnData.append("},").append("BallUpdate").append(", ");
            break;
        case GET_CUPPOS:
            mQuery.exec("SELECT * FROM koppos ORDER BY kop_pos_index DESC LIMIT 1");
            returnData.append("},").append("CupUpdate").append(", ");
            break;
        case GET_TCPPOS:
            mQuery.exec("SELECT * FROM tcppos ORDER BY tcp_index DESC LIMIT 1");
            returnData.append("},").append("TcpUpdate").append(", ");
            break;
        case GET_DYNAMIK:
            mQuery.exec("SELECT * FROM dynamik ORDER BY dynamik_index DESC LIMIT 1");
            returnData.append("},").append("DynamikUpdate").append(", ");
            break;
        default:
            qDebug() << "Wrong QueryType chosen in lodData()" << QString(getQueryTypeString(type).c_str());
        }

        int returnIndex{0};
        if (mQuery.next())
        {
            if (mQuery.size() == 1)
            {
                for (int i = 0; i < mQuery.record().count(); ++i)
                {
                    if (i == 0) returnIndex = mQuery.value(i).toInt();
                    returnData.append(mQuery.value(i).toString()).append(", ");
                }
                returnData.chop(2);
                returnData.append(".");
            }
            else
            {
                QJsonDocument  json;
                QJsonArray     recordsArray;
                do {
                    QJsonObject recordObject;
                    for(int x=0; x < mQuery.record().count(); x++)
                    {
                        recordObject.insert( mQuery.record().fieldName(x),QJsonValue::fromVariant(mQuery.value(x)) );
                    }
                    recordsArray.push_back(recordObject);
                } while(mQuery.next());
                json.setArray(recordsArray);
                returnData.append(json.toJson());
                //cout << endl << json.toJson().toStdString() << endl;
            }
        }
        else
        {
            qDebug() << "Error in GetData() executing query:" << mQuery.lastQuery() << "No data available in table";
            returnData.chop(returnData.size() - returnData.lastIndexOf('}'));
        }

        if (gui == nullptr)
        {
            emit updateForGUIsSignal(returnData);
        }
        else
        {
            if (!returnData.isEmpty())
            {
                //gui->write(returnData);
            }
            else
            {
                //qDebug() << "Individual has no data, moving on";
            }
        }
        return returnIndex;
    }
    else
    {
        qDebug() << "\nDatabase not open. failed getData() in DatabaseConnector with Querytype:" << getQueryTypeString(type).c_str() << "gui:" << gui->socketDescriptor();
        return  -1;
    }
}
