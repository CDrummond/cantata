PROJECT(cantata)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TBZ2")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt4/KDE4 Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "3")
set(CPACK_PACKAGE_VERSION_SPIN "")
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}${CPACK_PACKAGE_VERSION_SPIN}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}${CPACK_PACKAGE_VERSION_SPIN}")
set(CPACK_SOURCE_IGNORE_FILES  "/.svn/;CMakeLists.txt.user")
include(CPack)

option(ENABLE_TAGLIB "Enable TagLib library (required for tag editing, replaygain calculation, device support, etc)" ON)
option(ENABLE_TAGLIB_EXTRAS "Enable TagLib-Extras library (used by tag editing, replaygain calculation, device support, etc)" ON)
option(ENABLE_HTTP_STREAM_PLAYBACK "Enable playback of MPD HTTP streams (Phonon required for Qt4, QtMultimedia for Qt5)" OFF)
if (WIN32)
    option(ENABLE_HTTPS_SUPPORT "Enable HTTPS URLs for Last.fm, Wikipedia, SoundCloud, etc. (Requires a Qt built with SSL support)" OFF)
else (WIN32)
    option(ENABLE_HTTPS_SUPPORT "Enable HTTPS URLs for Last.fm, Wikipedia, SoundCloud, etc." ON)
endif (WIN32)
option(ENABLE_KDE "Enable KDE libraries" ON)
option(ENABLE_KWALLET "Use KWallet in KDE builds" ON)
option(ENABLE_QT5 "Build against Qt5" OFF)
option(ENABLE_MTP "Enable MTP library (required to support MTP devices)" ON)
option(ENABLE_FFMPEG "Enable ffmpeg/libav libraries (required for replaygain calculation)" ON)
option(ENABLE_MPG123 "Enable mpg123 libraries (required for replaygain calculation)" ON)
option(ENABLE_SPEEXDSP "Enable SpeexDSP libraries (used to speed-up replaygain calculation)" ON)
option(ENABLE_UDISKS2 "Build UDisks2 backend, and NOT UDisks, for Qt builds" OFF)
option(ENABLE_OVERLAYSCROLLBARS "Enable support for overlay style scrollbars when using QGtkStyle (Linux only - EXPERIMENTAL - NOT FULLY WORKING!)" OFF)
option(ENABLE_CDPARANOIA "Enable CDParanoia libraries (required for AudioCD support)" ON)
option(ENABLE_CDDB "Enable CDDB libraries (either this or MusicBrianz required for AudioCD support)" ON)
option(ENABLE_MUSICBRAINZ "Enable MusicBrianz libraries (either this or CDDB required for AudioCD support)" ON)
option(ENABLE_PROXY_CONFIG "Enable proxy config in settings dialog" OFF)
option(ENABLE_EXTERNAL_TAGS "Enable usage of external app for reading/writing tags (EXPERIMENTAL - NOT FULLY WORKING)" OFF)
option(ENABLE_UNCACHED_MTP "Open MTP devices in un-cached mode (faster)" ON)

if (ENABLE_QT5)
    set(ENABLE_KDE FALSE)
endif (ENABLE_QT5)

if (ENABLE_UDISKS2)
    set(WITH_SOLID_UDISKS2 ON)
endif (ENABLE_UDISKS2)

if (ENABLE_TAGLIB)
    set(TAGLIB_MIN_VERSION "1.6")
    find_package(Taglib)
endif (ENABLE_TAGLIB)

