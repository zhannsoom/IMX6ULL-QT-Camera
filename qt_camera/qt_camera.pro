QT += widgets

CONFIG += c++11
CONFIG -= app_bundle

TEMPLATE = app
TARGET = imx6ull_qt_camera

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    camerawindow.cpp \
    v4l2capture.cpp \
    aviwriter.cpp

HEADERS += \
    camerawindow.h \
    v4l2capture.h \
    aviwriter.h

RESOURCES += resources.qrc

linux:LIBS +=
