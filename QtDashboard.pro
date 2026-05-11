QT += core gui widgets network

TARGET = QtDashboard
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    SupervisorClient.cpp \
    ModeLoader.cpp \
    UserConfig.cpp

HEADERS += \
    MainWindow.h \
    SupervisorClient.h \
    ModeLoader.h \
    UserConfig.h

FORMS += \
    MainWindow.ui

QMAKE_LFLAGS += -no-pie