if (TAGLIB_FOUND)
    if (ENABLE_CDPARANOIA)
        find_package(Cdparanoia)
        if (CDPARANOIA_FOUND)
            if (ENABLE_CDDB)
                find_package(CDDB)
            endif (ENABLE_CDDB)
            if (ENABLE_MUSICBRAINZ)
                find_package(MusicBrainz5)
            endif (ENABLE_MUSICBRAINZ)
        endif (CDPARANOIA_FOUND)
    endif (ENABLE_CDPARANOIA)

    set(ENABLE_TAGEDITOR_SUPPORT 1)
    set(ENABLE_TRACKORGANIZER_SUPPORT 1)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDES}")
    set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")

    check_cxx_source_compiles("#include <asftag.h>
                            int main() { TagLib::ASF::Tag tag; return 0;}" TAGLIB_ASF_FOUND)
    if (NOT TAGLIB_ASF_FOUND)
    #    message(FATAL_ERROR "TagLib does not have ASF support compiled in.")
        message("TagLib does not have ASF support compiled in.")
    endif (NOT TAGLIB_ASF_FOUND)

    check_cxx_source_compiles("#include <mp4tag.h>
                            int main() { TagLib::MP4::Tag tag(0, 0); return 0;}" TAGLIB_MP4_FOUND)
    if (NOT TAGLIB_MP4_FOUND)
    #    message(FATAL_ERROR "TagLib does not have MP4 support compiled in.")
        message("TagLib does not have MP4 support compiled in.")
    endif (NOT TAGLIB_MP4_FOUND)

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
    if (ENABLE_SPEEXDSP AND (FFMPEG_FOUND OR MPG123_FOUND))
        find_package(SPEEXDSP)
    endif (ENABLE_SPEEXDSP AND (FFMPEG_FOUND OR MPG123_FOUND))
    if (ENABLE_MTP AND NOT WIN32 AND NOT APPLE)
        find_package(Mtp)
    endif (ENABLE_MTP AND NOT WIN32 AND NOT APPLE)
endif (TAGLIB_FOUND)

if (ENABLE_QT5)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5Xml REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(Qt5Concurrent REQUIRED)
    set(QTCORELIBS ${Qt5Core_LIBRARIES})
    set(QTNETWORKLIBS ${Qt5Network_LIBRARIES})
    set(QTGUILIBS ${Qt5Gui_LIBRARIES})
    set(QTLIBS ${QTCORELIBS} ${Qt5Widgets_LIBRARIES} ${QTNETWORKLIBS} ${QTGUILIBS} ${Qt5Xml_LIBRARIES} ${Qt5Concurrent_LIBRARIES})
    set(QTINCLUDES ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${Qt5Xml_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS})
    add_definitions(${Qt5Widgets_DEFINITIONS} ${Qt5Network_DEFINITIONS} ${Qt5Xml_DEFINITIONS} ${Qt5Concurrent_DEFINITIONS})
    set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
    find_package(Qt5DBus)
    if (Qt5DBus_FOUND)
        set(QT_QTDBUS_FOUND 1) # required for config.h !!!
        set(QTLIBS ${QTLIBS} ${Qt5DBus_LIBRARIES})
        set(QTINCLUDES ${QTINCLUDES} ${Qt5DBus_INCLUDE_DIRS})
        add_definitions(${Qt5DBus_DEFINITIONS})
    endif (Qt5DBus_FOUND)
else (ENABLE_QT5)
    find_package(Qt4 REQUIRED QtCore QtGui QtXml QtNetwork)
    set(QTCORELIBS ${QT_QTCORE_LIBRARY})
    set(QTNETWORKLIBS ${QT_QTNETWORK_LIBRARY})
    set(QTGUILIBS ${QT_QTGUI_LIBRARY})
    set(QTLIBS ${QT_QTSVG_LIBRARY} ${QT_QTXML_LIBRARY} ${QTCORELIBS} ${QTGUILIBS} ${QTNETWORKLIBS})
    if (QT_QTDBUS_FOUND)
        set(QTLIBS ${QTLIBS} ${QT_QTDBUS_LIBRARY})
    endif (QT_QTDBUS_FOUND)
    set(QTINCLUDES ${QT_INCLUDES})
    add_definitions(-DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra)
endif (ENABLE_QT5)

if (ENABLE_HTTP_STREAM_PLAYBACK)
    if (ENABLE_QT5)
        find_package(Qt5Multimedia REQUIRED)
        set(QTLIBS ${QTLIBS} ${Qt5Multimedia_LIBRARIES})
        set(QTINCLUDES ${QTINCLUDES} ${Qt5Multimedia_INCLUDES})
        add_definitions(${Qt5Multimedia_DEFINITIONS})
    else (ENABLE_QT5)
        find_package(Phonon REQUIRED)
    endif (ENABLE_QT5)
endif (ENABLE_HTTP_STREAM_PLAYBACK)

if (ENABLE_KDE)
    find_package(KDE4 REQUIRED)
    add_definitions(-DENABLE_KDE_SUPPORT)
    set(ENABLE_KDE_SUPPORT TRUE)
endif (ENABLE_KDE)

if (NOT ENABLE_QT5 AND NOT WIN32 AND NOT APPLE)
    find_package(QJSON)
endif (NOT ENABLE_QT5 AND NOT WIN32 AND NOT APPLE)

find_package(ZLIB REQUIRED)

if (ENABLE_OVERLAYSCROLLBARS)
    add_definitions(-DENABLE_OVERLAYSCROLLBARS)
endif (ENABLE_OVERLAYSCROLLBARS)

include_directories(${CMAKE_SOURCE_DIR}/gui ${CMAKE_SOURCE_DIR}/mpd ${CMAKE_SOURCE_DIR}/models ${CMAKE_SOURCE_DIR}/widgets
                     ${CMAKE_SOURCE_DIR}/network ${CMAKE_SOURCE_DIR}/dbus ${CMAKE_SOURCE_DIR}/http
                     ${CMAKE_SOURCE_DIR}/devices ${CMAKE_SOURCE_DIR}/dynamic ${CMAKE_SOURCE_DIR}/support ${CMAKE_SOURCE_DIR}/streams
                     ${CMAKE_SOURCE_DIR}/tags ${CMAKE_SOURCE_DIR}/online ${CMAKE_SOURCE_DIR}/context ${CMAKE_BINARY_DIR}/support
                     ${CMAKE_SOURCE_DIR}/3rdparty ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

# QJSon and MusicBrainz require exceptions to be enabled!
if (CMAKE_COMPILER_IS_GNUCXX)
   add_definitions("-fexceptions -UQT_NO_EXCEPTIONS")
endif (CMAKE_COMPILER_IS_GNUCXX)
if (CMAKE_C_COMPILER MATCHES "icc")
   add_definitions(-fexceptions)
endif (CMAKE_C_COMPILER MATCHES "icc")
if (MSVC)
   add_definitions(-EHsc)
endif (MSVC)

set(CANTATA_SRCS gui/application.cpp gui/main.cpp gui/initialsettingswizard.cpp gui/mainwindow.cpp gui/preferencesdialog.cpp
    gui/settings.cpp gui/covers.cpp gui/filesettings.cpp gui/interfacesettings.cpp gui/playbacksettings.cpp 
    gui/serversettings.cpp gui/librarypage.cpp gui/albumspage.cpp gui/folderpage.cpp gui/playlistspage.cpp gui/trayitem.cpp
    gui/cachesettings.cpp gui/coverdialog.cpp gui/stdactions.cpp
    streams/streamspage.cpp streams/streamdialog.cpp streams/streamfetcher.cpp streams/digitallyimportedsettings.cpp
    streams/streamssettings.cpp streams/tar.cpp
    models/musiclibraryitemroot.cpp models/musiclibraryitemartist.cpp models/musiclibraryitemalbum.cpp models/musiclibrarymodel.cpp
    models/musiclibraryproxymodel.cpp models/playlistsmodel.cpp models/playlistsproxymodel.cpp models/playqueuemodel.cpp
    models/playqueueproxymodel.cpp models/dirviewmodel.cpp models/dirviewproxymodel.cpp models/dirviewitem.cpp models/dirviewitemdir.cpp
    models/streamsmodel.cpp models/streamsproxymodel.cpp models/albumsmodel.cpp models/albumsproxymodel.cpp models/proxymodel.cpp
    models/actionmodel.cpp models/digitallyimported.cpp models/streamsearchmodel.cpp models/musiclibraryitem.cpp models/musicmodel.cpp
    models/multimusicmodel.cpp models/onlineservicesmodel.cpp models/musiclibraryitempodcast.cpp
    online/onlineservicespage.cpp online/onlineservice.cpp online/jamendoservice.cpp online/jamendosettingsdialog.cpp
    online/magnatuneservice.cpp online/magnatunesettingsdialog.cpp online/soundcloudservice.cpp online/podcastservice.cpp
    online/rssparser.cpp online/opmlparser.cpp online/podcastsearchdialog.cpp online/onlinesettings.cpp online/podcastsettingsdialog.cpp
    mpd/mpdconnection.cpp mpd/mpdparseutils.cpp mpd/mpdstats.cpp mpd/mpdstatus.cpp mpd/song.cpp mpd/mpduser.cpp mpd/cuefile.cpp
    dynamic/dynamic.cpp dynamic/dynamicpage.cpp dynamic/dynamicproxymodel.cpp dynamic/dynamicruledialog.cpp dynamic/dynamicrulesdialog.cpp
    widgets/treeview.cpp widgets/listview.cpp widgets/itemview.cpp widgets/autohidingsplitter.cpp widgets/timeslider.cpp
    widgets/actionlabel.cpp widgets/playqueueview.cpp widgets/groupedview.cpp widgets/actionitemdelegate.cpp widgets/textbrowser.cpp
    widgets/coverwidget.cpp widgets/volumeslider.cpp widgets/icons.cpp widgets/genrecombo.cpp widgets/toolbar.cpp widgets/menubutton.cpp
    widgets/toolbutton.cpp widgets/wizardpage.cpp widgets/statuslabel.cpp widgets/searchwidget.cpp widgets/messageoverlay.cpp
    widgets/basicitemdelegate.cpp widgets/sizegrip.cpp widgets/sizewidget.cpp widgets/servicestatuslabel.cpp widgets/spacerwidget.cpp
    widgets/songdialog.cpp
    network/networkaccessmanager.cpp network/networkproxyfactory.cpp
    http/httpserversettings.cpp http/httpserver.cpp http/httpsocket.cpp
    devices/deviceoptions.cpp
    context/lyricsettings.cpp context/ultimatelyricsprovider.cpp context/ultimatelyrics.cpp context/lyricsdialog.cpp
    context/contextwidget.cpp context/view.cpp context/artistview.cpp context/albumview.cpp context/songview.cpp context/contextengine.cpp
    context/wikipediaengine.cpp context/wikipediasettings.cpp context/othersettings.cpp context/contextsettings.cpp context/togglelist.cpp
    context/lastfmengine.cpp context/metaengine.cpp context/backdropcreator.cpp)
set(CANTATA_MOC_HDRS
    gui/initialsettingswizard.h gui/mainwindow.h gui/settings.h gui/covers.h gui/folderpage.h gui/librarypage.h gui/albumspage.h
    gui/playlistspage.h gui/playbacksettings.h gui/serversettings.h gui/preferencesdialog.h gui/filesettings.h
    gui/interfacesettings.h gui/cachesettings.h gui/trayitem.h gui/coverdialog.h
    streams/streamspage.h streams/streamdialog.h streams/streamfetcher.h streams/digitallyimportedsettings.h streams/streamssettings.h
    models/musiclibrarymodel.h models/musiclibraryproxymodel.h models/playlistsmodel.h models/playlistsproxymodel.h models/playqueuemodel.h
    models/playqueueproxymodel.h models/dirviewmodel.h models/dirviewproxymodel.h models/albumsmodel.h models/streamsmodel.h
    models/actionmodel.h models/digitallyimported.h models/streamsearchmodel.h models/multimusicmodel.h models/onlineservicesmodel.h
    online/onlineservice.h online/onlineservicespage.h online/jamendoservice.h online/magnatuneservice.h
    online/magnatunesettingsdialog.h online/magnatuneservice.h online/magnatunesettingsdialog.h online/soundcloudservice.h
    online/podcastservice.h online/podcastsearchdialog.h online/podcastsettingsdialog.h online/onlinesettings.h
    mpd/mpdconnection.h mpd/mpdstats.h mpd/mpdstatus.h
    dynamic/dynamic.h dynamic/dynamicpage.h dynamic/dynamicruledialog.h dynamic/dynamicrulesdialog.h
    widgets/treeview.h widgets/listview.h widgets/itemview.h widgets/autohidingsplitter.h widgets/timeslider.h widgets/actionlabel.h
    widgets/playqueueview.h widgets/groupedview.h widgets/actionitemdelegate.h widgets/coverwidget.h widgets/volumeslider.h
    widgets/genrecombo.h widgets/toolbar.h widgets/searchwidget.h widgets/messageoverlay.h widgets/servicestatuslabel.h
    network/networkaccessmanager.h
    http/httpserversettings.h http/httpsocket.h
    context/togglelist.h context/ultimatelyrics.h context/ultimatelyricsprovider.h context/lyricsdialog.h
    context/contextwidget.h context/artistview.h context/albumview.h context/songview.h context/view.h context/contextengine.h
    context/wikipediaengine.h context/wikipediasettings.h context/othersettings.h context/lastfmengine.h context/metaengine.h
    context/backdropcreator.h)
set(CANTATA_UIS
    gui/initialsettingswizard.ui gui/mainwindow.ui gui/folderpage.ui gui/librarypage.ui gui/albumspage.ui gui/playlistspage.ui
    gui/filesettings.ui gui/interfacesettings.ui gui/playbacksettings.ui gui/serversettings.ui gui/coverdialog.ui
    streams/streamspage.ui streams/digitallyimportedsettings.ui streams/streamssettings.ui
    dynamic/dynamicpage.ui dynamic/dynamicrule.ui dynamic/dynamicrules.ui
    context/togglelist.ui context/othersettings.ui
    widgets/itemview.ui
    http/httpserversettings.ui
    online/onlineservicespage.ui online/onlinesettings.ui)

if (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)
    set(QTLIBS ${QTLIBS} ${QT_QTTEST_LIBRARY})
    set(CANTATA_SRCS ${CANTATA_SRCS} models/modeltest.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} models/modeltest.h)
    set(CMAKE_BUILD_TYPE "Debug")
endif (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)

if (QT_QTDBUS_FOUND)
    if (NOT ENABLE_KDE)
        set(CANTATA_SRCS ${CANTATA_SRCS} dbus/gnomemediakeys.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} dbus/gnomemediakeys.h)
        set_SOURCE_FILES_PROPERTIES(dbus/org.gnome.SettingsDaemon.xml dbus/org.gnome.SettingsDaemon.MediaKeys.xml PROPERTIES NO_NAMESPACE TRUE)
    endif (NOT ENABLE_KDE)
    set(CANTATA_SRCS ${CANTATA_SRCS} dbus/mpris.cpp dbus/powermanagement.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} dbus/mpris.h dbus/powermanagement.h)
    set(CANTATA_SRCS ${CANTATA_SRCS} dbus/notify.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} dbus/notify.h)

    if (ENABLE_QT5)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/com.googlecode.cantata.xml gui/mainwindow.h MainWindow)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.MediaKeys.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.kde.Solid.PowerManagement.PolicyAgent.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.PowerManagement.Inhibit.xml)
    else (ENABLE_QT5)
        if ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
            qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.OLD.xml dbus/mpris.h Mpris)
        else ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
            qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
        endif ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/com.googlecode.cantata.xml gui/mainwindow.h MainWindow)
        if ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.OLD.xml)
        else ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
        endif ( (QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) )
        if (NOT ENABLE_KDE)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.xml)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.MediaKeys.xml)
        endif (NOT ENABLE_KDE)
        qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.kde.Solid.PowerManagement.PolicyAgent.xml)
        qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.PowerManagement.Inhibit.xml)
    endif (ENABLE_QT5)
