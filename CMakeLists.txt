PROJECT( cantata )

cmake_minimum_required( VERSION 2.6 )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt4/KDE4 Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "5")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
include(CPack)

SET( CANTATA_SRCS
    main.cpp
    gui/mainwindow.cpp
    gui/preferencesdialog.cpp
    gui/settings.cpp
    gui/covers.cpp
    gui/interfacesettings.cpp
    gui/externalsettings.cpp
    gui/playbacksettings.cpp
    gui/serversettings.cpp
    gui/httpserversettings.cpp
    gui/streamspage.cpp
    gui/streamdialog.cpp
    gui/streamcategorydialog.cpp
    gui/librarypage.cpp
    gui/albumspage.cpp
    gui/folderpage.cpp
    gui/infopage.cpp
    gui/serverinfopage.cpp
    gui/playlistspage.cpp
    gui/tageditor.cpp
    models/musiclibraryitemroot.cpp
    models/musiclibraryitemartist.cpp
    models/musiclibraryitemalbum.cpp
    models/musiclibrarymodel.cpp
    models/musiclibraryproxymodel.cpp
    models/playlistsmodel.cpp
    models/playlistsproxymodel.cpp
    models/playqueuemodel.cpp
    models/playqueueproxymodel.cpp
    models/dirviewmodel.cpp
    models/dirviewproxymodel.cpp
    models/dirviewitem.cpp
    models/dirviewitemdir.cpp
    models/streamsmodel.cpp
    models/streamsproxymodel.cpp
    models/albumsmodel.cpp
    models/albumsproxymodel.cpp
    models/streamfetcher.cpp
    mpd/mpdconnection.cpp
    mpd/mpdparseutils.cpp
    mpd/mpdstatus.cpp
    mpd/song.cpp
    widgets/treeview.cpp
    widgets/fancytabwidget.cpp
    widgets/listview.cpp
    widgets/itemview.cpp
    widgets/autohidingsplitter.cpp
    widgets/timeslider.cpp
    widgets/actionlabel.cpp
    widgets/playqueueview.cpp
    widgets/messagewidget.cpp
    lyrics/lyricspage.cpp
    lyrics/lyricsettings.cpp
    lyrics/ultimatelyricsprovider.cpp
    lyrics/ultimatelyricsreader.cpp
    lyrics/songinfoprovider.cpp
    network/networkaccessmanager.cpp
    network/network.cpp
    dbus/mpris.cpp
    dbus/dockmanager.cpp
    libmaia/maiaObject.cpp
    libmaia/maiaFault.cpp
    libmaia/maiaXmlRpcClient.cpp
    devices/utils.cpp
    http/httpserver.cpp
    http/httpsocket.cpp
)

SET( CANTATA_MOC_HDRS
    gui/mainwindow.h
    gui/settings.h
    gui/covers.h
    gui/folderpage.h
    gui/librarypage.h
    gui/albumspage.h
    gui/playlistspage.h
    gui/streamspage.h
    gui/infopage.h
    gui/serverinfopage.h
    gui/streamdialog.h
    gui/streamcategorydialog.h
    gui/playbacksettings.h
    gui/preferencesdialog.h
    gui/interfacesettings.h
    gui/externalsettings.h
    gui/tageditor.h
    models/musiclibrarymodel.h
    models/musiclibraryproxymodel.h
    models/playlistsmodel.h
    models/playlistsproxymodel.h
    models/playqueuemodel.h
    models/playqueueproxymodel.h
    models/dirviewmodel.h
    models/dirviewproxymodel.h
    models/albumsmodel.h
    models/streamfetcher.h
    models/streamsmodel.h
    mpd/mpdconnection.h
    mpd/mpdstatus.h
    widgets/fancytabwidget.h
    widgets/treeview.h
    widgets/listview.h
    widgets/itemview.h
    widgets/autohidingsplitter.h
    widgets/timeslider.h
    widgets/actionlabel.h
    widgets/playqueueview.h
    widgets/messagewidget.h
    lyrics/lyricspage.h
    lyrics/lyricsettings.h
    lyrics/ultimatelyricsprovider.h
    lyrics/songinfoprovider.h
    network/networkaccessmanager.h
    network/network.h
    dbus/mpris.h
    dbus/dockmanager.h
    libmaia/maiaObject.h
    libmaia/maiaFault.h
    libmaia/maiaXmlRpcClient.h
    http/httpsocket.h
)

