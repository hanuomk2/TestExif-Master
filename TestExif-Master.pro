#-------------------------------------------------
#
# Project created by QtCreator 2017-03-30T09:07:32
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TestExif-Master
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    exif-master/exif.c

HEADERS  += widget.h \
    exif-master/exif.h

FORMS    += widget.ui
