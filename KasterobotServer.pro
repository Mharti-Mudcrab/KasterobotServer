QT       += core
QT       += network
QT       += sql
QT       -= gui

TARGET = TcpServer
CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += /usr/local/include

TEMPLATE = app

SOURCES += main.cpp \
    Client.cpp \
    ClientHandler.cpp \
    DatabaseConnector.cpp \
    GripperClient.cpp \
    DashboardClient.cpp \
    Server.cpp

HEADERS += \
    Client.h \
    ClientHandler.h \
    ConnectionTester.h \
    DatabaseConnector.h \
    GripperClient.h \
    DashboardClient.h \
    Server.h