SET( CANTATA_UIS
    gui/mainwindow.ui
    gui/folderpage.ui
    gui/librarypage.ui
    gui/albumspage.ui
    gui/playlistspage.ui
    gui/streamspage.ui
    gui/serverinfopage.ui
    gui/interfacesettings.ui
    gui/externalsettings.ui
    gui/playbacksettings.ui
    gui/serversettings.ui
    gui/httpserversettings.ui
    gui/tageditor.ui
    lyrics/lyricspage.ui
    lyrics/lyricsettings.ui
    widgets/itemview.ui
)

SET( CANTATA_RCS cantata.qrc )

include_directories( ${CMAKE_SOURCE_DIR}/libmaia
                     ${CMAKE_SOURCE_DIR}/gui
                     ${CMAKE_SOURCE_DIR}/mpd
                     ${CMAKE_SOURCE_DIR}/models
                     ${CMAKE_SOURCE_DIR}/widgets
                     ${CMAKE_SOURCE_DIR}/lyrics
                     ${CMAKE_SOURCE_DIR}/network
                     ${CMAKE_SOURCE_DIR}/dbus
                     ${CMAKE_SOURCE_DIR}/http
                     ${CMAKE_SOURCE_DIR}/devices
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
)

SET( QT_USE_QTNETWORK TRUE )
SET( QT_USE_QTXML TRUE )
SET( QT_USE_QTDBUS TRUE )

# this command finds Qt4 libraries and sets all required variables
FIND_PACKAGE( Qt4 REQUIRED QtCore QtGui QtXml QtNetwork )

qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockItem.xml )
qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockManager.xml)

IF( NOT DEFINED WANT_KDE_SUPPORT )
    SET( WANT_KDE_SUPPORT ON )
ENDIF( NOT DEFINED WANT_KDE_SUPPORT )

IF( WANT_KDE_SUPPORT )
    FIND_PACKAGE( KDE4 )
    IF( KDE4_FOUND )
        ADD_DEFINITIONS( -DENABLE_KDE_SUPPORT )
        SET( ENABLE_KDE_SUPPORT TRUE )
    ENDIF( KDE4_FOUND )
ENDIF( WANT_KDE_SUPPORT )

IF( NOT DEFINED ENABLE_WEBKIT )
    SET( ENABLE_WEBKIT TRUE )
ENDIF( NOT DEFINED ENABLE_WEBKIT )

set(TAGLIB_MIN_VERSION "1.6")
find_package(Taglib REQUIRED)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDES}")
set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")

check_cxx_source_compiles("#include <asftag.h>
int main() { TagLib::ASF::Tag tag; return 0;}" TAGLIB_ASF_FOUND)
if( NOT TAGLIB_ASF_FOUND )
#    message(FATAL_ERROR "TagLib does not have ASF support compiled in.")
    message("TagLib does not have ASF support compiled in.")
endif( NOT TAGLIB_ASF_FOUND )

check_cxx_source_compiles("#include <mp4tag.h>
int main() { TagLib::MP4::Tag tag(0, 0); return 0;}" TAGLIB_MP4_FOUND)
if( NOT TAGLIB_MP4_FOUND )
#    message(FATAL_ERROR "TagLib does not have MP4 support compiled in.")
    message("TagLib does not have MP4 support compiled in.")
endif( NOT TAGLIB_MP4_FOUND )

set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)

set(TAGLIB-EXTRAS_MIN_VERSION "1.0")
find_package(Taglib-Extras)
set(TAGLIB_EXTRAS_FOUND ${TAGLIB-EXTRAS_FOUND}) # we need a c-compatible name for the include file

include(CheckTagLibFileName)

check_taglib_filename(COMPLEX_TAGLIB_FILENAME)

include_directories(${TAGLIB_INCLUDES})
add_definitions(${TAGLIB_CFLAGS})
if (TAGLIB-EXTRAS_FOUND)
    include_directories(${TAGLIB-EXTRAS_INCLUDES})
    add_definitions(${TAGLIB-EXTRAS_CFLAGS})
endif (TAGLIB-EXTRAS_FOUND)

SET( CANTATA_SRCS ${CANTATA_SRCS} devices/tags.cpp )

IF( ENABLE_KDE_SUPPORT )
    find_package(FFMPEG)
    find_package(MPG123)
    if(FFMPEG_FOUND AND MPG123_FOUND)
        set (ENABLE_REPLAYGAIN_SUPPORT 1)
        find_package(SPEEXDSP)
        set(EBUR128_USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ffmpeginput.cpp replaygain/mpg123input.cpp)
        include_directories(${CMAKE_SOURCE_DIR}/replaygain )
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ebur128.c replaygain/scanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp)
        SET(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} replaygain/scanner.h replaygain/rgdialog.h replaygain/tagreader.h)
    endif(FFMPEG_FOUND AND MPG123_FOUND)
