cmake_minimum_required(VERSION 2.6)

set(KCAT_SOURCES
  kcategorizedsortfilterproxymodel.cpp
  kcategorizedview.cpp 
  kcategorydrawer.cpp
)

#set(KCAT_MOC_HEADERS
#  kcategorizedview.h
#  kcategorydrawer.h
#)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

#QT5_WRAP_CPP(KCAT_SOURCES_MOC ${KCAT_MOC_HEADERS})
add_definitions(-DKITEMVIEWS_NO_DEPRECATED)

ADD_LIBRARY(kcategorizedviews STATIC
  ${KCAT_SOURCES}
  #${KCAT_SOURCES_MOC}
)
