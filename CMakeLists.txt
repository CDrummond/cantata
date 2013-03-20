PROJECT( cantata )

cmake_minimum_required( VERSION 2.6 )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_SOURCE_IGNORE_FILES ${CPACK_SOURCE_IGNORE_FILES} "/.svn/")
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt4/KDE4 Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "9")
set(CPACK_PACKAGE_VERSION_PATCH "50")
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
include(CPack)

OPTION(ENABLE_TAGLIB "Enable TagLib library (required for tag editing, replaygain calculation, device support, etc)" ON)
OPTION(ENABLE_TAGLIB_EXTRAS "Enable TagLib-Extras library (used by tag editing, replaygain calculation, device support, etc)" ON)
OPTION(ENABLE_PHONON "Enable Phonon library (required to playback MPD HTTP streams)" OFF)
OPTION(ENABLE_KDE "Enable KDE libraries" ON)
OPTION(ENABLE_QT5 "Build against Qt5" OFF)
OPTION(ENABLE_MTP "Enable MTP library (required to support MTP devices)" ON)
OPTION(ENABLE_FFMPEG "Enable ffmpeg/libav libraries (required for replaygain calculation)" ON)
OPTION(ENABLE_MPG123 "Enable mpg123 libraries (required for replaygain calculation)" ON)
OPTION(ENABLE_SPEEXDSP "Enable SpeexDSP libraries (used to speed-up replaygain calculation)" ON)
OPTION(ENABLE_UDISKS2 "Build UDisks2 backend, and NOT UDisks, for Qt builds" OFF)
OPTION(ENABLE_OVERLAYSCROLLBARS "Wnable support for overlay style scrollbars when using QGtkStyle (Linux only)" OFF)

if (ENABLE_QT5)
    set(ENABLE_PHONON FALSE)
    set(ENABLE_KDE FALSE)
endif (ENABLE_QT5)

if (ENABLE_UDISKS2)
    set (WITH_SOLID_UDISKS2 ON)
endif (ENABLE_UDISKS2)

macro(cantata_check_for_sse)
    # check for SSE extensions
    include(CheckCXXSourceRuns)
    if( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX )
        set(CMAKE_REQUIRED_FLAGS "-msse3")
        check_cxx_source_runs("
            #include <pmmintrin.h>
            int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals); b = _mm_hadd_pd(a,a); _mm_storeu_pd(vals, b); return 0; }"
            HAS_SSE3_EXTENSIONS)
        if(HAS_SSE3_EXTENSIONS)
            add_definitions("-msse3 -mfpmath=sse")
        else(HAS_SSE3_EXTENSIONS)
            # No sse3, check sse2
            set(CMAKE_REQUIRED_FLAGS "-msse2")
            check_cxx_source_runs("
                #include <emmintrin.h>
                int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals); b = _mm_add_pd(a,a);  _mm_storeu_pd(vals,b); return 0; }"
                HAS_SSE2_EXTENSIONS)
            if(HAS_SSE2_EXTENSIONS)
                add_definitions("-msse2 -mfpmath=sse")
            else(HAS_SSE2_EXTENSIONS)
                # No sse2, check sse
                set(CMAKE_REQUIRED_FLAGS "-msse")
                check_cxx_source_runs("
                    #include <xmmintrin.h>
                    int main() { __m128 a, b; float vals[4] = {0}; a = _mm_loadu_ps(vals); b = a; b = _mm_add_ps(a,b); _mm_storeu_ps(vals,b); return 0; }"
                    HAS_SSE_EXTENSIONS)
                if(HAS_SSE_EXTENSIONS)
                    add_definitions("-msse -mfpmath=sse")
                endif(HAS_SSE_EXTENSIONS)
            endif(HAS_SSE2_EXTENSIONS)
        endif(HAS_SSE3_EXTENSIONS)
        set(CMAKE_REQUIRED_FLAGS)
    elseif(MSVC)
        check_cxx_source_runs("
            #include <emmintrin.h>
            int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals);  b = _mm_add_pd(a,a);  _mm_storeu_pd(vals,b); return 0; }"
            HAS_SSE2_EXTENSIONS)
    endif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