endif( ENABLE_KDE_SUPPORT )

IF( ENABLE_KDE_SUPPORT )
    set (ENABLE_TAG_EDITOR_SUPPORT 1)
    set (ENABLE_DEVICES_SUPPORT 1)
    find_package(Mtp)

    SET( CANTATA_SRCS ${CANTATA_SRCS} devices/mediadevicecache.cpp devices/devicespage.cpp
            devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp
            models/devicesmodel.cpp devices/filetyperesolver.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
            devices/devicepropertiesdialog.cpp devices/filenameschemedialog.cpp devices/encoders.cpp
            devices/transcodingjob.cpp devices/valueslider.cpp devices/trackorganiser.cpp)
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mediadevicecache.h devices/devicespage.h
            devices/device.h devices/fsdevice.h devices/umsdevice.h models/devicesmodel.h
            devices/actiondialog.h devices/devicepropertieswidget.h devices/devicepropertiesdialog.h
            devices/filenameschemedialog.h devices/transcodingjob.h devices/valueslider.h devices/trackorganiser.h)
    SET( CANTATA_UIS ${CANTATA_UIS} devices/devicespage.ui devices/actiondialog.ui devices/devicepropertieswidget.ui
            devices/filenameschemedialog.ui devices/trackorganiser.ui)

    if (ENABLE_REMOTE_DEVICES)
        SET(CANTATA_SRCS ${CANTATA_SRCS} devices/remotedevice.cpp devices/remotedevicepropertiesdialog.cpp
            devices/remotedevicepropertieswidget.cpp)
        SET(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/remotedevice.h devices/remotedevicepropertiesdialog.h
            devices/remotedevicepropertieswidget.h)
        SET(CANTATA_UIS ${CANTATA_UIS} devices/remotedevicepropertieswidget.ui)
    endif (ENABLE_REMOTE_DEVICES)
    if (MTP_FOUND)
        SET( CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp )
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mtpdevice.h )
    endif (MTP_FOUND)

    macro_ensure_version("4.7.0" ${KDE_VERSION} KDE_VERSION_47)

    if (NOT KDE_VERSION_47)
        SET( CANTATA_SRCS ${CANTATA_SRCS} widgets/kmessagewidget.cpp )
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} widgets/kmessagewidget.h )
    endif (NOT KDE_VERSION_47)

    include_directories( ${KDE4_INCLUDES})
    add_definitions( ${KDE4_DEFINITIONS} )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    KDE4_ADD_UI_FILES( CANTATA_UI_HDRS ${CANTATA_UIS} )
    KDE4_ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} )
    TARGET_LINK_LIBRARIES( cantata ${KDE4_KDEWEBKIT_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS}
                           ${QT_QTWEBKIT_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTDBUS_LIBRARY} )
    if (MTP_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${MTP_LIBRARIES})
        include_directories( ${MTP_INCLUDE_DIR})
    endif (MTP_FOUND)
    install( TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS} )
    install( FILES cantataui.rc DESTINATION ${DATA_INSTALL_DIR}/cantata )
    install( FILES cantata.notifyrc DESTINATION ${DATA_INSTALL_DIR}/cantata )

    if (NOT CANTATA_TRANSLATIONS)
        set(CANTATA_TRANSLATIONS "all")
    endif (NOT CANTATA_TRANSLATIONS)

    foreach(trans ${CANTATA_TRANSLATIONS})
        if (trans STREQUAL "all")
            set(CANTATA_CS_TRANSLATION 1)
            set(CANTATA_DE_TRANSLATION 1)
            set(CANTATA_EN_TRANSLATION 1)
            set(CANTATA_ES_TRANSLATION 1)
            set(CANTATA_PL_TRANSLATION 1)
        elseif (trans STREQUAL "cs")
            set(CANTATA_CS_TRANSLATION 1)
        elseif (trans STREQUAL "de")
            set(CANTATA_DE_TRANSLATION 1)
        elseif (trans STREQUAL "en")
            set(CANTATA_EN_TRANSLATION 1)
        elseif (trans STREQUAL "es")
            set(CANTATA_ES_TRANSLATION 1)
        elseif (trans STREQUAL "pl")
            set(CANTATA_PL_TRANSLATION 1)
        endif (trans STREQUAL "all")
    endforeach(trans ${CANTATA_TRANSLATIONS})

    add_subdirectory(po)
