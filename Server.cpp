#include "Server.h"

using namespace std;

Server::Server(QObject *parent)
    : QTcpServer (parent), mIsProgramEnabled{false} , mClientHandler{this}, mGripper{this}, mDashboard{this}, mTimer{this}
{
    stringstream log;
    if(listen(QHostAddress::Any, 60065))
    {
        log << "\nServer started. listening on port: 60065";
        qDebug() << log.str().c_str();
    }
    else
    {
        qDebug() << "\nServer could not start";
    }

    // Initialising mTimer
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(throwTimeOutSlot()), Qt::ConnectionType::DirectConnection);
    mTimer.setSingleShot(true);
    mTimer.setTimerType(Qt::PreciseTimer);

    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}

void Server::goButtonOnGUISlot()
{
    stringstream log;
    mIsProgramEnabled = !mIsProgramEnabled;
    log << endl << "The \"Go\" button has been clicked on a GUI, and the system is now: " << (mIsProgramEnabled ? "Enabled" : "Disabled");
    if (mIsProgramEnabled)
    {
        log << endl << "Staring Program. Homing Gripper, and Enabling gripper- and robotStateChangedSlot()";
        qDebug() << log.str().c_str();
        mGripper.homeSlot();
        mDashboard.startProgramSlot();
    }
    else
    {
        log << endl << "Closing Program. Stopping Gripper, and disabling gripper- and robotStateChangedSlot()";
        qDebug() << log.str().c_str();
        mDashboard.stopProgram();
        mGripper.stopGripperSlot();
    }
    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}