endmacro(cantata_check_for_sse)

SET( CANTATA_SRCS
    gui/application.cpp
    gui/main.cpp
    gui/initialsettingswizard.cpp
    gui/mainwindow.cpp
    gui/preferencesdialog.cpp
    gui/settings.cpp
    gui/covers.cpp
    gui/filesettings.cpp
    gui/interfacesettings.cpp
    gui/playbacksettings.cpp
    gui/serverplaybacksettings.cpp
    gui/serversettings.cpp
    gui/librarypage.cpp
    gui/albumspage.cpp
    gui/folderpage.cpp
    gui/playlistspage.cpp
    gui/infopage.cpp
    gui/trayitem.cpp
    gui/cachesettings.cpp
    gui/coverdialog.cpp
    gui/stdactions.cpp
    streams/streamspage.cpp
    streams/streamdialog.cpp
    streams/streamcategorydialog.cpp
    streams/streamfetcher.cpp
    streams/webstreams.cpp
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
    models/proxymodel.cpp
    models/onlineservicesmodel.cpp
    models/actionmodel.cpp
    mpd/mpdconnection.cpp
    mpd/mpdparseutils.cpp
    mpd/mpdstats.cpp
    mpd/mpdstatus.cpp
    mpd/song.cpp
    dynamic/dynamic.cpp
    dynamic/dynamicpage.cpp
    dynamic/dynamicproxymodel.cpp
    dynamic/dynamicruledialog.cpp
    dynamic/dynamicrulesdialog.cpp
    widgets/treeview.cpp
    widgets/listview.cpp
    widgets/itemview.cpp
    widgets/autohidingsplitter.cpp
    widgets/timeslider.cpp
    widgets/actionlabel.cpp
    widgets/playqueueview.cpp
    widgets/groupedview.cpp
    widgets/actionitemdelegate.cpp
    widgets/textbrowser.cpp
    widgets/coverwidget.cpp
    widgets/volumecontrol.cpp
    widgets/icons.cpp
    widgets/genrecombo.cpp
    widgets/toolbar.cpp
    widgets/menubutton.cpp
    widgets/toolbutton.cpp
    widgets/wizardpage.cpp
    lyrics/lyricspage.cpp
    lyrics/lyricsettings.cpp
    lyrics/ultimatelyricsprovider.cpp
    lyrics/ultimatelyrics.cpp
    lyrics/lyricsdialog.cpp
    network/networkaccessmanager.cpp
    online/onlineservicespage.cpp
    online/onlineservice.cpp
    online/jamendoservice.cpp
    online/jamendosettingsdialog.cpp
    online/magnatuneservice.cpp
    online/magnatunesettingsdialog.cpp
    online/onlinedevice.cpp
)

