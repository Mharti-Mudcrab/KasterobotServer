#include "ClientHandler.h"
#include "client.h"

using namespace std;

ClientHandler::ClientHandler(QObject *parent)
    : QObject(parent), mRobotState{ROBOT_DISCONNECTED}, isCameraConnected{true}, mConnected{0}, mDbCon{this},
      mCameraTimer{this}, mPendingCameraCommand{"No pending command"}
{
    // general incomming from server
    connect(parent, SIGNAL(newConnectionSignal(qintptr)), this, SLOT(newConnectionSlot(qintptr)));
    connect(parent, SIGNAL(logInDatabaseSignal(QueryType, QVariantList)), this, SLOT(logInDatabaseSlot(QueryType, QVariantList)));
    // general outgoing to server
    connect(this, SIGNAL(robotStateChangedSignal(RobotState)), parent, SLOT(robotStateChangedSlot(RobotState)));
    connect(this, SIGNAL(goButtonOnGUISignal()), parent, SLOT(goButtonOnGUISlot()));
    connect(this, SIGNAL(cameraConnectedSignal()), parent, SLOT(cameraConnectedSlot()));
    connect(this, SIGNAL(newTargetPosSignal(double, double)), parent, SLOT(getThrowParametersFromCalculation(double, double)));
    connect(this, SIGNAL(newBallPosSignal(QByteArray)), parent, SLOT(getNewBallPosSlot(QByteArray)));
    // general write-out signals and slots, coming from serv.
    connect(parent, SIGNAL(sendToRobotSignal(QByteArray)), this, SLOT(sendToRobotSlot(QByteArray)));
    connect(parent, SIGNAL(sendToCameraSignal(QByteArray)), this, SLOT(sendToCameraSlot(QByteArray)));

    // camera setup
    mCameraTimer.setSingleShot(true);
    mCameraTimer.setInterval(5000);
    connect(&mCameraTimer, SIGNAL(timeout()), this, SLOT(cameraTimerTimeoutSlot()));
}

RobotState ClientHandler::getRobotState() const
{
    return mRobotState;
}
void ClientHandler::setRobotState(RobotState state)
{
    mRobotState = state;
}
std::string ClientHandler::getRobotStateString() const
{
    static std::map<RobotState, std::string> strings;
    if (strings.size() == 0){
    #define INSERT_ELEMENT(i) strings[i] = #i
            INSERT_ELEMENT(ROBOT_DISCONNECTED);
            INSERT_ELEMENT(ROBOT_MOVING_TO_HOME);
            INSERT_ELEMENT(ROBOT_WAITING_TO_GET_LOCATION);
            INSERT_ELEMENT(ROBOT_MOVING_TO_LOCATION);
            INSERT_ELEMENT(ROBOT_WAITING_FOR_OPEN);
            INSERT_ELEMENT(ROBOT_MOVING_DOWN_TO_PART);
            INSERT_ELEMENT(ROBOT_WAITING_FOR_GRIP);
            INSERT_ELEMENT(ROBOT_MOVING_UP_WITH_PART);
            INSERT_ELEMENT(ROBOT_WAITING_TO_THROW);
            INSERT_ELEMENT(ROBOT_MOVING_TO_THROW);
            INSERT_ELEMENT(ROBOT_THROWING);
     #undef INSERT_ELEMENT
    }
    return strings[mRobotState];
}

void ClientHandler::newConnectionSlot(qintptr handle)
{
    Client *newClient = new Client(this, handle);
    stringstream log;
    if (newClient->getClientType() == CLIENT_ROBOT)
    {
        mRobotState = ROBOT_MOVING_TO_HOME;
        log << "Robot connected with socket-id: " << newClient->getId();
    }
    else if (newClient->getClientType() == CLIENT_CAMERA)
    {
        isCameraConnected = true;
        log << "Camera connected with socket-id: " << newClient->getId();
        mCameraTimer.start();
        emit cameraConnectedSignal();
    }
    else
    {
        mConnected++;
        log << "New GUI connected with socket-id: " << newClient->getId() << ", GUI count: " << mConnected;
        mDbCon.updateNewGUIConnection(newClient);
    }
    qDebug() << log.str().c_str();
    mDbCon.logData(LOG_ROBOT, QVariantList({GripperClient::getStateString(), getRobotStateString().c_str(), log.str().c_str()}));
}
void ClientHandler::disconnectedSlot()
{
    Client *client = qobject_cast<Client*>(sender());
    stringstream log;
    switch (client->getClientType())
    {
    case CLIENT_ROBOT:
        {
            log << "Robot disconnected with id: " << client->getId();
            mRobotState = ROBOT_DISCONNECTED;
            emit robotStateChangedSignal(mRobotState);
            break;
        }
    case CLIENT_GUI:
        {
            mConnected--;
            log << "GUI disconnected with id: " << client->getId();
            log << "\nServer now has " << mConnected << " connected GUI(s)";
            break;
        }
    case CLIENT_CAMERA:
        {
            log << "Camera disconnected with id: " << client->getId();
            isCameraConnected = false;
            break;
        }
    }
    qDebug() << log.str().c_str();
    mDbCon.logData(LOG_ROBOT, QVariantList({GripperClient::getStateString(), getRobotStateString().c_str(), log.str().c_str()}));
}