ELSE( ENABLE_KDE_SUPPORT )
    SET( CANTATA_SRCS ${CANTATA_SRCS} widgets/dirrequester.cpp widgets/lineedit.cpp network/networkproxyfactory.cpp network/proxysettings.cpp widgets/kmessagewidget.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} widgets/dirrequester.h widgets/lineedit.h network/proxysettings.h widgets/kmessagewidget.h )
    SET( CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui )
    SET( CANTATA_RCS ${CANTATA_RCS} cantata_qt.qrc )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    QT4_WRAP_UI( CANTATA_UI_HDRS ${CANTATA_UIS} )
    INCLUDE( ${QT_USE_FILE} )
    QT4_WRAP_CPP( CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS} )
    ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} )
    TARGET_LINK_LIBRARIES( cantata ${QT_QTWEBKIT_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTDBUS_LIBRARY} )
    install( TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
    ADD_DEFINITIONS( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
    SET( XDG_APPS_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/applications" )
ENDIF( ENABLE_KDE_SUPPORT )

ADD_SUBDIRECTORY( icons )

TARGET_LINK_LIBRARIES(cantata ${TAGLIB_LIBRARIES})
include_directories( ${TAGLIB_INCLUDES})
if (TAGLIB-EXTRAS_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${TAGLIB-EXTRAS_LIBRARIES})
    include_directories( ${TAGLIB-EXTRAS_INCLUDES})
endif (TAGLIB-EXTRAS_FOUND)

if(FFMPEG_FOUND AND MPG123_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${KDE4_THREADWEAVER_LIBRARIES} ${FFMPEG_LIBRARIES} ${MPG123_LIBRARIES})
    include_directories(${FFMPEG_INCLUDE_DIRS} ${MPG123_INCLUDE_DIRS})
    if(SPEEXDSP_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${SPEEXDSP_LIBRARIES})
        include_directories( ${SPEEXDSP_INCLUDE_DIRS})
    endif(SPEEXDSP_FOUND)
endif(FFMPEG_FOUND AND MPG123_FOUND)

if (ENABLE_WEBKIT)
    SET( CANTATA_SRCS ${CANTATA_SRCS} gui/infopage.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/infopage.h )
    ADD_DEFINITIONS( -DENABLE_WEBKIT )
    IF( ENABLE_KDE_SUPPORT )
        TARGET_LINK_LIBRARIES( cantata ${KDE4_KDEWEBKIT_LIBRARY} ${QT_QTWEBKIT_LIBRARY}  )
    ELSE( ENABLE_KDE_SUPPORT )
        TARGET_LINK_LIBRARIES( cantata ${QT_QTWEBKIT_LIBRARY} )
    ENDIF( ENABLE_KDE_SUPPORT )
endif (ENABLE_WEBKIT)

# enable warnings
ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT )

IF( UNIX AND NOT APPLE )
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
ENDIF( UNIX AND NOT APPLE )

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

message("")
IF( ENABLE_KDE_SUPPORT )
    message("INFO: Building with KDE support")
    message("      Enabled features:")
    message("        - Track organizer")
    message("        - Open files via command line")
    message("        - UMS device sync")
    if (MTP_FOUND)
        message("        - MTP device sync")
    endif (MTP_FOUND)
    if (ENABLE_REMOTE_DEVICES)
        message("        - Remote device sync (EXPERIMENTAL)")
    endif (ENABLE_REMOTE_DEVICES)
    if(FFMPEG_FOUND AND MPG123_FOUND)
        message("        - ReplayGain calculation")
    endif(FFMPEG_FOUND AND MPG123_FOUND)
    if( NOT MTP_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
        message("      Disabled features:")
        if (NOT MTP_FOUND)
            message("        - MTP device sync (libmtp required)")
        endif (NOT MTP_FOUND)
        #if (NOT ENABLE_REMOTE_DEVICES)
        #    message("        - Remote device sync (EXPERIMENTAL)")
        #endif (NOT ENABLE_REMOTE_DEVICES)
        if(NOT FFMPEG_FOUND OR NOT MPG123_FOUND)
            message("        - ReplayGain calculation (ffmpeg AND mpg123 required)")
        endif(NOT FFMPEG_FOUND OR NOT MPG123_FOUND)
    endif( NOT MTP_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
ELSE( ENABLE_KDE_SUPPORT )
    message("INFO: Building WITHOUT KDE support - Device Sync, Track Organizer, ReplayGain, and CommandLine support DISABLED")
ENDIF( ENABLE_KDE_SUPPORT )
message("")