SET( CANTATA_MOC_HDRS
    gui/initialsettingswizard.h
    gui/mainwindow.h
    gui/settings.h
    gui/covers.h
    gui/folderpage.h
    gui/librarypage.h
    gui/albumspage.h
    gui/playlistspage.h
    gui/infopage.h
    gui/serverplaybacksettings.h
    gui/serversettings.h
    gui/preferencesdialog.h
    gui/filesettings.h
    gui/interfacesettings.h
    gui/cachesettings.h
    gui/trayitem.h
    gui/coverdialog.h
    streams/streamspage.h
    streams/streamdialog.h
    streams/streamcategorydialog.h
    streams/streamfetcher.h
    streams/webstreams.h
    models/musiclibrarymodel.h
    models/musiclibraryproxymodel.h
    models/playlistsmodel.h
    models/playlistsproxymodel.h
    models/playqueuemodel.h
    models/playqueueproxymodel.h
    models/dirviewmodel.h
    models/dirviewproxymodel.h
    models/albumsmodel.h
    models/streamsmodel.h
    models/onlineservicesmodel.h
    models/actionmodel.h
    mpd/mpdconnection.h
    mpd/mpdstats.h
    mpd/mpdstatus.h
    dynamic/dynamic.h
    dynamic/dynamicpage.h
    dynamic/dynamicruledialog.h
    dynamic/dynamicrulesdialog.h
    widgets/treeview.h
    widgets/listview.h
    widgets/itemview.h
    widgets/autohidingsplitter.h
    widgets/timeslider.h
    widgets/actionlabel.h
    widgets/playqueueview.h
    widgets/groupedview.h
    widgets/actionitemdelegate.h
    widgets/coverwidget.h
    widgets/volumecontrol.h
    widgets/genrecombo.h
    widgets/toolbar.h
    lyrics/lyricspage.h
    lyrics/lyricsettings.h
    lyrics/ultimatelyrics.h
    lyrics/ultimatelyricsprovider.h
    lyrics/lyricsdialog.h
    network/networkaccessmanager.h
    online/onlineservice.h
    online/onlineservicespage.h
    online/jamendoservice.h
    online/magnatuneservice.h
    online/magnatunesettingsdialog.h
    online/onlinedevice.h
)

SET( CANTATA_UIS
    gui/initialsettingswizard.ui
    gui/mainwindow.ui
    gui/folderpage.ui
    gui/librarypage.ui
    gui/albumspage.ui
    gui/playlistspage.ui
    gui/filesettings.ui
    gui/interfacesettings.ui
    gui/playbacksettings.ui
    gui/serverplaybacksettings.ui
    gui/serversettings.ui
    gui/coverdialog.ui
    streams/streamspage.ui
    dynamic/dynamicpage.ui
    dynamic/dynamicrule.ui
    dynamic/dynamicrules.ui
    lyrics/lyricspage.ui
    lyrics/lyricsettings.ui
    widgets/itemview.ui
    online/onlineservicespage.ui
)

find_package( Cdparanoia )
if (CDPARANOIA_FOUND) 
    find_package( CDDB )
    find_package( MusicBrainz5 )
    find_package( Lame )
endif (CDPARANOIA_FOUND)

if (ENABLE_TAGLIB)
    set(TAGLIB_MIN_VERSION "1.6")
    find_package(Taglib)
endif (ENABLE_TAGLIB)

if (ENABLE_OVERLAYSCROLLBARS)
    add_definitions(-DENABLE_OVERLAYSCROLLBARS)
endif (ENABLE_OVERLAYSCROLLBARS)

if (TAGLIB_FOUND)
    SET( CANTATA_SRCS ${CANTATA_SRCS}
        tags/tageditor.cpp
        tags/trackorganiser.cpp
        tags/tags.cpp
        devices/deviceoptions.cpp
        devices/filenameschemedialog.cpp
        http/httpserversettings.cpp
        http/httpserver.cpp
        http/httpsocket.cpp
    )

    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS}
        tags/tageditor.h
        tags/trackorganiser.h
        http/httpserversettings.h
        http/httpsocket.h
        devices/filenameschemedialog.h
        devices/device.h
    )

    SET( CANTATA_UIS ${CANTATA_UIS}
        http/httpserversettings.ui
        tags/tageditor.ui
        tags/trackorganiser.ui
        devices/filenameschemedialog.ui
    )
endif (TAGLIB_FOUND)

if (NOT WIN32)
    SET( CANTATA_SRCS ${CANTATA_SRCS} dbus/mpris.cpp dbus/gnomemediakeys.cpp devices/mountpoints.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} dbus/mpris.h dbus/gnomemediakeys.h devices/mountpoints.h )
    if (NOT ENABLE_KDE_SUPPORT)
        SET( CANTATA_SRCS ${CANTATA_SRCS} dbus/notify.cpp)
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} dbus/notify.h)
    endif (NOT ENABLE_KDE_SUPPORT)
