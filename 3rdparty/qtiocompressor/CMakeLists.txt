cmake_minimum_required(VERSION 2.6)

set(IOCOMPRESSOR-SOURCES
    qtiocompressor.cpp
)

include_directories(${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})

ADD_LIBRARY(qtiocompressor STATIC
    ${IOCOMPRESSOR-SOURCES}
)