endif (QT_QTDBUS_FOUND)

if (NOT WIN32 AND NOT APPLE)
    set(CANTATA_SRCS ${CANTATA_SRCS} devices/mountpoints.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mountpoints.h)
endif (NOT WIN32 AND NOT APPLE)

set(CANTATA_RCS cantata.qrc)

if (TAGLIB_FOUND)
    set(CANTATA_SRCS ${CANTATA_SRCS}
        tags/tageditor.cpp tags/trackorganiser.cpp
        devices/filenameschemedialog.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS}
        tags/tageditor.h tags/trackorganiser.h
        devices/filenameschemedialog.h devices/device.h)
    set(CANTATA_UIS ${CANTATA_UIS}
        tags/tageditor.ui tags/trackorganiser.ui
        devices/filenameschemedialog.ui)

    if (FFMPEG_FOUND OR MPG123_FOUND)
        include_directories(${CMAKE_SOURCE_DIR}/replaygain)
        set(CANTATA_SRCS ${CANTATA_SRCS} replaygain/albumscanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp replaygain/jobcontroller.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} replaygain/albumscanner.h replaygain/rgdialog.h replaygain/tagreader.h replaygain/jobcontroller.h)
        set(USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})
        set(ENABLE_REPLAYGAIN_SUPPORT 1)
        add_subdirectory(replaygain)
    endif (FFMPEG_FOUND OR MPG123_FOUND)
    if (ENABLE_EXTERNAL_TAGS)
        set(CANTATA_SRCS ${CANTATA_SRCS} tags/tagclient.cpp)
        add_subdirectory(tags)
    else (ENABLE_EXTERNAL_TAGS)
        set(CANTATA_SRCS ${CANTATA_SRCS} tags/tags.cpp tags/filetyperesolver.cpp)
    endif (ENABLE_EXTERNAL_TAGS)

    if (WIN32 OR APPLE)
        set(CANTATA_SRCS ${CANTATA_SRCS} devices/device.cpp)
    else (WIN32 OR APPLE)
        set(ENABLE_DEVICES_SUPPORT 1)
        if (NOT ENABLE_KDE)
            add_subdirectory(3rdparty/solid-lite)
        endif (NOT ENABLE_KDE)
        if (MTP_FOUND)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mtpdevice.h)
        endif (MTP_FOUND)
        if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
            if (CDDB_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/cddbinterface.cpp)
                set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/cddbinterface.h)
            endif (CDDB_FOUND)
            if (MUSICBRAINZ5_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/musicbrainz.cpp)
                set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/musicbrainz.h)
            endif (MUSICBRAINZ5_FOUND)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/audiocddevice.cpp devices/cddbselectiondialog.cpp
                 devices/cdparanoia.cpp devices/audiocdsettings.cpp devices/extractjob.cpp devices/albumdetailsdialog.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/audiocddevice.h devices/extractjob.h
                 devices/albumdetailsdialog.h devices/cddbselectiondialog.h)
            set(CANTATA_UIS ${CANTATA_UIS} devices/albumdetails.ui devices/audiocdsettings.ui)
        endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)

        set(CANTATA_SRCS ${CANTATA_SRCS} devices/devicespage.cpp devices/filejob.cpp
                devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp
                models/devicesmodel.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
                devices/devicepropertiesdialog.cpp devices/encoders.cpp devices/freespaceinfo.cpp
                devices/transcodingjob.cpp devices/valueslider.cpp devices/syncdialog.cpp devices/synccollectionwidget.cpp
                online/onlinedevice.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/devicespage.h devices/filejob.h
                devices/fsdevice.h devices/umsdevice.h models/devicesmodel.h
                devices/actiondialog.h devices/devicepropertieswidget.h devices/devicepropertiesdialog.h
                devices/transcodingjob.h devices/valueslider.h devices/syncdialog.h devices/synccollectionwidget.h
                online/onlinedevice.h)
        set(CANTATA_UIS ${CANTATA_UIS} devices/devicespage.ui devices/actiondialog.ui devices/devicepropertieswidget.ui
                devices/synccollectionwidget.ui)

        if (ENABLE_REMOTE_DEVICES)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/remotefsdevice.cpp devices/remotedevicepropertiesdialog.cpp
                devices/remotedevicepropertieswidget.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/remotefsdevice.h devices/remotedevicepropertiesdialog.h
                devices/remotedevicepropertieswidget.h)
            set(CANTATA_UIS ${CANTATA_UIS} devices/remotedevicepropertieswidget.ui)
            if (ENABLE_QT5)
                qt5_add_dbus_interfaces(CANTATA_SRCS devices/mounter/com.googlecode.cantata.mounter.xml)
            else (ENABLE_QT5)
                qt4_add_dbus_interfaces(CANTATA_SRCS devices/mounter/com.googlecode.cantata.mounter.xml)
            endif (ENABLE_QT5)
            add_subdirectory(devices/mounter)
            add_subdirectory(devices/avahi)
        endif (ENABLE_REMOTE_DEVICES)
    endif (WIN32 OR APPLE)