endif(NOT WIN32)

SET( CANTATA_RCS cantata.qrc )

include_directories( ${CMAKE_SOURCE_DIR}/gui
                     ${CMAKE_SOURCE_DIR}/mpd
                     ${CMAKE_SOURCE_DIR}/models
                     ${CMAKE_SOURCE_DIR}/widgets
                     ${CMAKE_SOURCE_DIR}/lyrics
                     ${CMAKE_SOURCE_DIR}/network
                     ${CMAKE_SOURCE_DIR}/dbus
                     ${CMAKE_SOURCE_DIR}/http
                     ${CMAKE_SOURCE_DIR}/devices
                     ${CMAKE_SOURCE_DIR}/dynamic
                     ${CMAKE_SOURCE_DIR}/support
                     ${CMAKE_SOURCE_DIR}/streams
                     ${CMAKE_SOURCE_DIR}/tags
                     ${CMAKE_BINARY_DIR}/support
                     ${CMAKE_SOURCE_DIR}/online
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
)

if (ENABLE_QT5)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5Xml REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(Qt5Concurrent REQUIRED)
    set (QTLIBS ${Qt5Widgets_LIBRARIES} ${Qt5Network_LIBRARIES} ${Qt5Xml_LIBRARIES} ${Qt5Concurrent_LIBRARIES})
    set (QTINCLUDES ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${Qt5Xml_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS})
    add_definitions(${Qt5Widgets_DEFINITIONS} ${Qt5Network_DEFINITIONS} ${Qt5Xml_DEFINITIONS} ${Qt5Concurrent_DEFINITIONS})
    set (CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
    if(NOT WIN32)
        find_package(Qt5DBus REQUIRED)
        set (QTLIBS ${QTLIBS} ${Qt5DBus_LIBRARIES})
        set (QTINCLUDES ${QTINCLUDES} ${Qt5DBus_INCLUDE_DIRS})
        add_definitions(${Qt5DBus_DEFINITIONS})
    endif(NOT WIN32)
else (ENABLE_QT5)
    find_package(Qt4 REQUIRED QtCore QtGui QtXml QtNetwork)
    set (QTLIBS ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTDBUS_LIBRARY} )
    set (QTINCLUDES ${QT_INCLUDES})
    add_definitions( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
endif (ENABLE_QT5)

find_package(ZLIB REQUIRED)

if (ENABLE_PHONON)
    FIND_PACKAGE( Phonon )
endif (ENABLE_PHONON)

if(NOT WIN32)
    if (ENABLE_QT5)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/com.googlecode.cantata.xml gui/mainwindow.h MainWindow)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.MediaKeys.xml)
    else (ENABLE_QT5)
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/com.googlecode.cantata.xml gui/mainwindow.h MainWindow)
        if (NOT ENABLE_KDE_SUPPORT)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
        endif (NOT ENABLE_KDE_SUPPORT)
        qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.MediaKeys.xml)
    endif (ENABLE_QT5)
endif(NOT WIN32)

IF( ENABLE_KDE )
    FIND_PACKAGE( KDE4 )
    IF( KDE4_FOUND )
        ADD_DEFINITIONS( -DENABLE_KDE_SUPPORT )
        SET( ENABLE_KDE_SUPPORT TRUE )
    ENDIF( KDE4_FOUND )
ENDIF( ENABLE_KDE )

