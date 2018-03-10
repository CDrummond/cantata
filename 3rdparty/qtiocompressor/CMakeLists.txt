cmake_minimum_required(VERSION 2.6)

include_directories(${QTINCLUDES} ${ZLIB_INCLUDE_DIRS} ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
ADD_LIBRARY(qtiocompressor STATIC qtiocompressor.cpp)
