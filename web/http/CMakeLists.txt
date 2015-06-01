cmake_minimum_required(VERSION 2.6)

set(httpserver_SOURCES httpconnection.cpp httprequest.cpp
    httpresponse.cpp basichttpserver.cpp httpcookie.cpp
    staticfilecontroller.cpp)

set(httpserver_MOC_HEADERS httpconnection.h httprequesthandler.h basichttpserver.h)

QT5_WRAP_CPP(httpserver_SOURCES_MOC ${httpserver_MOC_HEADERS})

include_directories(${QTINCLUDES})

ADD_LIBRARY(httpserver STATIC ${httpserver_SOURCES} ${httpserver_SOURCES_MOC})
