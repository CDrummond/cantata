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

if(WIN32)
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_win.cpp)
elseif(WIN32)
  set(SINGLEAPP-SOURCES ${SINGLEAPP-SOURCES} qtlockedfile_unix.cpp)
endif(WIN32)

if (ENABLE_QT5)
  qt5_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
else (ENABLE_QT5)
  qt4_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
endif (ENABLE_QT5)

ADD_LIBRARY(qtsingleapplication STATIC
    ${SINGLEAPP-SOURCES}
    ${SINGLEAPP-SOURCES-MOC}
)
