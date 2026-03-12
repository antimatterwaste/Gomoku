QT       += core gui multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += \
    $$PWD/src

SOURCES += \
    $$PWD/src/click_label.cpp \
    $$PWD/src/gomo5yu.cpp \
    $$PWD/src/main.cpp

HEADERS += \
    $$PWD/src/click_label.h \
    $$PWD/src/gomo5yu.h

FORMS += \
    $$PWD/src/gomo5yu.ui
