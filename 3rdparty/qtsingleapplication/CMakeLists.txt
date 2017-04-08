cmake_minimum_required(VERSION 2.6)

set(SINGLEAPP-SOURCES
    qtlocalpeer.cpp
    qtlockedfile.cpp
    qtsingleapplication.cpp
    qtsinglecoreapplication.cpp
)

set(SINGLEAPP-MOC-HEADERS
    qtlocalpeer.h
    qtsingleapplication.h
    qtsinglecoreapplication.h
)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

if(WIN32)
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_win.cpp)
elseif()
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_unix.cpp)
endif()

qt5_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})

ADD_LIBRARY(qtsingleapplication STATIC
    ${SINGLEAPP-SOURCES}
    ${SINGLEAPP-SOURCES-MOC}
)
