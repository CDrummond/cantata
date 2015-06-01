PROJECT(cantata-web)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/../cmake)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TBZ2")
set(DEBIAN_PACKAGE_DESCRIPTION "Web Client for MPD")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_PACKAGE_VERSION_SPIN "")  # Use ".$number" - e.g. ".1"
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_WEB_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_WEB_VERSION_FULL "${CANTATA_WEB_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CANTATA_WEB_VERSION_WITH_SPIN "${CANTATA_WEB_VERSION_FULL}${CPACK_PACKAGE_VERSION_SPIN}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_WEB_VERSION_WITH_SPIN}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_WEB_VERSION_WITH_SPIN}")
set(CPACK_SOURCE_IGNORE_FILES  "/.svn/;CMakeLists.txt.user")
include(CPack)

find_package(Qt5Core REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5WebSockets REQUIRED)
set(QTCORELIBS ${Qt5Core_LIBRARIES})
set(QTNETWORKLIBS ${Qt5Network_LIBRARIES})
set(QTLIBS ${QTCORELIBS} ${QTNETWORKLIBS} ${Qt5Concurrent_LIBRARIES})
set(QTINCLUDES ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS} ${Qt5Sql_INCLUDE_DIRS} ${Qt5WebSockets_INCLUDE_DIRS})
if (Qt5_POSITION_INDEPENDENT_CODE)
    SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(Qt5_POSITION_INDEPENDENT_CODE)

set(ENABLE_STREAMS OFF)
set(ENABLE_QT5 ON)
set(ENABLE_WEB ON)

#find_package(Sqlite REQUIRED)
add_definitions(${Qt5Network_DEFINITIONS} ${Qt5Concurrent_DEFINITIONS} ${Qt5Sql_DEFINITIONS} ${Qt5WebSockets_DEFINITIONS} ${SQLITE_DEFINITIONS} -DCANTATA_WEB)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                    ${QTINCLUDES} ${SQLITE_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/..)

set(CANTATA_WEB_SRCS ${CANTATA_WEB_SRCS}
    app/main.cpp app/application.cpp app/httpserver.cpp app/apihandler.cpp app/libraryapi.cpp app/playlistsapi.cpp
    app/playqueueapi.cpp app/streamsapi.cpp app/coversapi.cpp app/statusapi.cpp
    ../db/librarydb.cpp ../db/mpdlibrarydb.cpp
    ../mpd-interface/mpdconnection.cpp ../mpd-interface/mpdparseutils.cpp
    ../mpd-interface/song.cpp ../mpd-interface/cuefile.cpp
    ../streams/streamfetcher.cpp
    ../network/networkaccessmanager.cpp ../network/networkproxyfactory.cpp
    ../models/streamsmodel.cpp)
set(CANTATA_WEB_MOC_HDRS ${CANTATA_CORE_MOC_HDRS}
    app/application.h app/apihandler.h app/libraryapi.h app/playlistsapi.h app/playqueueapi.h app/streamsapi.h app/statusapi.h
    ../db/librarydb.h ../db/mpdlibrarydb.h
    ../mpd-interface/mpdconnection.h
    ../streams/streamfetcher.h
    ../network/networkaccessmanager.h)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)
configure_file(${PROJECT_NAME}.conf.cmake ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.conf)
add_definitions(-DQT_NO_DEBUG_OUTPUT)
add_subdirectory(../support ${CMAKE_BINARY_DIR}/support)
add_subdirectory(http)

QT5_WRAP_CPP(CANTATA_WEB_MOC_SRCS ${CANTATA_WEB_MOC_HDRS})
ADD_EXECUTABLE(${PROJECT_NAME} ${CANTATA_WEB_SRCS} ${CANTATA_WEB_MOC_SRCS})
target_link_libraries(${PROJECT_NAME} support-core httpserver ${QTLIBS} ${Qt5Sql_LIBRARIES} ${Qt5WebSockets_LIBRARIES} ${SQLITE_LIBRARIES})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
