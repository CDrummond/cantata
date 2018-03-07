cmake_minimum_required(VERSION 2.6)

set(SINGLEAPP-SOURCES
    qtlocalpeer.cpp
    qtlockedfile.cpp
    qtsingleapplication.cpp
    qtsinglecoreapplication.cpp
)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

if(WIN32)
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_win.cpp)
else()
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_unix.cpp)
endif()

ADD_LIBRARY(qtsingleapplication STATIC
    ${SINGLEAPP-SOURCES}
)