endif (TAGLIB_FOUND)

if (ENABLE_PROXY_CONFIG)
    set(CANTATA_SRCS ${CANTATA_SRCS} network/proxysettings.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} network/proxysettings.h)
    set(CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui)
endif (ENABLE_PROXY_CONFIG)

add_subdirectory(po)
add_subdirectory(support)
add_subdirectory(streams/icons)
add_subdirectory(streams/providers)
add_subdirectory(online/icons)

if (ENABLE_KDE)
    include_directories(${KDE4_INCLUDES})
    add_definitions(${KDE4_DEFINITIONS})
    qt4_add_resources(CANTATA_RC_SRCS ${CANTATA_RCS})
    kde4_add_ui_files(CANTATA_UI_HDRS ${CANTATA_UIS})
    kde4_add_executable(cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
    target_link_libraries(cantata ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_SOLID_LIBS} ${KDE4_KIO_LIBRARY} X11)
    if (NOT WIN32 AND NOT APPLE AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
        target_link_libraries(cantata ${KDE4_KFILE_LIBS})
    endif (NOT WIN32 AND NOT APPLE AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
    install(TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS})
else (ENABLE_KDE)
    if (WIN32)
        add_definitions(-DWIN32)
        add_subdirectory(windows)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/application.h)
        if (NOT CANTATA_WINDOWS_INSTALLER_DEST)
            set(CANTATA_WINDOWS_INSTALLER_DEST z:\ )
        endif (NOT CANTATA_WINDOWS_INSTALLER_DEST)
        if (MINGW)
            # resource compilation for MinGW
            ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o
                                COMMAND windres.exe -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/windows/cantataico.rc
                                    -o ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
            set(CANTATA_SRCS ${CANTATA_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
        else (MINGW)
            set(CANTATA_SRCS ${CANTATA_SRCS} windows/cantataico.rc)
        endif (MINGW)
    endif (WIN32)

    set(CANTATA_RCS ${CANTATA_RCS} cantata_qt.qrc)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/shortcutssettingspage.cpp gui/mediakeys.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/shortcutssettingspage.h gui/multimediakeysinterface.h)

    if (ENABLE_QT5)
        QT5_ADD_RESOURCES(CANTATA_RC_SRCS ${CANTATA_RCS})
        QT5_WRAP_UI(CANTATA_UI_HDRS ${CANTATA_UIS})
        QT5_WRAP_CPP(CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS})
    else (ENABLE_QT5)
        qt4_add_resources(CANTATA_RC_SRCS ${CANTATA_RCS})
        QT4_WRAP_UI(CANTATA_UI_HDRS ${CANTATA_UIS})
        INCLUDE(${QT_USE_FILE})
        QT4_WRAP_CPP(CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS})
    endif (ENABLE_QT5)

    if (WIN32 OR APPLE OR NOT ENABLE_QT5)
        add_definitions(-DQXT_STATIC)
        add_subdirectory(3rdparty/qxt)
        include_directories(${CMAKE_SOURCE_DIR}/3rdparty/qxt)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/qxtmediakeys.cpp)
    endif (WIN32 OR APPLE OR NOT ENABLE_QT5)

    if (WIN32)
        set(CMAKE_BUILD_TYPE "Release")
        ADD_EXECUTABLE(cantata WIN32 ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
        install(TARGETS cantata DESTINATION ${CMAKE_INSTALL_PREFIX})
    elseif (APPLE)
        ADD_EXECUTABLE(cantata MACOSX_BUNDLE ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
        #ADD_EXECUTABLE(cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
        set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
        set(MACOSX_BUNDLE_BUNDLE_NAME Cantata)
        SET(MACOSX_BUNDLE_SHORT_VERSION_STRING ${CANATA_VERSION})
        SET(MACOSX_BUNDLE_VERSION ${CANATA_VERSION})
        SET(MACOSX_BUNDLE_LONG_VERSION_STRING ${CANTATA_VERSION_FULL})
        set(MACOSX_BUNDLE_ICON_FILE "cantata.icns")
        add_subdirectory(mac)
        include(DeployQt4)
        INSTALL_QT4_EXECUTABLE("cantata.app" "qjpeg;qsvg;qsvgicon")
    else (WIN32)
        ADD_EXECUTABLE(cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
        install(TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    endif (WIN32)

    set(XDG_APPS_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/applications")

    if (WIN32)
        add_subdirectory(3rdparty/qtsingleapplication)
        target_link_libraries(cantata qtsingleapplication)
    endif (WIN32)
    if (WIN32 OR APPLE OR NOT ENABLE_QT5)
        target_link_libraries(cantata qxt)
    endif (WIN32 OR APPLE OR NOT ENABLE_QT5)
    if (NOT ENABLE_QT5 AND NOT APPLE AND NOT WIN32)
        target_link_libraries(cantata X11)
    endif (NOT ENABLE_QT5 AND NOT APPLE AND NOT WIN32)
endif (ENABLE_KDE)

if (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_QT5)
    target_link_libraries(cantata ${PHONON_LIBS})
    include_directories(${PHONON_INCLUDES})
endif (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_QT5)

if (WIN32 OR APPLE)
    # install(FILES mpd/mpd.conf.template DESTINATION ${CMAKE_INSTALL_PREFIX}/mpd)
else (WIN32 OR APPLE)
    install(PROGRAMS dynamic/cantata-dynamic DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/scripts)
    install(FILES mpd/mpd.conf.template DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/mpd)

    if (MTP_FOUND)
        target_link_libraries(cantata ${MTP_LIBRARIES})
        include_directories(${MTP_INCLUDE_DIR})
    endif (MTP_FOUND)

    if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
        target_link_libraries(cantata ${CDPARANOIA_LIBRARIES})
        include_directories(${CDPARANOIA_INCLUDE_DIR})
        if (CDDB_FOUND)
            target_link_libraries(cantata ${CDDB_LIBS})
            include_directories(${CDDB_INCLUDE_DIR})
        endif (CDDB_FOUND)
        if (MUSICBRAINZ5_FOUND)
            target_link_libraries(cantata ${MUSICBRAINZ5_LIBRARIES})
            include_directories(${MUSICBRAINZ5_INCLUDE_DIR})
        endif (MUSICBRAINZ5_FOUND)
        if (ENABLE_KDE)
            install(FILES cantata-play-audiocd.desktop DESTINATION ${DATA_INSTALL_DIR}/solid/actions)
        endif (ENABLE_KDE)
    endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
endif (WIN32 OR APPLE)

if (QJSON_FOUND)
    include_directories(${QJSON_INCLUDE_DIR})
    target_link_libraries(cantata ${QJSON_LIBRARIES})
else (QJSON_FOUND)
    add_subdirectory(3rdparty/qjson)
    target_link_libraries(cantata qjson)
endif (QJSON_FOUND)

add_subdirectory(3rdparty/qtiocompressor)
add_subdirectory(icons)

target_link_libraries(cantata support qtiocompressor ${QTLIBS} ${ZLIB_LIBRARIES})
include_directories(${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})

if (TAGLIB_FOUND)
    # Cantata still links to taglib, even if external tag reader/writer is used,
    # because JamendoService uses taglib for ID3 genres.
    target_link_libraries(cantata ${TAGLIB_LIBRARIES})
    include_directories(${TAGLIB_INCLUDES})
    if (NOT ENABLE_EXTERNAL_TAGS)     
        if (TAGLIB-EXTRAS_FOUND)
            target_link_libraries(cantata ${TAGLIB-EXTRAS_LIBRARIES})
            include_directories(${TAGLIB-EXTRAS_INCLUDES})
        endif (TAGLIB-EXTRAS_FOUND)
    endif (NOT ENABLE_EXTERNAL_TAGS)
    if (NOT ENABLE_KDE AND NOT WIN32 AND NOT APPLE)
        target_link_libraries(cantata solidlite)
    endif (NOT ENABLE_KDE AND NOT WIN32 AND NOT APPLE)
    if (ENABLE_REMOTE_DEVICES AND NOT WIN32 AND NOT APPLE)
        target_link_libraries(cantata avahi)
    endif (ENABLE_REMOTE_DEVICES AND NOT WIN32 AND NOT APPLE)
endif (TAGLIB_FOUND)

# enable warnings
add_definitions(-DQT_NO_DEBUG_OUTPUT)

if (UNIX AND NOT APPLE)
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
endif (UNIX AND NOT APPLE)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

message("")
if (ENABLE_KDE)
    message("INFO: Building KDE version")
else (ENABLE_KDE)
    if (NOT WIN32)
        if (ENABLE_QT5)
            message("INFO: Building Qt5 version")
        else (ENABLE_QT5)
            message("INFO: Building Qt4 version")
        endif (ENABLE_QT5)
    endif (NOT WIN32)
endif (ENABLE_KDE)

if (TAGLIB_FOUND)

    if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
        set(AUDIOCD_SUPPORT 1)
    endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)

    if (ENABLE_REMOTE_DEVICES OR ENABLE_HTTP_STREAM_PLAYBACK OR AUDIOCD_SUPPORT OR MTP_FOUND OR ENABLE_PROXY_CONFIG OR ENABLE_REPLAYGAIN_SUPPORT)
        message("      Extra enabled features:")
        if (MTP_FOUND)
            message("        - MTP device sync")
        endif (MTP_FOUND)
        if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
            message("        - AudioCD")
        endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
        if (ENABLE_REMOTE_DEVICES)
            message("        - Remote device sync (EXPERIMENTAL)")
        endif (ENABLE_REMOTE_DEVICES)
        if (ENABLE_HTTP_STREAM_PLAYBACK)
            message("        - MPD HTTP stream playback")
        endif (ENABLE_HTTP_STREAM_PLAYBACK)
        if (ENABLE_REPLAYGAIN_SUPPORT)
            message("        - ReplayGain calculation")
        endif (ENABLE_REPLAYGAIN_SUPPORT)
        if (ENABLE_PROXY_CONFIG)
            message("        - Proxy configuration")
        endif (ENABLE_PROXY_CONFIG)
    endif (ENABLE_REMOTE_DEVICES OR ENABLE_HTTP_STREAM_PLAYBACK OR AUDIOCD_SUPPORT OR MTP_FOUND OR ENABLE_PROXY_CONFIG OR ENABLE_REPLAYGAIN_SUPPORT)
    
    if (NOT TAGLIB_FOUND OR NOT MTP_FOUND OR NOT AUDIOCD_SUPPORT OR NOT ENABLE_REPLAYGAIN_SUPPORT OR NOT ENABLE_HTTP_STREAM_PLAYBACK OR NOT ENABLE_REMOTE_DEVICES OR (NOT ENABLE_PROXY_CONFIG AND NOT ENABLE_KDE))
        message("      Disabled features:")
        if (NOT ENABLE_HTTP_STREAM_PLAYBACK)
            message("        - MPD HTTP stream playback (Phonon required for Qt4, QtMultiMedia for Qt5)")
        endif (NOT ENABLE_HTTP_STREAM_PLAYBACK)
        if (NOT MTP_FOUND AND NOT WIN32 AND NOT APPLE)
            message("        - MTP device sync (taglib and libmtp required)")
        endif (NOT MTP_FOUND AND NOT WIN32 AND NOT APPLE)
        if (NOT AUDIOCD_SUPPORT AND NOT WIN32 AND NOT APPLE)
            message("        - AudioCD (libcdparanoia and libcddb/libmusicbrainz5 required)")
        endif (NOT AUDIOCD_SUPPORT AND NOT WIN32 AND NOT APPLE)
        if (NOT ENABLE_REMOTE_DEVICES)
            message("        - Remote device sync (EXPERIMENTAL)")
        endif (NOT ENABLE_REMOTE_DEVICES)
        if (NOT ENABLE_REPLAYGAIN_SUPPORT)
            message("        - ReplayGain calculation (taglib, and ffmpeg and/or mpg123 required)")
        endif (NOT ENABLE_REPLAYGAIN_SUPPORT)
        if (NOT ENABLE_PROXY_CONFIG AND NOT ENABLE_KDE)
            message("        - Proxy configuration")
        endif (NOT ENABLE_PROXY_CONFIG AND NOT ENABLE_KDE)
    endif (NOT TAGLIB_FOUND OR NOT MTP_FOUND OR NOT AUDIOCD_SUPPORT OR NOT ENABLE_REPLAYGAIN_SUPPORT OR NOT ENABLE_HTTP_STREAM_PLAYBACK OR NOT ENABLE_REMOTE_DEVICES OR (NOT ENABLE_PROXY_CONFIG AND NOT ENABLE_KDE))

    if (ENABLE_EXTERNAL_TAGS)
        message("      Tags will be read/written via an external helper app")
        message("      NOTE: This is EXPERIMENTAL, and is NOT CURRENTLY WORKING!!!!")
        message("            Do NOT report bugs with this build!!!")
    endif (ENABLE_EXTERNAL_TAGS)
else (TAGLIB_FOUND)

    message("      Disabled features:")
    if (NOT PHONON_FOUND)
        message("        - MPD HTTP stream playback (phonon required)")
    endif (NOT PHONON_FOUND)
    message("        - Tag Editor (taglib required)")
    message("        - Track Organizer (taglib required)")
    if (NOT WIN32 AND NOT APPLE)
        message("        - UMS device sync (taglib required)")
        message("        - MTP device sync (taglib and libmtp required)")
        message("        - AudioCD (taglib, libcdparanoia, and libcddb/libmusicbrainz5 required)")
        message("        - AudioCD playback via MP3 (liblame required)")
        message("        - Remote device sync (EXPERIMENTAL) (taglib required)")
    endif (NOT WIN32 AND NOT APPLE)
    if (NOT ENABLE_REPLAYGAIN_SUPPORT)
        message("        - ReplayGain calculation(taglib, and ffmpeg and/or mpg123 required)")
    endif (NOT ENABLE_REPLAYGAIN_SUPPORT)

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

if (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)
    message("***************************************************")
    message("************* ENABLING MODEL TESTS!!! *************")
    message("***************************************************")
    message("")
    message("This build is intended to be run under a debugger!!")
    message("DO NOT use this as a release build!!!!")
    message("")
endif (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)