void ClientHandler::newDataFromClientSlot() // GUI AND CAMERA NEED TO BE IMPLEMENTED
{
    Client *client = qobject_cast<Client*>(sender());
    QByteArray data = client->readAll();
    if (data.endsWith("\r\n"))
    {
        data.chop(2);
    }
    else if (data.endsWith("\n") || data.endsWith("\r"))
    {
        data.chop(1);
    }

    if (data.isEmpty()) return;

    stringstream log;

    switch (client->getClientType())
    {
    case CLIENT_ROBOT:
    {
        log << "Incoming data from Robot: " << data.toStdString() << " on socket: " << client->getId();
        if (data.contains("moving to home"))
        {
            mRobotState = ROBOT_MOVING_TO_HOME;
        }
        else if (data.contains("ready to pick up"))
        {
            mRobotState = ROBOT_WAITING_TO_GET_LOCATION;
        }
        else if (data.contains("Open"))
        {
            mRobotState = ROBOT_WAITING_FOR_OPEN;
        }
        else if (data.contains("Close"))
        {
            mRobotState = ROBOT_WAITING_FOR_GRIP;
        }
        else if (data.contains("ready to throw"))
        {
            mRobotState = ROBOT_WAITING_TO_THROW;
        }
        else if (data.contains("throwing"))
        {
            mRobotState = ROBOT_THROWING;
        }
        else {
            log << "\nCommand from robot was not understood, or we just dont care about it, data is:" << data.toStdString();
            break;
        }
        emit robotStateChangedSignal(mRobotState);
        break;
    }
    case CLIENT_CAMERA:
    {
        log << "Incoming data from Camera: " << data.toStdString() << " on socket: " << client->getId();
        if (data.startsWith("Ball"))
        {
            emit newBallPosSignal(data.mid(4));
        }
        else
        {
            stringstream stream(data.toStdString());
            double x,y;
            stream.ignore(6);
            while (!stream.eof())
            {
                stream >> x;
                stream.ignore(1); // ignore ','
                stream >> y;
                mTargetPos.push({x,y});
                stream.ignore(1); // ignore ','
            }
            emit newTargetPosSignal(mTargetPos.front()[0], mTargetPos.front()[1]);
            mTargetPos.pop();
        }
        break;
    }
    case CLIENT_GUI:
    {
        if (data.startsWith("Connect"))
        {
            client->write("},ok");
            return;
        }
        else if (data.startsWith("Go"))
        {
            log << "Incoming data from GUI: " << data.toStdString() << " on socket: " << client->getId();
            emit goButtonOnGUISignal();
            break;
        }
        else
        {

        }
    }
    }
    qDebug() << log.str().c_str();
    mDbCon.logData(LOG_ROBOT, QVariantList({GripperClient::getStateString(), getRobotStateString().c_str(), log.str().c_str()}));
}

void ClientHandler::sendToRobotSlot(QByteArray data)
{
    stringstream log;
    log << "Sending \"" << data.toStdString() << "\" out to Robot";
    emit sendDataToRobotSignal(data);
    cout << endl << log.str();
    mDbCon.logData(LOG_ROBOT, QVariantList({GripperClient::getStateString(), getRobotStateString().c_str(), log.str().c_str()}));
}
void ClientHandler::sendToCameraSlot(QByteArray data)
{
    stringstream log;

    if (mCameraTimer.isActive())
    {
        mPendingCameraCommand = data;
        log << "Server Tried to send camera command: " << data.toStdString() << " but cooldown on camera has not finished.";
    }
    else
    {
        if (isCameraConnected)
        {
            if (data.startsWith("getTargetPos") && !mTargetPos.empty())
            {
                log << "Picking out next target from memory. ";
                if (mTargetPos.size()-1 == 0)
                    log << " This is the last one.";
                else
                    log << " There are " << mTargetPos.size()-1 << " Targets left in memory";
                emit newTargetPosSignal(mTargetPos.front()[0], mTargetPos.front()[1]);
                mTargetPos.pop();
            }
            else
            {
                log << "Sending \"" << data.toStdString() << "\" out to Camera";
                emit sendDataToCameraSignal(data);
            }
            mCameraTimer.start();
        }
        else
        {
            mPendingCameraCommand = data;
            log << "Camera is not connected to handle request, but will catch up once it does";
        }
    }
    qDebug() << log.str().c_str();
    mDbCon.logData(LOG_ROBOT, QVariantList({GripperClient::getStateString(), getRobotStateString().c_str(), log.str().c_str()}));
}
void ClientHandler::sendToGUISlot(QByteArray data)
{
    if (mConnected > 0)
    {
        stringstream log;
        //cout << endl << "Sending databaseupdate out to " << mConnected << (mConnected > 1 ? " GUI's" : " GUI") << endl;
        //emit sendDataToGUISignal(data);
        qDebug() << log.str().c_str();
    }
    else
    {
        //qDebug() << "No GUI's connected, data was:" << data;
    }
}

void ClientHandler::cameraTimerTimeoutSlot()
{
    qDebug() << "cameraTImerTimoutSlot() called, camera is open for receiving commands once again";
    if (mPendingCameraCommand != "No pending command")
    {
        sendDataToCameraSignal(mPendingCameraCommand);
        qDebug() << "There was a pending command for the camera:" << mPendingCameraCommand << "it is send to the camera immediately";
        mPendingCameraCommand = "No pending command";
        mCameraTimer.start();
    }
}

void ClientHandler::logInDatabaseSlot(QueryType type, QVariantList data)
{
    mDbCon.logData(type, data);
}
