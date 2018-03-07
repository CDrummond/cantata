cmake_minimum_required(VERSION 2.6)

set(QXT-SOURCES
  qxtglobal.cpp
  qxtglobalshortcut.cpp
)

if (NOT WIN32 AND NOT APPLE)
  find_package(X11)
  include_directories(${X11_INCLUDE_DIR})
endif ()

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

if(WIN32)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_win.cpp)
elseif(APPLE)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_mac.cpp)
else()
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_x11.cpp)
endif()

ADD_LIBRARY(qxt STATIC
  ${QXT-SOURCES}
)
