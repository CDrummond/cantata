include_directories(${QTINCLUDES})
include_directories(${CMAKE_SOURCE_DIR}/tags)
set(CANTATA_TAGS_SRCS ${CANTATA_TAGS_SRCS} main.cpp tagserver.cpp tags.cpp filetyperesolver.cpp ../mpd/song.cpp)
set(CANTATA_TAGS_MOC_HDRS tagserver.h)

if (ENABLE_KDE_SUPPORT)
    kde4_add_executable(cantata-tags ${CANTATA_TAGS_SRCS} ${CANTATA_TAGS_MOC_SRCS})
    install(TARGETS cantata-tags RUNTIME DESTINATION lib/cantata)
else (ENABLE_KDE_SUPPORT)
    if (ENABLE_QT5)
        QT5_WRAP_CPP(CANTATA_TAGS_MOC_SRCS ${CANTATA_TAGS_MOC_HDRS})
    else (ENABLE_QT5)
        INCLUDE(${QT_USE_FILE})
        QT4_WRAP_CPP(CANTATA_TAGS_MOC_SRCS ${CANTATA_TAGS_MOC_HDRS})
    endif (ENABLE_QT5)

    if (WIN32)
        set(CMAKE_BUILD_TYPE "Release")
        ADD_EXECUTABLE(cantata-tags WIN32 ${CANTATA_TAGS_SRCS} ${CANTATA_TAGS_MOC_SRCS}})
        install(TARGETS cantata-tags DESTINATION ${CMAKE_INSTALL_PREFIX})
    else (WIN32)
        ADD_EXECUTABLE(cantata-tags ${CANTATA_TAGS_SRCS} ${CANTATA_TAGS_MOC_SRCS})
        install(TARGETS cantata-tags RUNTIME DESTINATION lib/cantata)
    endif (WIN32)
endif (ENABLE_KDE_SUPPORT)

add_definitions(-DCANTATA_NO_SONG_TIME_FUNCTION)
include_directories(${TAGLIB_INCLUDES})
target_link_libraries(cantata-tags ${TAGLIB_LIBRARIES} ${QTCORELIBS} ${QTNETWORKLIBS})
if (TAGLIB-EXTRAS_FOUND)
    target_link_libraries(cantata-tags ${TAGLIB-EXTRAS_LIBRARIES})
    include_directories(${TAGLIB-EXTRAS_INCLUDES})
endif (TAGLIB-EXTRAS_FOUND)





