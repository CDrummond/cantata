#-------------------------------------------------
#
# Project created by QtCreator 2013-04-18T13:28:42
#
#-------------------------------------------------

TARGET = QtAwesome
TEMPLATE = lib
CONFIG += staticlib c++11

SOURCES += QtAwesome.cpp
HEADERS += QtAwesome.h

isEmpty(PREFIX) {
    unix {
        PREFIX = /usr
    } else {
        PREFIX = $$[QT_INSTALL_PREFIX]
    }
}

install_headers.files = QtAwesome.h
install_headers.path = $$PREFIX/include
target.path = $$PREFIX/lib
INSTALLS += install_headers target

RESOURCES += \
    QtAwesome.qrc