void Server::gripperStateChangedSlot(GripperState state)
{
    cout << endl << "\tgripperSateChangedSlot() called. GripperState: " << mGripper.getStateString().toStdString() << endl;
    stringstream log;
    if (mIsProgramEnabled)
    {
        switch (state)
        {
        case GRIPPER_RELEASED: // RELEASED only called while THROWING
        {
            switch (mClientHandler.getRobotState())
            {
            case ROBOT_THROWING:
            {
                log << "Gripper is done releasing during throw, calling moveSlot() on mGripper.\n"
                    << "Robot is throwing.";
                qDebug() << log.str().c_str() << endl;
                mGripper.moveSlot();
                break;
            }
            case ROBOT_MOVING_TO_HOME:
            {
                log << "Gripper is done releasing during throw, calling moveSlot() on mGripper.\n"
                    << "Robot is moving to home.";
                qDebug() << log.str().c_str() << endl;
                mGripper.moveSlot();
                break;
            }
            default:
            {
                log << "Error in gripperSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", state: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case GRIPPER_OPEN: // OPEN called, either after RELEASED during THROWING, or before MOVING_DOWN_TO_PART.
        {
            switch (mClientHandler.getRobotState())
            {
            case ROBOT_THROWING:
            {
                log << "Gripper has reached HOME position. Waiting for robot to enter WAITING_TO_GET_LOCATION state.\n"
                    << "Robot is curerntly in THROWING state";
                if (!mTargetPos.empty())
                {
                    log << "\nProceeding with sending \"getBallPos\" to camera, to check success of throw.";
                    emit sendToCameraSignal("getBallPos");
                }
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case ROBOT_MOVING_TO_HOME:
            {
                log << "Gripper is in HOME position. Robot is in MOVING_TO_HOME. Waiting for WAITING_TO_GET_LOCATION from robot.";
                if (!mTargetPos.empty())
                {
                    log << "\nProceeding with sending \"getBallPos\" to camera, to check success of throw.";
                    qDebug() << log.str().c_str() << endl;
                    emit sendToCameraSignal("getBallPos");
                    break;
                }
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case ROBOT_WAITING_TO_GET_LOCATION:
            {
                log << "Gripper has reached HOME position. Robot is already in WAITING_TO_GET_LOCATION state.\n"
                    << "Proceeding with getNewBallPosSlot()";
                qDebug() << log.str().c_str() << endl;
                emit sendToCameraSignal("getBallPos");
                break;
            }
            case ROBOT_WAITING_FOR_OPEN:
            {
                log << "Gripper has reached HOME. Sending \"Gripper opened\" to robot with sendToRobotSignal()";
                qDebug() << log.str().c_str() << endl;
                mClientHandler.setRobotState(ROBOT_MOVING_DOWN_TO_PART);
                emit sendToRobotSignal("Gripper opened");
                break;
            }
            default:
            {
                log << "Error in gripperSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", state: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case GRIPPER_HOLDING: // Can only happen when picking up.
        {
            switch (mClientHandler.getRobotState()) {
            case ROBOT_WAITING_FOR_GRIP:
            {
                log << "Gripper is HOLDING ball. Sending \"Gripper holding\" to robot with sendToRobotSignal()";
                qDebug() << log.str().c_str() << endl;
                emit sendToRobotSignal("Gripper holding");
                mClientHandler.setRobotState(ROBOT_MOVING_UP_WITH_PART);
                break;
            }
            default:
            {
                log << "Error in gripperSateChangedSlot(). RobotState: " << mClientHandler.getRobotStateString() << ", state: " << mGripper.getStateString().toStdString() << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case GRIPPER_HOME: // Can only happen when initialising connection to gripper.
        {
            switch (mClientHandler.getRobotState()) {
            case ROBOT_DISCONNECTED:
            {
                log << "Initialising. Gripper is in the HOME. Robot is DISCONNECTED.";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case ROBOT_MOVING_TO_HOME:
            {
                log << "Gripper is in the HOME position. Robot is MOVING_TO_HOME.";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case ROBOT_WAITING_TO_GET_LOCATION:
            {
                log << "Gripper is in the HOME position. The robot is also in HOME.\n"
                    << "Proceeding with getting next ballposiyion from camera.";
                qDebug() << log.str().c_str() << endl;
                emit sendToCameraSignal("getBallPos");
                break;
            }
            default:
            {
                log << "Error in gripperSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", state: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case GRIPPER_FAILED_GRIP: // Can only happen when gripper has failed grip
        {
            log << "Gripper has failed grip. HOMING gripper, and sending \"Grip failed\" to robot.";
            qDebug() << log.str().c_str() << endl;
            emit sendToRobotSignal("Grip failed");
            mGripper.moveSlot();
            break;
        }
        case GRIPPER_DISCONNECTED: // If gripper disconnects Robot should be stopped in order to avoid damage.
        {
            log << "Terminating robot program to avoid damage";
            qDebug() << log.str().c_str() << endl;
            mIsProgramEnabled = false;
            mDashboard.stopProgram();
            break;
        }
        default: // Gripper state changed, but we mostly dont care.
        {
            log << "Gripper state changed, but we dont care too much. State is: " << mGripper.getStateString().toStdString() << endl;
            qDebug() << log.str().c_str() << endl;
            break;
        }
        }
    }
    else
    {
        log << "System is Disabled, and no action can be taken on behalf of gripperStateChanged()";
    }
    cout << log.str() << endl;
    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}
void Server::robotStateChangedSlot(RobotState state)
{
    cout << endl << "\trobotSateChangedSlot() called. RobotState: " << mClientHandler.getRobotStateString() << endl;
    stringstream log;
    if (mIsProgramEnabled)
    {
        switch (state)
        {
        case ROBOT_WAITING_TO_GET_LOCATION: // Robot is done moving to home, either from INITIALIZE or THROWING.
        {
            switch (mGripper.getState())
            {
            case GRIPPER_HOMING:
            {
                log << "Initialising. Robot is in HOME. Gripper not yet in HOME. waiting for GRIPPER_HOME.";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case GRIPPER_HOME:
            {
                log << "Initialising. Robot is in the HOME position. Gripper is also in HOME.\n"
                    << "Proceeding with getting next ballposiyion from camera.";
                qDebug() << log.str().c_str() << endl;
                emit sendToCameraSignal("getBallPos");
                break;
            }
            case GRIPPER_OPENING:
            {
                log << "Robot is in HOME. Gripper is in the OPENING state. waiting for gripperStateChangedSignal().";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case GRIPPER_OPEN:
            {
                log << "Robot is in the HOME position. Gripper is also in HOME.\n"
                    << "Proceeding with getting next ballposiyion from camera.";
                qDebug() << log.str().c_str() << endl;
                emit sendToCameraSignal("getBallPos");
                break;
            }
            case GRIPPER_RELEASING:
            {
                log << "Robot is in HOME. Gripper is in the RELEASING state. waiting for GRIPPER_OPEN.";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            case GRIPPER_RELEASED:
            {
                log << "Robot is in HOME. Gripper is in the RELEASED state. waiting for GRIPPER_OPEN.";
                qDebug() << log.str().c_str() << endl;
                break;
            }
            default:
            {
                log << "Error in robotSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case ROBOT_WAITING_FOR_OPEN: // Only WAITING_FOR_RELEASE when over part.
        {
            switch (mGripper.getState())
            {
            case GRIPPER_HOME:
            {
                log << "Robot is at location. Gripper is HOME. Sending \"Gripper opened\" to robot with sendToRobotSignal()";
                qDebug() << log.str().c_str() << endl;
                emit sendToRobotSignal("Gripper opened");
                mClientHandler.setRobotState(ROBOT_MOVING_DOWN_TO_PART);
                break;
            }
            case GRIPPER_OPEN: // implemented for move shit around test
            {
                log << "Robot is at location. Gripper is OPEN. Sending \"Gripper opened\" to robot with sendToRobotSignal()";
                qDebug() << log.str().c_str() << endl;
                emit sendToRobotSignal("Gripper opened");
                mClientHandler.setRobotState(ROBOT_MOVING_DOWN_TO_PART);
                break;
            }
            case GRIPPER_HOLDING: // implemented for move shit around test
            {
                log << "Robot is at location. Gripper is HOLDING. Openning Gripper";
                qDebug() << log.str().c_str() << endl;
                mClientHandler.setRobotState(ROBOT_WAITING_FOR_OPEN);
                mGripper.moveSlot();
                break;
            }
            default:
            {
                log << "Error in robotSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case ROBOT_WAITING_FOR_GRIP: // Only WAITING_FOR_GRIP when down by part.
        {
            switch (mGripper.getState())
            {
            case GRIPPER_OPEN:
            {
                log << "Robot is done moving down to part. Calling gripSlot(). Setting program state to: "
                    << mClientHandler.getRobotStateString() << endl;
                qDebug() << log.str().c_str() << endl;
                mGripper.gripSlot();
                break;
            }
            case GRIPPER_HOME:
            {
                log << "Robot is done moving down to part. Calling gripSlot(). Setting program state to: "
                    << mClientHandler.getRobotStateString() << endl;
                qDebug() << log.str().c_str() << endl;
                mGripper.gripSlot();
                break;
            }
            default:
            {
                log << "Error in robotSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case ROBOT_WAITING_TO_THROW: // Only WAITING_TO_THROW when up after part.
        {
            switch (mGripper.getState())
            {
            case GRIPPER_HOLDING:
            {
                //mProgramState = PROG_WAITING_TO_THROW;
                log << "Robot waiting to get parameters for throw. Proceeding with getting target from camera.";
                qDebug() << log.str().c_str() << endl;
                emit sendToCameraSignal("getTargetPos");
                break;
            }
            default:
            {
                log << "Error in robotSateChangedSlot(). RobotState: \""
                    << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                qDebug() << log.str().c_str() << endl;
                break;
            }
            }
            break;
        }
        case ROBOT_THROWING: // Only THROWING when robot has started throwing.
        {
            switch (mGripper.getState())
            {
            case GRIPPER_HOLDING:
            {
                mTimer.start();
                mTimestamps[0] = QDateTime::currentMSecsSinceEpoch();
                log << "Robot starting throw with milli-timestamp" << mTimestamps[0];
                qDebug() << log.str().c_str() << endl;
                break;
            }
            default:
            {
                cerr << "Error in robotSateChangedSlot(). RobotState: \""
                     << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
                //mProgramState = PROG_ERROR;
                break;
            }
            }
            break;
        }
        case ROBOT_DISCONNECTED: // Opening Gripper when disconnects.
        {
            log << "Homing Gripper upon Robot disconnect";
            qDebug() << log.str().c_str() << endl;
            mIsProgramEnabled = false;
            mGripper.homeSlot();
            break;
        }
        default: // This should never be the case.
        {
            log << "Error in robotSateChangedSlot(). RobotState: \""
                << mClientHandler.getRobotStateString() << "\", GripperState: \"" << mGripper.getStateString().toStdString() << "\"" << endl;
            //mProgramState = PROG_ERROR;
            break;
        }
        }
    }
    else
    {
        log << "System is Disabled, and no action can be taken on behalf of robotStateChanged()";
        qDebug() << log.str().c_str() << endl;
    }
    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}

void Server::throwTimeOutSlot()
{
    mGripper.releaseSlot();
    mTimestamps[1] = QDateTime::currentMSecsSinceEpoch();
    stringstream log;
    log << "Calculated timing on robot Throw is: " << mTimestamps[1] << " - " << mTimestamps[0] << " = " << mTimestamps[1]-mTimestamps[0] << "ms.";
    qDebug() << log.str().c_str();
    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}

void Server::getThrowParametersFromCalculation(double x, double y) // NEEDS IMPLEMENTATION
{
    mTargetPos.clear();
    mTargetPos << x << y;
    emit logInDatabaseSignal(LOG_CUPPOS, QVariantList() << x << y << 0.17);
    string cinString;
    QByteArray sendToRobotString;
    stringstream out;
    do {
        qDebug() << "\ngetThrowParametersFromCalculation(double x, double y) called (I know.. it's a work in progress)\n"
                    "Parameters are x =" << x << "y =" << y;
                    qDebug() << "   NOT YET IMPLEMENTED, ONLY WORKS ON MANUAL INPUT!" << endl;
        cout << "        Enter START position for robot throw: ";
        std::getline(cin, cinString);
        sendToRobotString.append(cinString.c_str()).append(" ");

        cout << "          Enter SPEED VECTOR for robot throw: ";
        std::getline(cin, cinString);
        sendToRobotString.append(cinString.c_str()).append(" ");

        cout << "Enter TIME needed for gripperRelease timeout: ";
        int time;
        cin >> time;
        if (cin.fail()) {
            cin.clear();
                qDebug() << "cin.fail == true";
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // cleaning cin up for later.
        mTimer.setInterval(time);

        stringstream stream(sendToRobotString.toStdString());
        out << "(";
        double temp;
        while (!stream.eof())
        {
            if (stream.peek() != '(' && stream.peek() != ')' &&
                stream.peek() != ',' && stream.peek() != ' ' && stream.peek() != EOF)
            {
                stream >> temp;
                mThrowParameters << temp;
                out << temp << ", ";
            }
            else
            {
                stream.ignore(1);
            }
        }
        mThrowParameters << time << 10;
        if (mThrowParameters.size() != 14)
        {
            qDebug() << "\nWrong number of inputs, number of inputs was:" << mThrowParameters.size() << "\b. It should have been 14, try again";
            mThrowParameters.clear();
        }
    } while (mThrowParameters.size() != 14);


    for (int i = 0; i < 3; ++i) mThrowParameters.removeAt(3);
    sendToRobotString = out.str().c_str();
    sendToRobotString.chop(2);
    emit sendToRobotSignal(sendToRobotString.append(")"));
}
void Server::getNewBallPosSlot(QByteArray ballPos)
{
    stringstream stream(ballPos.toStdString());
    QVariantList tempBallPos;
    double temp;
    while (!stream.eof())
    {
        if (stream.peek() != '(' && stream.peek() != ')' && stream.peek() != ',')
        {
            stream >> temp;
            tempBallPos << temp;
        }
        else
        {
            stream.ignore(1);
        }
    }
    tempBallPos.pop_back();

    if (mThrowParameters.empty())
    {
        emit sendToRobotSignal(ballPos);
        mClientHandler.setRobotState(ROBOT_MOVING_TO_LOCATION);
        emit logInDatabaseSignal(LOG_BALLPOS, tempBallPos);
    }
    else
    {
        double targetRadius = 0.0471;
        cout << endl << endl << "Now to determine success\n"
                            "sqrt( pow( " << mTargetPos.value(0).toDouble() << " - " << tempBallPos.value(0).toDouble() << " , 2) + "
                                  "pow( " << mTargetPos.value(1).toDouble() << " - " << tempBallPos.value(1).toDouble() << " , 2))";
        double distanceToTargetCenter = sqrt(pow(mTargetPos.value(0).toDouble() - tempBallPos.value(0).toDouble(), 2) +
                                             pow(mTargetPos.value(1).toDouble() - tempBallPos.value(1).toDouble(), 2));
        cout << " = " << distanceToTargetCenter << endl;
        cout << "\t" << distanceToTargetCenter << " < " << targetRadius << " = " << (distanceToTargetCenter < targetRadius ? "true" : "false");
        if (distanceToTargetCenter < targetRadius)
            mThrowParameters << 1;
        else
            mThrowParameters << 0;

        emit logInDatabaseSignal(LOG_TCPPOS, mThrowParameters);
        mThrowParameters.clear();
    }
}

void Server::cameraConnectedSlot()
{
    stringstream log;
    switch (mClientHandler.getRobotState())
    {
    case ROBOT_WAITING_TO_THROW:
    {
        if (mGripper.getState() == GRIPPER_HOME || mGripper.getState() == GRIPPER_OPEN)
        {
            log << "Camera is needed imediately after connection, and is set to get a targetposition";
            emit sendToCameraSignal("getTargetPos");
        }
        break;
    }
    case ROBOT_WAITING_TO_GET_LOCATION:
    {
        if (mGripper.getState() == GRIPPER_HOME || mGripper.getState() == GRIPPER_OPEN)
        {
            log << "Camera is needed imediately after connection, and is set to get the ballposition";
            emit sendToCameraSignal("getBallPos");
        }
        break;
    }
    default:
    {
        log << "Camera is not needed at the moment of connection, doing nothing.";
        break;
    }
    }
    qDebug() << log.str().c_str();
    emit logInDatabaseSignal(LOG_ROBOT, QVariantList({mGripper.getStateString(), mClientHandler.getRobotStateString().c_str(), log.str().c_str()}));
}
void Server::incomingConnection(qintptr handle)
{
    emit newConnectionSignal(handle);
}