if (TAGLIB_FOUND)
    set(ENABLE_TAGEDITOR_SUPPORT 1)
    set(ENABLE_TRACKORGANIZER_SUPPORT 1)
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
    if (ENABLE_TAGLIB_EXTRAS)
        find_package(Taglib-Extras)
    endif (ENABLE_TAGLIB_EXTRAS)
    set(TAGLIB_EXTRAS_FOUND ${TAGLIB-EXTRAS_FOUND}) # we need a c-compatible name for the include file

    include(CheckTagLibFileName)
    check_taglib_filename(COMPLEX_TAGLIB_FILENAME)

    add_definitions(${TAGLIB_CFLAGS})
    if (TAGLIB-EXTRAS_FOUND)
        include_directories(${TAGLIB-EXTRAS_INCLUDES})
        add_definitions(${TAGLIB-EXTRAS_CFLAGS})
    endif (TAGLIB-EXTRAS_FOUND)

    if (ENABLE_FFMPEG)
        find_package(FFMPEG)
    endif (ENABLE_FFMPEG)
    if (ENABLE_MPG123)
        find_package(MPG123)
    endif (ENABLE_MPG123)
    if(FFMPEG_FOUND OR MPG123_FOUND)
        cantata_check_for_sse()
        set (ENABLE_REPLAYGAIN_SUPPORT 1)
        if (ENABLE_SPEEXDSP)
            find_package(SPEEXDSP)
        endif (ENABLE_SPEEXDSP)
        set(USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})
    if(FFMPEG_FOUND)
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ffmpeginput.cpp)
    endif(FFMPEG_FOUND)
    if(MPG123_FOUND)
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/mpg123input.cpp)
    endif(MPG123_FOUND)
        include_directories(${CMAKE_SOURCE_DIR}/replaygain )
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ebur128.c replaygain/scanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp replaygain/jobcontroller.cpp )
        SET(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} replaygain/scanner.h replaygain/rgdialog.h replaygain/tagreader.h replaygain/jobcontroller.h )
    endif(FFMPEG_FOUND OR MPG123_FOUND)

    if (WIN32)
        SET( CANTATA_SRCS ${CANTATA_SRCS} devices/device.cpp tags/filetyperesolver.cpp)
    else (WIN32)
        set (ENABLE_DEVICES_SUPPORT 1)
        if (NOT ENABLE_KDE_SUPPORT)
            add_subdirectory(solid-lite)
        endif (NOT ENABLE_KDE_SUPPORT)
        if (ENABLE_MTP)
            find_package(Mtp)
        endif (ENABLE_MTP)
        if (MTP_FOUND)
            SET( CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp )
            SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mtpdevice.h )
        endif (MTP_FOUND)
        if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
            if (CDDB_FOUND)
                SET( CANTATA_SRCS ${CANTATA_SRCS} devices/cddb.cpp )
                SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/cddb.h )
            endif (CDDB_FOUND)
            if (MUSICBRAINZ5_FOUND)
                SET( CANTATA_SRCS ${CANTATA_SRCS} devices/musicbrainz.cpp )
                SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/musicbrainz.h )
                if (ENABLE_KDE_SUPPORT)
                    add_definitions(${KDE4_ENABLE_EXCEPTIONS})
                endif (ENABLE_KDE_SUPPORT)
            endif (MUSICBRAINZ5_FOUND)
            SET( CANTATA_SRCS ${CANTATA_SRCS} devices/audiocddevice.cpp devices/cddb.cpp devices/cddbselectiondialog.cpp
                 devices/cdparanoia.cpp devices/audiocdsettings.cpp devices/extractjob.cpp devices/albumdetailsdialog.cpp)
            SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/audiocddevice.h devices/cddb.h devices/extractjob.h
                 devices/albumdetailsdialog.h devices/cddbselectiondialog.h)
            SET( CANTATA_UIS ${CANTATA_UIS} devices/albumdetails.ui devices/audiocdsettings.ui)
        endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)

        SET( CANTATA_SRCS ${CANTATA_SRCS} devices/devicespage.cpp devices/filejob.cpp
                devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp
                models/devicesmodel.cpp tags/filetyperesolver.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
                devices/devicepropertiesdialog.cpp devices/encoders.cpp devices/freespaceinfo.cpp
                devices/transcodingjob.cpp devices/valueslider.cpp devices/syncdialog.cpp devices/synccollectionwidget.cpp)
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/devicespage.h devices/filejob.h
                devices/fsdevice.h devices/umsdevice.h models/devicesmodel.h
                devices/actiondialog.h devices/devicepropertieswidget.h devices/devicepropertiesdialog.h
                devices/transcodingjob.h devices/valueslider.h devices/syncdialog.h devices/synccollectionwidget.h)
        SET( CANTATA_UIS ${CANTATA_UIS} devices/devicespage.ui devices/actiondialog.ui devices/devicepropertieswidget.ui
                devices/synccollectionwidget.ui)

        if (ENABLE_REMOTE_DEVICES)
            SET(CANTATA_SRCS ${CANTATA_SRCS} devices/remotefsdevice.cpp devices/remotedevicepropertiesdialog.cpp
                devices/remotedevicepropertieswidget.cpp)
            SET(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/remotefsdevice.h devices/remotedevicepropertiesdialog.h
                devices/remotedevicepropertieswidget.h)
            SET(CANTATA_UIS ${CANTATA_UIS} devices/remotedevicepropertieswidget.ui)
            if (ENABLE_QT5)
                qt5_add_dbus_interfaces(CANTATA_SRCS devices/mounter/com.googlecode.cantata.mounter.xml)
            else (ENABLE_QT5)
                qt4_add_dbus_interfaces(CANTATA_SRCS devices/mounter/com.googlecode.cantata.mounter.xml)
            endif (ENABLE_QT5)
            add_subdirectory(devices/mounter)
            add_subdirectory(devices/avahi)
        endif (ENABLE_REMOTE_DEVICES)
    endif (WIN32)
endif (TAGLIB_FOUND)

add_subdirectory(po)
add_subdirectory(support)

IF( ENABLE_KDE_SUPPORT )
    include_directories( ${KDE4_INCLUDES})
    add_definitions( ${KDE4_DEFINITIONS} )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    KDE4_ADD_UI_FILES( CANTATA_UI_HDRS ${CANTATA_UIS} )
    KDE4_ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} )
    TARGET_LINK_LIBRARIES( cantata ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_SOLID_LIBS} X11)
    if (NOT WIN32 AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
        TARGET_LINK_LIBRARIES(cantata ${KDE4_KFILE_LIBS})
    endif (NOT WIN32 AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
    install( TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS} )
    install( FILES cantataui.rc DESTINATION ${DATA_INSTALL_DIR}/${CMAKE_PROJECT_NAME} )
    install( FILES cantata.notifyrc DESTINATION ${DATA_INSTALL_DIR}/${CMAKE_PROJECT_NAME} )
ELSE( ENABLE_KDE_SUPPORT )
    if (WIN32)
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/application.h )
        IF( MINGW )
            # resource compilation for MinGW
            ADD_CUSTOM_COMMAND( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o
                                COMMAND windres.exe -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/icons/cantataico.rc
                                    -o ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o )
            SET(CANTATA_SRCS ${CANTATA_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
        ELSE( MINGW )
            SET(CANTATA_SRCS ${CANTATA_SRCS} icons/cantataico.rc)
        ENDIF( MINGW )
    endif (WIN32)

    SET( CANTATA_SRCS ${CANTATA_SRCS} network/networkproxyfactory.cpp network/proxysettings.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} network/proxysettings.h )
    SET( CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui )
    SET( CANTATA_RCS ${CANTATA_RCS} cantata_qt.qrc )
    if (ENABLE_QT5)
        QT5_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
        QT5_WRAP_UI( CANTATA_UI_HDRS ${CANTATA_UIS} )
        QT5_WRAP_CPP( CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS} )
    else (ENABLE_QT5)
        QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
        QT4_WRAP_UI( CANTATA_UI_HDRS ${CANTATA_UIS} )
        INCLUDE( ${QT_USE_FILE} )
        QT4_WRAP_CPP( CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS} )
    endif (ENABLE_QT5)

    if (WIN32)
        SET(CMAKE_BUILD_TYPE "Release")
        ADD_EXECUTABLE( cantata WIN32 ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
    else (WIN32)
        ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
        TARGET_LINK_LIBRARIES(cantata X11)
    endif (WIN32)

    install( TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
    SET( XDG_APPS_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/applications" )

    if (WIN32)
        add_subdirectory(qtsingleapplication)
        TARGET_LINK_LIBRARIES(cantata qtsingleapplication)
        include_directories(${CMAKE_BINARY_DIR}/qtsingleapplication)
    endif (WIN32)
ENDIF( ENABLE_KDE_SUPPORT )

if (NOT WIN32)
    install(PROGRAMS dynamic/cantata-dynamic DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/)
    if (MTP_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${MTP_LIBRARIES})
        include_directories(${MTP_INCLUDE_DIR})
    endif (MTP_FOUND)

    if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${CDPARANOIA_LIBRARIES})
        include_directories(${CDPARANOIA_INCLUDE_DIR})
        if (CDDB_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${CDDB_LIBS})
            include_directories(${CDDB_INCLUDE_DIR})
        endif (CDDB_FOUND)
        if (MUSICBRAINZ5_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${MUSICBRAINZ5_LIBRARIES})
            include_directories(${MUSICBRAINZ5_INCLUDE_DIR})
        endif (MUSICBRAINZ5_FOUND)
        if (LAME_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${LAME_LIBS})
            include_directories(${LAME_INCLUDE_DIR})
        endif (LAME_FOUND)
    endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
endif (NOT WIN32)

add_subdirectory(qtiocompressor)
add_subdirectory(qjson)
TARGET_LINK_LIBRARIES(cantata qtiocompressor qjson)
include_directories(${CMAKE_BINARY_DIR}/qtiocompressor)

ADD_SUBDIRECTORY( icons )

target_link_libraries(cantata support ${QTLIBS} ${ZLIB_LIBRARIES})
include_directories(${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})

if (TAGLIB_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${TAGLIB_LIBRARIES})
    include_directories(${TAGLIB_INCLUDES})
    if (TAGLIB-EXTRAS_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${TAGLIB-EXTRAS_LIBRARIES})
        include_directories( ${TAGLIB-EXTRAS_INCLUDES})
    endif (TAGLIB-EXTRAS_FOUND)

    if(FFMPEG_FOUND OR MPG123_FOUND)
        if(FFMPEG_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${FFMPEG_LIBRARIES})
            include_directories(${FFMPEG_INCLUDE_DIRS})
        endif(FFMPEG_FOUND)
        if(MPG123_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${MPG123_LIBRARIES})
            include_directories(${MPG123_INCLUDE_DIRS})
        endif(MPG123_FOUND)
        if(SPEEXDSP_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${SPEEXDSP_LIBRARIES})
            include_directories( ${SPEEXDSP_INCLUDE_DIRS})
        endif(SPEEXDSP_FOUND)
    endif(FFMPEG_FOUND OR MPG123_FOUND)
    if (NOT ENABLE_KDE_SUPPORT AND NOT WIN32)
        TARGET_LINK_LIBRARIES(cantata solidlite)
    endif (NOT ENABLE_KDE_SUPPORT AND NOT WIN32)
    if (ENABLE_REMOTE_DEVICES AND NOT WIN32)
        TARGET_LINK_LIBRARIES(cantata avahi)
    endif (ENABLE_REMOTE_DEVICES AND NOT WIN32)
endif (TAGLIB_FOUND)

if (PHONON_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${PHONON_LIBS})
    include_directories( ${PHONON_INCLUDES})
endif (PHONON_FOUND)

# enable warnings
ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT )

IF( UNIX AND NOT APPLE)
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
ENDIF( UNIX AND NOT APPLE)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

message("")
IF( ENABLE_KDE_SUPPORT )
    message("INFO: Building KDE version")
else ( ENABLE_KDE_SUPPORT )
    if (NOT WIN32)
        if (ENABLE_QT5)
            message("INFO: Building Qt5 version")
        else (ENABLE_QT5)
            message("INFO: Building Qt4 version")
        endif (ENABLE_QT5)
    endif (NOT WIN32)
endif ( ENABLE_KDE_SUPPORT )

if (TAGLIB_FOUND)
    if (ENABLE_REMOTE_DEVICES OR PHONON_FOUND)
        message("      Extra enabled features:")
        if (ENABLE_REMOTE_DEVICES)
            message("        - Remote device sync (EXPERIMENTAL)")
        endif (ENABLE_REMOTE_DEVICES)
        if (PHONON_FOUND)
            message("        - MPD HTTP stream playback")
        endif (PHONON_FOUND)
    endif (ENABLE_REMOTE_DEVICES OR PHONON_FOUND)
    if (NOT TAGLIB_FOUND OR NOT MTP_FOUND OR (NOT CDDB_FOUND AND NOT MUSICBRAINZ5_FOUND) OR (NOT FFMPEG_FOUND AND NOT MPG123_FOUND) OR NOT PHONON_FOUND OR NOT ENABLE_REMOTE_DEVICES)
        message("      Disabled features:")
        if (NOT PHONON_FOUND)
            message("        - MPD HTTP stream playback (phonon required)")
        endif (NOT PHONON_FOUND)
        if (NOT TAGLIB_FOUND)
            message("        - Tag Editor (taglib required)")
            message("        - Track Organizer (taglib required)")
            message("        - Commandline support (taglib required)")
            message("        - HTTP server support (taglib required)")
            if (NOT WIN32)
                message("        - UMS device sync (taglib required)")
            endif (NOT WIN32)
        endif (NOT TAGLIB_FOUND)
        if (NOT MTP_FOUND AND NOT WIN32)
            message("        - MTP device sync (taglib and libmtp required)")
        endif (NOT MTP_FOUND AND NOT WIN32)
        if (NOT CDDB_FOUND AND NOT MUSICBRAINZ5_FOUND AND NOT WIN32)
            message("        - AudioCD (libcdparanoia and libcddb/libmusicbrainz5 required)")
        endif (NOT CDDB_FOUND AND NOT MUSICBRAINZ5_FOUND AND NOT WIN32)
        if (NOT ENABLE_REMOTE_DEVICES)
            message("        - Remote device sync (EXPERIMENTAL)")
        endif (NOT ENABLE_REMOTE_DEVICES)
        if(NOT FFMPEG_FOUND AND NOT MPG123_FOUND)
            message("        - ReplayGain calculation (taglib, and ffmpeg and/or mpg123 required)")
        endif(NOT FFMPEG_FOUND AND NOT MPG123_FOUND)
    endif (NOT TAGLIB_FOUND OR NOT MTP_FOUND OR (NOT CDDB_FOUND AND NOT MUSICBRAINZ5_FOUND) OR (NOT FFMPEG_FOUND AND NOT MPG123_FOUND) OR NOT PHONON_FOUND OR NOT ENABLE_REMOTE_DEVICES)
endif (TAGLIB_FOUND)

if (NOT CMAKE_SYSTEM_NAME MATCHES Linux)
    message("")
    message("*************************************************************")
    message("*************************************************************")
    message("** NOTE: You NEED to have the Oxygen icons installed!!     **")
    message("**       If not, Cantata may have *lots* of missing icons. **")
    message("*************************************************************")
    message("*************************************************************")
    message("")
endif (NOT CMAKE_SYSTEM_NAME MATCHES Linux)
message("")
