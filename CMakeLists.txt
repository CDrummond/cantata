PROJECT(cantata)

#  NOTE: If PROJECT_URL, or PROJECT_REV_URL, are changed, then cantata-dynamic, and README will need updating.
#        dbus/mpd.cantata.xml and devices/mounter/mpd.cantata.mounter.xml will also need renaming/updating.
set(PROJECT_URL "cantata.mpd")
set(PROJECT_REV_URL "mpd.cantata")

if (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")

if (WIN32)
    # For some reason I chose mpd as the organization name for Windows builds. So, for config compatability, this is
    # left as mpd for now...
    set(ORGANIZATION_NAME "mpd")
else (WIN32)
    # Otherwise its set to the same as the project name. Not sure what else to set it to!!!
    set(ORGANIZATION_NAME ${CMAKE_PROJECT_NAME})
endif (WIN32)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TBZ2")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt/KDE Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "50")
set(CPACK_PACKAGE_VERSION_SPIN "")  # Use ".$number" - e.g. ".1"
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CANTATA_VERSION_WITH_SPIN "${CANTATA_VERSION_FULL}${CPACK_PACKAGE_VERSION_SPIN}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_WITH_SPIN}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_WITH_SPIN}")
set(CPACK_SOURCE_IGNORE_FILES  "/.svn/;/.git/;.gitignore;.project;CMakeLists.txt.user;README.md")
include(CPack)
include(MacroLogFeature)

option(ENABLE_TAGLIB "Enable TagLib library (required for tag editing, replaygain calculation, device support, etc)" ON)
option(ENABLE_TAGLIB_EXTRAS "Enable TagLib-Extras library (used by tag editing, replaygain calculation, device support, etc)" ON)
option(ENABLE_HTTP_STREAM_PLAYBACK "Enable playback of MPD HTTP streams (LibVLC or, Phonon for Qt4, QtMultimedia for Qt5)" ON)
if (WIN32 OR APPLE)
    option(ENABLE_LIBVLC "Use libVLC for MPD HTTP stream playback (if ENABLE_HTTP_STREAM_PLAYBACK=ON)" OFF)
else (WIN32 OR APPLE)
    option(ENABLE_LIBVLC "Use libVLC for MPD HTTP stream playback (if ENABLE_HTTP_STREAM_PLAYBACK=ON)" ON)
endif (WIN32 OR APPLE)
option(ENABLE_KDE "Enable KDE libraries" OFF)
option(ENABLE_QT5 "Build against Qt5" ON)
option(ENABLE_UBUNTU "Build for the Ubuntu SDK" OFF)
option(ENABLE_UBUNTU_COMMAND_LINE "Build for the Ubuntu SDK from the command line" OFF)
option(ENABLE_FFMPEG "Enable ffmpeg/libav libraries (required for replaygain calculation)" ON)
option(ENABLE_MPG123 "Enable mpg123 libraries (required for replaygain calculation)" ON)
option(ENABLE_PROXY_CONFIG "Enable proxy config in settings dialog" OFF)
option(ENABLE_EXTERNAL_TAGS "Enable usage of external app for reading/writing tags" ON)
option(ENABLE_HTTP_SERVER "Enable internal HTTP server to play non-MPD files" ON)
if (NOT WIN32 AND NOT APPLE AND NOT ENABLE_UBUNTU)
    option(ENABLE_SIMPLE_MPD_SUPPORT "Enable support for simple (Cantata controlled) MPD" ON)
endif (NOT WIN32 AND NOT APPLE AND NOT ENABLE_UBUNTU)

if (WIN32 OR APPLE)
    option(ENABLE_DEVICES_SUPPORT "Enable suport for external devices" OFF)
else (WIN32 OR APPLE)
    option(ENABLE_DEVICES_SUPPORT "Enable suport for external devices" ON)
endif (WIN32 OR APPLE)
if (WIN32)
    set(USE_SYSTEM_MENU_ICON OFF)
    option(ENABLE_TOUCH_SUPPORT "Enable option to make interface more touch-friendly" ON)
else (WIN32)
    option(USE_SYSTEM_MENU_ICON "Use system menu icon" ON)
    option(ENABLE_TOUCH_SUPPORT "Enable option to make interface more touch-friendly" OFF)
endif (WIN32)
option(ENABLE_CDPARANOIA "Enable CDParanoia libraries (required for AudioCD support)" ON)
option(ENABLE_CDDB "Enable CDDB libraries (either this or MusicBrianz required for AudioCD support)" ON)
option(ENABLE_MUSICBRAINZ "Enable MusicBrianz libraries (either this or CDDB required for AudioCD support)" ON)
option(ENABLE_UDISKS2 "Build UDisks2 backend, and NOT UDisks, for Qt builds" ON)
option(ENABLE_REMOTE_DEVICES "Enable support for remote (sshfs, samba) devices (EXPERIMENTAL)" OFF)
option(ENABLE_MTP "Enable MTP library (required to support MTP devices)" ON)
option(ENABLE_UNCACHED_MTP "Open MTP devices in un-cached mode (faster)" ON)

if (NOT APPLE AND NOT WIN32)
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share"
        CACHE PATH "Define install directory for read-only architecture-independent data")
endif (NOT APPLE AND NOT WIN32)
if (WIN32)
    set(ENABLE_QT5 ON)
    set(CANTATA_ICON_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/icons/cantata)
elseif (APPLE)
    set(ENABLE_QT5 ON)
    set(MACOSX_BUNDLE_BUNDLE_NAME Cantata)
    set(MACOSX_BUNDLE_EXECUTABLE cantata)
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${CANTATA_VERSION})
    set(MACOSX_BUNDLE_VERSION ${CANTATA_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${CANTATA_VERSION_WITH_SPIN})
    set(MACOSX_BUNDLE_ICON_FILE cantata.icns)
    set(CANTATA_APP_CONTENTS_DIR ${CMAKE_INSTALL_PREFIX}/Cantata.app/Contents)
    set(MACOSX_BUNDLE_RESOURCES ${CANTATA_APP_CONTENTS_DIR}/Resources)
    set(MACOSX_BUNDLE_APP_DIR ${CANTATA_APP_CONTENTS_DIR}/MacOS)
    set(CANTATA_ICON_INSTALL_PREFIX ${MACOSX_BUNDLE_RESOURCES}/icons/cantata)
elseif (NOT ENABLE_UBUNTU)
    set(CANTATA_ICON_INSTALL_PREFIX ${SHARE_INSTALL_PREFIX}/icons/hicolor)
endif (WIN32)

if (ENABLE_UBUNTU_COMMAND_LINE)
    set(ENABLE_UBUNTU ON)
endif (ENABLE_UBUNTU_COMMAND_LINE)

if (NOT APPLE AND NOT WIN32 AND NOT ENABLE_UBUNTU AND NOT ENABLE_KDE_SUPPORT)
    set(UNITY_MENU_HACK ON)
endif (NOT APPLE AND NOT WIN32 AND NOT ENABLE_UBUNTU AND NOT ENABLE_KDE_SUPPORT)

if (ENABLE_UBUNTU)
    set(ENABLE_QT5 ON)
    set(ENABLE_TAGLIB OFF)
    set(ENABLE_TAGLIB_EXTRAS OFF)
    set(ENABLE_FFMPEG OFF)
    set(ENABLE_MPG123 OFF)
    set(ENABLE_EXTERNAL_TAGS OFF)
    set(ENABLE_HTTP_SERVER OFF)
    set(ENABLE_CDPARANOIA OFF)
    set(ENABLE_CDDB OFF)
    set(ENABLE_MUSICBRAINZ OFF)
    set(ENABLE_UDISKS2 OFF)
    set(ENABLE_DEVICES_SUPPORT OFF)
    set(ENABLE_REMOTE_DEVICES OFF)
    set(ENABLE_MTP OFF)
    set(ENABLE_UNCACHED_MTP OFF)
    set(USE_SYSTEM_MENU_ICON OFF)
endif (ENABLE_UBUNTU)

if (ENABLE_KDE)
    set(ENABLE_QT5 OFF)
endif (ENABLE_KDE)

if (NOT WIN32 AND NOT APPLE AND ENABLE_DEVICES_SUPPORT AND ENABLE_UDISKS2)
    set(WITH_SOLID_UDISKS2 ON)
endif (NOT WIN32 AND NOT APPLE AND ENABLE_DEVICES_SUPPORT AND ENABLE_UDISKS2)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message("-- Set build type to ${CMAKE_BUILD_TYPE}")
endif (NOT CMAKE_BUILD_TYPE)

if (NOT APPLE AND NOT WIN32)
    if (CANTATA_HELPERS_LIB_DIR AND NOT ENABLE_KDE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINUX_LIB_DIR ${CANTATA_HELPERS_LIB_DIR})
    else (CANTATA_HELPERS_LIB_DIR AND NOT ENABLE_KDE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINUX_LIB_DIR lib)
    endif (CANTATA_HELPERS_LIB_DIR AND NOT ENABLE_KDE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
endif (NOT APPLE AND NOT WIN32)

if (ENABLE_UBUNTU)
    if (ENABLE_UBUNTU_COMMAND_LINE)
        set(CMAKE_INSTALL_PREFIX click)
    else (ENABLE_UBUNTU_COMMAND_LINE)
        set(CMAKE_INSTALL_PREFIX /)
    endif (ENABLE_UBUNTU_COMMAND_LINE)
elseif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if (ENABLE_KDE)
        find_program(KDE4_KDECONFIG_EXECUTABLE NAMES kde4-config
                     PATHS ${CMAKE_INSTALL_PREFIX}/bin ${_KDEDIRS} /opt/kde4/bin NO_DEFAULT_PATH)
        if (NOT KDE4_KDECONFIG_EXECUTABLE)
           find_program(KDE4_KDECONFIG_EXECUTABLE NAMES kde4-config )
        endif (NOT KDE4_KDECONFIG_EXECUTABLE)
        if (KDE4_KDECONFIG_EXECUTABLE)
            exec_program(${KDE4_KDECONFIG_EXECUTABLE} ARGS --prefix OUTPUT_VARIABLE CMAKE_INSTALL_PREFIX)
            message("-- Set install prefix to ${CMAKE_INSTALL_PREFIX}")
        endif (KDE4_KDECONFIG_EXECUTABLE)
    elseif (NOT APPLE AND NOT WIN32)
        set(CMAKE_INSTALL_PREFIX "/usr")
        message("-- Set install prefix to ${CMAKE_INSTALL_PREFIX}")
    endif (ENABLE_KDE)
endif (ENABLE_UBUNTU)

if (NOT ENABLE_QT5 AND NOT APPLE AND NOT WIN32)
    find_package(X11)
    if (X11_FOUND)
        include_directories(${X11_INCLUDE_DIR})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${X11_LIBRARIES})
    endif (X11_FOUND)
endif (NOT ENABLE_QT5 AND NOT APPLE AND NOT WIN32)

if (ENABLE_TAGLIB)
    set(TAGLIB_MIN_VERSION "1.6")
    find_package(Taglib)
    macro_log_feature(TAGLIB_FOUND "TagLib" "Tag editor, file organiser, etc." "http://taglib.github.io/")
endif (ENABLE_TAGLIB)

if (TAGLIB_FOUND)
    if (ENABLE_DEVICES_SUPPORT AND ENABLE_CDPARANOIA)
        find_package(Cdparanoia)
        macro_log_feature(CDPARANOIA_FOUND "libcdda_paranoia" "CD ripping." "http://www.xiph.org/paranoia")
        if (CDPARANOIA_FOUND)
            if (ENABLE_CDDB)
                find_package(CDDB)
                macro_log_feature(CDDB_FOUND "libcddb" "CD info retrieval via CDDB." "http://libcddb.sourceforge.net")
            endif (ENABLE_CDDB)
            if (ENABLE_MUSICBRAINZ)
                find_package(MusicBrainz5)
                macro_log_feature(MUSICBRAINZ5_FOUND "libmusicbrainz5" "CD info retrieval via MusicBrainz." "http://musicbrainz.org/doc/libmusicbrainz")
            endif (ENABLE_MUSICBRAINZ)
        endif (CDPARANOIA_FOUND)
    endif (ENABLE_DEVICES_SUPPORT AND ENABLE_CDPARANOIA)

    set(ENABLE_TAGEDITOR_SUPPORT 1)
    set(ENABLE_TRACKORGANIZER_SUPPORT 1)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDES}")
    set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")

    check_cxx_source_compiles("#include <asftag.h>
                            int main() { TagLib::ASF::Tag tag; return 0;}" TAGLIB_ASF_FOUND)
    if (NOT TAGLIB_ASF_FOUND)
        message("TagLib does not have ASF support compiled in.")
    endif (NOT TAGLIB_ASF_FOUND)

    check_cxx_source_compiles("#include <mp4tag.h>
                            int main() { TagLib::MP4::Tag tag(0, 0); return 0;}" TAGLIB_MP4_FOUND)
    if (NOT TAGLIB_MP4_FOUND)
        message("TagLib does not have MP4 support compiled in.")
    endif (NOT TAGLIB_MP4_FOUND)

    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_LIBRARIES)

    set(TAGLIB-EXTRAS_MIN_VERSION "1.0")
    if (ENABLE_TAGLIB_EXTRAS)
        find_package(Taglib-Extras)
        macro_log_feature(TAGLIB-EXTRAS_FOUND "TagLib-Extras" "Tag editor, file organiser, etc." "http://taglib.github.io/")
    endif (ENABLE_TAGLIB_EXTRAS)
    set(TAGLIB_EXTRAS_FOUND ${TAGLIB-EXTRAS_FOUND}) # we need a c-compatible name for the include file

    #include(CheckTagLibFileName)
    #check_taglib_filename(COMPLEX_TAGLIB_FILENAME)

    add_definitions(${TAGLIB_CFLAGS})
    if (TAGLIB-EXTRAS_FOUND)
        include_directories(${TAGLIB-EXTRAS_INCLUDES})
        add_definitions(${TAGLIB-EXTRAS_CFLAGS})
    endif (TAGLIB-EXTRAS_FOUND)

    if (ENABLE_FFMPEG)
        find_package(FFMPEG)
        macro_log_feature(FFMPEG_FOUND "libavcodec/libavutil/libavformat" "ReplayGain calculation." "http://ffmpeg.org")
    endif (ENABLE_FFMPEG)
    if (ENABLE_MPG123)
        find_package(MPG123)
        macro_log_feature(MPG123_FOUND "libmpg123" "ReplayGain calculation." "http://www.mpg123.de")
    endif (ENABLE_MPG123)
    if (ENABLE_MTP AND ENABLE_DEVICES_SUPPORT AND NOT WIN32 AND NOT APPLE)
        find_package(Mtp)
        macro_log_feature(MTP_FOUND "libmtp" "MTP Device Support." "http://libmtp.sourceforge.net")
    endif (ENABLE_MTP AND ENABLE_DEVICES_SUPPORT AND NOT WIN32 AND NOT APPLE)
else (TAGLIB_FOUND)
   set(ENABLE_DEVICES_SUPPORT OFF)
   set(ENABLE_EXTERNAL_TAGS OFF)
endif (TAGLIB_FOUND)

if (NOT MTP_FOUND)
   set(ENABLE_UNCACHED_MTP OFF)
endif (NOT MTP_FOUND)

if (ENABLE_QT5)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5Xml REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(Qt5Concurrent REQUIRED)
    find_package(Qt5Svg REQUIRED)
    find_package(Qt5Sql REQUIRED)

    set(QTCORELIBS ${Qt5Core_LIBRARIES})
    set(QTNETWORKLIBS ${Qt5Network_LIBRARIES})
    set(QTGUILIBS ${Qt5Gui_LIBRARIES})
    set(QTLIBS ${QTCORELIBS} ${Qt5Widgets_LIBRARIES} ${QTNETWORKLIBS} ${QTGUILIBS} ${Qt5Xml_LIBRARIES} ${Qt5Concurrent_LIBRARIES} ${Qt5Svg_LIBRARIES} ${Qt5Sql_LIBRARIES})
    set(QTINCLUDES ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${Qt5Xml_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS}
                   ${Qt5Svg_INCLUDE_DIRS} ${Qt5Sql_INCLUDE_DIRS})
    add_definitions(${Qt5Widgets_DEFINITIONS} ${Qt5Network_DEFINITIONS} ${Qt5Xml_DEFINITIONS} ${Qt5Concurrent_DEFINITIONS} ${Qt5Svg_DEFINITIONS} ${Qt5Sql_DEFINITIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
    if (NOT ENABLE_UBUNTU AND NOT APPLE AND NOT WIN32)
        # Does Ubuntu/Touch version need DBus?
        find_package(Qt5DBus)
        macro_log_feature(Qt5DBus_FOUND "Qt5DBus" "DBus support." "http://qtproject.org")
            if (Qt5DBus_FOUND)
            set(QT_QTDBUS_FOUND 1) # required for config.h !!!
            set(QTLIBS ${QTLIBS} ${Qt5DBus_LIBRARIES})
            set(QTINCLUDES ${QTINCLUDES} ${Qt5DBus_INCLUDE_DIRS})
            add_definitions(${Qt5DBus_DEFINITIONS})
        endif (Qt5DBus_FOUND)
    endif (NOT ENABLE_UBUNTU AND NOT APPLE AND NOT WIN32)
    if (ENABLE_UBUNTU)
        find_package(Qt5Quick REQUIRED)
        find_package(Qt5Qml REQUIRED)
    endif (ENABLE_UBUNTU)
    if (APPLE OR WIN32)
        get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
        exec_program(${QT_QMAKE_EXECUTABLE} ARGS -query QT_INSTALL_TRANSLATIONS OUTPUT_VARIABLE QT_TRANSLATIONS_DIR)
    endif (APPLE OR WIN32)
else (ENABLE_QT5)
    find_package(Qt4 REQUIRED QtCore QtGui QtXml QtNetwork QtSvg QtSql)
    set(QTCORELIBS ${QT_QTCORE_LIBRARY})
    set(QTNETWORKLIBS ${QT_QTNETWORK_LIBRARY})
    set(QTGUILIBS ${QT_QTGUI_LIBRARY})
    set(QTLIBS ${QT_QTXML_LIBRARY} ${QTCORELIBS} ${QTGUILIBS} ${QTNETWORKLIBS} ${QT_QTSVG_LIBRARY} ${QT_QTSQL_LIBRARY})
    if (QT_QTDBUS_FOUND)
        set(QTLIBS ${QTLIBS} ${QT_QTDBUS_LIBRARY})
    endif (QT_QTDBUS_FOUND)
    set(QTINCLUDES ${QT_INCLUDES})
    add_definitions(-DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra)
endif (ENABLE_QT5)

if (ENABLE_HTTP_STREAM_PLAYBACK)
    if (ENABLE_LIBVLC)
        find_package(LIBVLC REQUIRED)
        macro_log_feature(LIBVLC_FOUND "libVLC" "MPD HTTP stream playback." "http://videolan.org")
        add_definitions(-DLIBVLC_FOUND)
        include_directories(${LIBVLC_INCLUDE_DIR})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${LIBVLC_LIBRARY})
    elseif (ENABLE_QT5)
        find_package(Qt5Multimedia REQUIRED)
        macro_log_feature(Qt5Multimedia_FOUND "Qt5Multimedia" "MPD HTTP stream playback." "http://qtproject.org")
        include_directories(${Qt5Multimedia_INCLUDE_DIRS})
        add_definitions(${Qt5Multimedia_DEFINITIONS})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${Qt5Multimedia_LIBRARIES})
    else (ENABLE_LIBVLC)
        find_package(Phonon REQUIRED)
        macro_log_feature(PHONON_FOUND "Phonon" "MPD HTTP stream playback." "http://phonon.kde.org")
        include_directories(${PHONON_INCLUDES})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${PHONON_LIBS})
    endif (ENABLE_LIBVLC)
    set(CANTATA_SRCS ${CANTATA_SRCS} mpd-interface/httpstream.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} mpd-interface/httpstream.h)
endif (ENABLE_HTTP_STREAM_PLAYBACK)

if (ENABLE_KDE)
    find_package(KDE4 REQUIRED)
    add_definitions(-DENABLE_KDE_SUPPORT)
    include_directories(${KDE4_INCLUDES})
    add_definitions(${KDE4_DEFINITIONS})
    set(ENABLE_KDE_SUPPORT TRUE)
endif (ENABLE_KDE)

find_package(ZLIB REQUIRED)

if (APPLE)
    set(CANTATA_SRCS ${CANTATA_SRCS} mac/dockmenu.cpp mac/macnotify.mm)
    find_package(IOKit)
    if (IOKIT_FOUND)
        set(CANTATA_LIBS ${CANTATA_LIBS} ${IOKIT_LIBRARY})
        set(CANTATA_SRCS ${CANTATA_SRCS} mac/powermanagement.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} mac/powermanagement.h)
    endif (IOKIT_FOUND)
    find_package(Foundation REQUIRED)
    set(CANTATA_LIBS ${CANTATA_LIBS} ${FOUNDATION_LIBRARY})
    find_package(Qt5MacExtras)
    if (Qt5MacExtras_FOUND)
        set(QTINCLUDES ${QTINCLUDES} ${Qt5MacExtras_INCLUDE_DIRS})
        set(QTLIBS ${QTLIBS} ${Qt5MacExtras_LIBRARIES})
        add_definitions(${Qt5MacExtras_DEFINITIONS})
        set(QT_MAC_EXTRAS_FOUND 1)
    endif (Qt5MacExtras_FOUND)
elseif (WIN32 AND ENABLE_QT5)
    find_package(Qt5WinExtras REQUIRED)
    set(QTINCLUDES ${QTINCLUDES} ${Qt5WinExtras_INCLUDE_DIRS})
    set(QTLIBS ${QTLIBS} ${Qt5WinExtras_LIBRARIES})
    add_definitions(${Qt5WinExtras_DEFINITIONS})
    set(CANTATA_SRCS ${CANTATA_SRCS} windows/thumbnailtoolbar.cpp)
endif (APPLE)
# For Qt5, we use Qt's own QJSonDocument class, so no need to find libqjson
if (NOT ENABLE_QT5 AND NOT WIN32 AND NOT APPLE)
    find_package(QJSON)
endif (NOT ENABLE_QT5 AND NOT WIN32 AND NOT APPLE)

if (QJSON_FOUND)
    message("-- Using system libqjson")
    include_directories(${QJSON_INCLUDE_DIR})
    set(CANTATA_LIBS ${CANTATA_LIBS} ${QJSON_LIBRARIES})
elseif (NOT ENABLE_QT5)
    message("-- Using supplied libqjson")
    add_subdirectory(3rdparty/qjson)
    set(CANTATA_LIBS ${CANTATA_LIBS} qjson)
endif (QJSON_FOUND)

# QJSon and MusicBrainz require exceptions to be enabled!
if (MUSICBRAINZ5_FOUND OR NOT ENABLE_QT5)
    message("-- Enabling exceptions")
    if (ENABLE_KDE)
        add_definitions(${KDE4_ENABLE_EXCEPTIONS})
    else (ENABLE_KDE)
        if (CMAKE_COMPILER_IS_GNUCXX)
            add_definitions("-fexceptions -UQT_NO_EXCEPTIONS")
        endif (CMAKE_COMPILER_IS_GNUCXX)
        if (CMAKE_C_COMPILER MATCHES "icc")
            add_definitions(-fexceptions)
        endif (CMAKE_C_COMPILER MATCHES "icc")
        if (MSVC)
            add_definitions(-EHsc)
        endif (MSVC)
    endif (ENABLE_KDE)
endif (MUSICBRAINZ5_FOUND OR NOT ENABLE_QT5)

include_directories(${CMAKE_SOURCE_DIR}/3rdparty ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})

set(CANTATA_CORE_SRCS ${CANTATA_CORE_SRCS}
    gui/covers.cpp gui/currentcover.cpp
    models/musiclibraryitemroot.cpp models/musiclibraryitemartist.cpp models/musiclibraryitemalbum.cpp
    models/musiclibraryproxymodel.cpp models/playlistsmodel.cpp models/playlistsproxymodel.cpp models/playqueuemodel.cpp
    models/proxymodel.cpp models/actionmodel.cpp models/musiclibraryitem.cpp models/browsemodel.cpp
    models/searchmodel.cpp models/streamsmodel.cpp models/searchproxymodel.cpp models/musiclibraryitemsong.cpp
    models/sqllibrarymodel.cpp models/mpdlibrarymodel.cpp models/mpdsearchmodel.cpp models/playqueueproxymodel.cpp
    mpd-interface/mpdconnection.cpp mpd-interface/mpdparseutils.cpp mpd-interface/mpdstats.cpp mpd-interface/mpdstatus.cpp
    mpd-interface/song.cpp mpd-interface/cuefile.cpp
    db/librarydb.cpp db/mpdlibrarydb.cpp
    network/networkaccessmanager.cpp network/networkproxyfactory.cpp
    streams/streamfetcher.cpp
    http/httpserver.cpp)
set(CANTATA_CORE_MOC_HDRS ${CANTATA_CORE_MOC_HDRS}
    gui/covers.h gui/currentcover.h
    models/musiclibraryproxymodel.h models/playlistsmodel.h models/playlistsproxymodel.h models/playqueuemodel.h
    models/playqueueproxymodel.h models/actionmodel.h models/browsemodel.h
    models/searchmodel.h models/sqllibrarymodel.h models/mpdlibrarymodel.h models/mpdsearchmodel.h
    mpd-interface/mpdconnection.h mpd-interface/mpdstats.h mpd-interface/mpdstatus.h
    db/librarydb.h db/mpdlibrarydb.h
    network/networkaccessmanager.h
    streams/streamfetcher.h
    widgets/notelabel.h)

set(CANTATA_SRCS ${CANTATA_CORE_SRCS} ${CANTATA_SRCS}
    gui/settings.cpp gui/application.cpp gui/initialsettingswizard.cpp gui/mainwindow.cpp gui/preferencesdialog.cpp gui/customactionssettings.cpp
    gui/filesettings.cpp gui/interfacesettings.cpp gui/playbacksettings.cpp gui/serversettings.cpp gui/librarypage.cpp gui/customactions.cpp
    gui/folderpage.cpp gui/playlistspage.cpp gui/trayitem.cpp gui/cachesettings.cpp gui/coverdialog.cpp gui/searchpage.cpp gui/stdactions.cpp
    devices/deviceoptions.cpp
    widgets/treeview.cpp widgets/listview.cpp widgets/itemview.cpp widgets/autohidingsplitter.cpp widgets/nowplayingwidget.cpp
    widgets/actionlabel.cpp widgets/playqueueview.cpp widgets/groupedview.cpp widgets/actionitemdelegate.cpp widgets/textbrowser.cpp
    widgets/volumeslider.cpp widgets/menubutton.cpp widgets/icons.cpp widgets/toolbutton.cpp widgets/wizardpage.cpp
    widgets/searchwidget.cpp widgets/messageoverlay.cpp widgets/basicitemdelegate.cpp widgets/sizegrip.cpp
    widgets/sizewidget.cpp widgets/servicestatuslabel.cpp widgets/spacerwidget.cpp widgets/songdialog.cpp widgets/stretchheaderview.cpp
    widgets/tableview.cpp widgets/thinsplitterhandle.cpp widgets/coverwidget.cpp widgets/ratingwidget.cpp widgets/notelabel.cpp
    widgets/selectorlabel.cpp widgets/titlewidget.cpp widgets/multipagewidget.cpp widgets/singlepagewidget.cpp widgets/stackedpagewidget.cpp
    widgets/mirrormenu.cpp
    context/lyricsettings.cpp context/ultimatelyricsprovider.cpp context/ultimatelyrics.cpp context/lyricsdialog.cpp
    context/contextwidget.cpp context/view.cpp context/artistview.cpp context/albumview.cpp context/songview.cpp context/contextengine.cpp
    context/wikipediaengine.cpp context/wikipediasettings.cpp context/othersettings.cpp context/contextsettings.cpp context/togglelist.cpp
    context/lastfmengine.cpp context/metaengine.cpp context/onlineview.cpp
    streams/streamspage.cpp streams/digitallyimportedsettings.cpp streams/streamssettings.cpp streams/streamdialog.cpp streams/tar.cpp
    streams/streamproviderlistdialog.cpp
    models/streamsproxymodel.cpp models/streamsearchmodel.cpp models/digitallyimported.cpp
    dynamic/dynamic.cpp dynamic/dynamicproxymodel.cpp dynamic/dynamicpage.cpp dynamic/dynamicruledialog.cpp dynamic/dynamicrulesdialog.cpp
    online/onlineservicespage.cpp online/onlinedbservice.cpp online/jamendoservice.cpp online/onlinedbwidget.cpp online/onlineservice.cpp
    online/jamendosettingsdialog.cpp online/magnatuneservice.cpp online/magnatunesettingsdialog.cpp online/soundcloudservice.cpp
    online/soundcloudwidget.cpp online/podcastservice.cpp online/rssparser.cpp online/opmlparser.cpp online/podcastsearchdialog.cpp
    online/podcastsettingsdialog.cpp online/podcastwidget.cpp
    db/onlinedb.cpp
    scrobbling/scrobbler.cpp scrobbling/pausabletimer.cpp scrobbling/scrobblingsettings.cpp scrobbling/scrobblingstatus.cpp
    scrobbling/scrobblinglove.cpp)
set(CANTATA_MOC_HDRS ${CANTATA_CORE_MOC_HDRS} ${CANTATA_MOC_HDRS}
    gui/initialsettingswizard.h gui/mainwindow.h gui/folderpage.h gui/librarypage.h gui/playlistspage.h
    gui/playbacksettings.h gui/serversettings.h gui/preferencesdialog.h gui/interfacesettings.h gui/cachesettings.h gui/trayitem.h
    gui/coverdialog.h gui/searchpage.h gui/customactions.h gui/customactionssettings.h
    widgets/treeview.h widgets/listview.h widgets/itemview.h widgets/autohidingsplitter.h widgets/nowplayingwidget.h widgets/actionlabel.h
    widgets/playqueueview.h widgets/groupedview.h widgets/actionitemdelegate.h widgets/volumeslider.h widgets/singlepagewidget.h
    widgets/searchwidget.h widgets/messageoverlay.h widgets/servicestatuslabel.h widgets/stretchheaderview.h widgets/tableview.h
    widgets/coverwidget.h widgets/ratingwidget.h widgets/selectorlabel.h widgets/titlewidget.h widgets/multipagewidget.h
    widgets/stackedpagewidget.h widgets/mirrormenu.h
    context/togglelist.h context/ultimatelyrics.h context/ultimatelyricsprovider.h context/lyricsdialog.h
    context/contextwidget.h context/artistview.h context/albumview.h context/songview.h context/view.h context/contextengine.h
    context/wikipediaengine.h context/wikipediasettings.h context/othersettings.h context/lastfmengine.h context/metaengine.h
    streams/streamspage.h streams/digitallyimportedsettings.h streams/streamssettings.h
    streams/streamdialog.h models/streamsmodel.h streams/streamproviderlistdialog.h
    models/digitallyimported.h models/streamsearchmodel.h
    online/onlineservicespage.h online/onlinedbservice.h online/onlinedbwidget.h online/magnatunesettingsdialog.h
    online/soundcloudservice.h online/soundcloudwidget.h online/podcastservice.h online/podcastsearchdialog.h
    online/podcastsettingsdialog.h online/podcastwidget.h
    db/onlinedb.h
    dynamic/dynamic.h dynamic/dynamicruledialog.h dynamic/dynamicpage.h dynamic/dynamicrulesdialog.h
    scrobbling/scrobbler.h scrobbling/scrobblingsettings.h scrobbling/scrobblingstatus.h scrobbling/scrobblinglove.h)
set(CANTATA_UIS ${CANTATA_UIS}
    gui/initialsettingswizard.ui gui/mainwindow.ui
    gui/filesettings.ui gui/interfacesettings.ui gui/playbacksettings.ui gui/serversettings.ui gui/coverdialog.ui
    context/togglelist.ui context/othersettings.ui
    streams/digitallyimportedsettings.ui streams/streamssettings.ui
    dynamic/dynamicrule.ui dynamic/dynamicrules.ui
    widgets/itemview.ui scrobbling/scrobblingsettings.ui)

if (ENABLE_SIMPLE_MPD_SUPPORT)
     set(CANTATA_SRCS ${CANTATA_CORE_SRCS} ${CANTATA_SRCS} mpd-interface/mpduser.cpp)
endif (ENABLE_SIMPLE_MPD_SUPPORT)

if (ENABLE_UBUNTU)
    set(UBUNTU_SRCS ${CANTATA_CORE_SRCS} ubuntu/main.cpp ubuntu/backend/mpdbackend.cpp gui/plurals_qt.cpp)
    set(UBUNTU_MOC_HDRS ${CANTATA_CORE_MOC_HDRS} ubuntu/backend/mpdbackend.h)
else (ENABLE_UBUNTU)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/main.cpp)
    set(CANTATA_LIBS ${CANTATA_LIBS} support)
    add_subdirectory(icons)
endif (ENABLE_UBUNTU)


if (WIN32)
    # Not installed for windows - script uses sym-links...
elseif (APPLE)
    install(PROGRAMS dynamic/cantata-dynamic DESTINATION ${MACOSX_BUNDLE_RESOURCES}/scripts/)
elseif (NOT ENABLE_UBUNTU)
    install(PROGRAMS dynamic/cantata-dynamic DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/scripts)
endif (WIN32)

if (ENABLE_HTTP_SERVER)
    set(CANTATA_SRCS ${CANTATA_SRCS} http/httpsocket.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} http/httpserver.h http/httpsocket.h)
endif (ENABLE_HTTP_SERVER)

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
        qt5_add_dbus_adaptor(CANTATA_SRCS dbus/${PROJECT_REV_URL}.xml gui/mainwindow.h MainWindow)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.login1.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.gnome.SettingsDaemon.MediaKeys.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.kde.Solid.PowerManagement.PolicyAgent.xml)
        qt5_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.PowerManagement.Inhibit.xml)
    else (ENABLE_QT5)
        if (USE_OLD_DBUS_TYPEDEF OR ((QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) ))
            qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.OLD.xml dbus/mpris.h Mpris)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.OLD.xml)
        else (USE_OLD_DBUS_TYPEDEF OR ((QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) ))
            qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.Notifications.xml)
        endif (USE_OLD_DBUS_TYPEDEF OR ((QT_VERSION_MINOR LESS 8) OR (QT_VERSION_MINOR EQUAL 8 AND QT_VERSION_PATCH LESS 2) ))
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
        qt4_add_dbus_adaptor(CANTATA_SRCS dbus/${PROJECT_REV_URL}.xml gui/mainwindow.h MainWindow)

        if (NOT ENABLE_KDE)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.UPower.xml)
            qt4_add_dbus_interfaces(CANTATA_SRCS dbus/org.freedesktop.login1.xml)
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

set(CANTATA_RCS cantata.qrc cantata_media.qrc)
if (USE_SYSTEM_MENU_ICON AND NOT WIN32)
    set(CANTATA_RCS ${CANTATA_RCS} cantata_menu.qrc)
endif (USE_SYSTEM_MENU_ICON AND NOT WIN32)

if (TAGLIB_FOUND)
    set(CANTATA_SRCS ${CANTATA_SRCS}  tags/tageditor.cpp tags/trackorganiser.cpp devices/filenameschemedialog.cpp widgets/tagspinbox.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} tags/tageditor.h tags/trackorganiser.h devices/filenameschemedialog.h devices/device.h)
    set(CANTATA_UIS ${CANTATA_UIS} tags/tageditor.ui tags/trackorganiser.ui devices/filenameschemedialog.ui)

    # Cantata still links to taglib, even if external tag reader/writer is used, because JamendoService uses taglib for ID3 genres.
    set(CANTATA_LIBS ${CANTATA_LIBS} ${TAGLIB_LIBRARIES})
    include_directories(${TAGLIB_INCLUDES})
    if (APPLE OR OPENBSD)
        # for some reason under osx we get full path no include/taglib?
        include_directories(${TAGLIB_INCLUDES}/..)
    endif (APPLE OR OPENBSD)
    if (NOT ENABLE_EXTERNAL_TAGS AND TAGLIB-EXTRAS_FOUND)     
        set(CANTATA_LIBS ${CANTATA_LIBS} ${TAGLIB-EXTRAS_LIBRARIES})
        include_directories(${TAGLIB-EXTRAS_INCLUDES})
    endif (NOT ENABLE_EXTERNAL_TAGS AND TAGLIB-EXTRAS_FOUND)
    if (NOT ENABLE_KDE AND ENABLE_DEVICES_SUPPORT)
        set(CANTATA_LIBS ${CANTATA_LIBS} solidlite)
    endif (NOT ENABLE_KDE AND ENABLE_DEVICES_SUPPORT)
    if (ENABLE_DEVICES_SUPPORT AND ENABLE_REMOTE_DEVICES AND NOT WIN32 AND NOT APPLE)
        set(CANTATA_LIBS ${CANTATA_LIBS} avahi)
    endif (ENABLE_DEVICES_SUPPORT AND ENABLE_REMOTE_DEVICES AND NOT WIN32 AND NOT APPLE)

    if (FFMPEG_FOUND OR MPG123_FOUND)
        set(CANTATA_SRCS ${CANTATA_SRCS} replaygain/albumscanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp replaygain/jobcontroller.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} replaygain/albumscanner.h replaygain/rgdialog.h replaygain/tagreader.h replaygain/jobcontroller.h)
        set(ENABLE_REPLAYGAIN_SUPPORT 1)
        add_subdirectory(replaygain)
    endif (FFMPEG_FOUND OR MPG123_FOUND)
    if (ENABLE_EXTERNAL_TAGS)
        set(CANTATA_SRCS ${CANTATA_SRCS} tags/taghelperiface.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} tags/taghelperiface.h)
        add_subdirectory(tags)
    else (ENABLE_EXTERNAL_TAGS)
        set(CANTATA_SRCS ${CANTATA_SRCS} tags/tags.cpp tags/filetyperesolver.cpp)
    endif (ENABLE_EXTERNAL_TAGS)

    if (ENABLE_DEVICES_SUPPORT)
        if (NOT ENABLE_KDE)
            add_subdirectory(3rdparty/solid-lite)
        endif (NOT ENABLE_KDE)
        if (MTP_FOUND)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mtpdevice.h)
            include_directories(${MTP_INCLUDE_DIR})
            set(CANTATA_LIBS ${CANTATA_LIBS} ${MTP_LIBRARIES})
        endif (MTP_FOUND)
        if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
            if (CDDB_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/cddbinterface.cpp)
                set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/cddbinterface.h)
                include_directories(${CDDB_INCLUDE_DIR})
                set(CANTATA_LIBS ${CANTATA_LIBS} ${CDDB_LIBS})
            endif (CDDB_FOUND)
            if (MUSICBRAINZ5_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/musicbrainz.cpp)
                set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/musicbrainz.h)
                include_directories(${MUSICBRAINZ5_INCLUDE_DIRS})
                set(CANTATA_LIBS ${CANTATA_LIBS} ${MUSICBRAINZ5_LIBRARIES})
            endif (MUSICBRAINZ5_FOUND)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/audiocddevice.cpp devices/cddbselectiondialog.cpp
                 devices/cdparanoia.cpp devices/audiocdsettings.cpp devices/extractjob.cpp devices/albumdetailsdialog.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/audiocddevice.h devices/extractjob.h
                 devices/albumdetailsdialog.h devices/cddbselectiondialog.h)
            set(CANTATA_UIS ${CANTATA_UIS} devices/albumdetails.ui devices/audiocdsettings.ui)
            # If CDDB/MusicBrainz5 found - then CDParanoia must have been!
            set(CANTATA_LIBS ${CANTATA_LIBS} ${CDPARANOIA_LIBRARIES})
            include_directories(${CDPARANOIA_INCLUDE_DIR})
            if (ENABLE_KDE)
                install(FILES cantata-play-audiocd.desktop DESTINATION ${DATA_INSTALL_DIR}/solid/actions)
            endif (ENABLE_KDE)
        endif (CDDB_FOUND OR MUSICBRAINZ5_FOUND)

        set(CANTATA_SRCS ${CANTATA_SRCS} devices/devicespage.cpp devices/filejob.cpp
                devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp devices/splitlabelwidget.cpp
                models/devicesmodel.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
                devices/devicepropertiesdialog.cpp devices/encoders.cpp devices/freespaceinfo.cpp
                devices/transcodingjob.cpp devices/valueslider.cpp devices/syncdialog.cpp
                devices/synccollectionwidget.cpp online/onlinedevice.cpp models/musiclibrarymodel.cpp)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/devicespage.h devices/filejob.h
                devices/fsdevice.h devices/umsdevice.h models/devicesmodel.h
                devices/actiondialog.h devices/devicepropertieswidget.h devices/devicepropertiesdialog.h
                devices/transcodingjob.h devices/valueslider.h devices/syncdialog.h
                devices/synccollectionwidget.h online/onlinedevice.h)
        set(CANTATA_UIS ${CANTATA_UIS} devices/actiondialog.ui devices/devicepropertieswidget.ui
                devices/synccollectionwidget.ui)

        if (ENABLE_REMOTE_DEVICES)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/remotefsdevice.cpp devices/remotedevicepropertiesdialog.cpp
                devices/remotedevicepropertieswidget.cpp)
            set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/remotefsdevice.h devices/remotedevicepropertiesdialog.h
                devices/remotedevicepropertieswidget.h)
            set(CANTATA_UIS ${CANTATA_UIS} devices/remotedevicepropertieswidget.ui)
            if (ENABLE_QT5)
                qt5_add_dbus_interfaces(CANTATA_SRCS devices/mounter/${PROJECT_REV_URL}.mounter.xml)
            else (ENABLE_QT5)
                qt4_add_dbus_interfaces(CANTATA_SRCS devices/mounter/${PROJECT_REV_URL}.mounter.xml)
            endif (ENABLE_QT5)
            add_subdirectory(devices/mounter)
            add_subdirectory(devices/avahi)
        endif (ENABLE_REMOTE_DEVICES)
    else (ENABLE_DEVICES_SUPPORT)
        set(CANTATA_SRCS ${CANTATA_SRCS} devices/device.cpp)
    endif (ENABLE_DEVICES_SUPPORT)
else (TAGLIB_FOUND)
    set(ENABLE_DEVICES_SUPPORT OFF)
    set(ENABLE_REMOTE_DEVICES OFF)
endif (TAGLIB_FOUND)

if (ENABLE_PROXY_CONFIG)
    set(CANTATA_SRCS ${CANTATA_SRCS} network/proxysettings.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} network/proxysettings.h)
    set(CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui)
endif (ENABLE_PROXY_CONFIG)

if (ENABLE_KDE)
    qt4_add_resources(CANTATA_RC_SRCS ${CANTATA_RCS})
    kde4_add_ui_files(CANTATA_UI_HDRS ${CANTATA_UIS})
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/plurals_kde.cpp gui/application_kde.cpp)
    kde4_add_executable(cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
    target_link_libraries(cantata ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_SOLID_LIBS} ${KDE4_KIO_LIBRARY})
    if (NOT WIN32 AND NOT APPLE AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
        target_link_libraries(cantata ${KDE4_KFILE_LIBS})
    endif (NOT WIN32 AND NOT APPLE AND TAGLIB_FOUND AND ENABLE_REMOTE_DEVICES)
    install(TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS})
elseif (ENABLE_UBUNTU)
    set(CANTATA_RCS ubuntu/ubuntu.qrc)
    QT5_ADD_RESOURCES(CANTATA_RC_SRCS ${CANTATA_RCS})
    qt5_wrap_cpp(UBUNTU_MOC_SRCS ${UBUNTU_MOC_HDRS})
    add_executable(cantata ${CMAKE_CURRENT_SOURCE_DIR}/ubuntu ${UBUNTU_SRCS} ${UBUNTU_MOC_SRCS} ${CANTATA_RC_SRCS})
    qt5_use_modules(cantata Network Core Qml Quick Gui)
else (ENABLE_KDE)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/plurals_qt.cpp)
    if (WIN32 OR APPLE)
        set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/singleapplication.h)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/singleapplication.cpp)
    endif (WIN32 OR APPLE)
    if (WIN32)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_win.cpp)
    elseif (APPLE)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_mac.cpp)
    else (WIN32)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_qt.cpp)
    endif (WIN32)
    if (WIN32)
        add_definitions(-DWIN32)
        add_subdirectory(windows)
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
    elseif (APPLE)
        add_definitions(-D__APPLE__)
    endif (WIN32)

    set(CANTATA_SRCS ${CANTATA_SRCS} gui/shortcutssettingspage.cpp gui/mediakeys.cpp)
    set(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} gui/multimediakeysinterface.h)

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

    if (WIN32 OR (NOT APPLE AND NOT ENABLE_QT5))
        add_definitions(-DQXT_STATIC)
        add_subdirectory(3rdparty/qxt)
        set(CANTATA_SRCS ${CANTATA_SRCS} gui/qxtmediakeys.cpp)
    endif (WIN32 OR (NOT APPLE AND NOT ENABLE_QT5))

    if (WIN32)
        set(CMAKE_BUILD_TYPE "Release")
        ADD_EXECUTABLE(cantata WIN32 ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
        install(TARGETS cantata DESTINATION ${CMAKE_INSTALL_PREFIX})
    elseif (APPLE)
        ADD_EXECUTABLE(cantata MACOSX_BUNDLE ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
        add_subdirectory(mac)
        include(DeployQt5)
        install(PROGRAMS ${CMAKE_BINARY_DIR}/cantata.app/Contents/MacOS/cantata DESTINATION ${MACOSX_BUNDLE_APP_DIR})
        # Create our own plist file to enable HighDPI support
        configure_file(mac/Info.plist.cmake ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist)
        configure_file(mac/dmg/create-dmg.sh.in ${CMAKE_CURRENT_BINARY_DIR}/mac/create-dmg.sh)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mac/Info.plist DESTINATION ${CANTATA_APP_CONTENTS_DIR})
        if (TAGLIB_FOUND)
            # *NEED* to install cantata-replagain/cantata-tags *before* install_qt5_executable fix'es the bundle
            # If we install from their own CMakeLists.txt then they are installed *after* the fix-up, and so link
            #  to the build libs not the packaged ones!
            if (FFMPEG_FOUND OR MPG123_FOUND)
                install(PROGRAMS ${CMAKE_BINARY_DIR}/replaygain/cantata-replaygain DESTINATION ${MACOSX_BUNDLE_APP_DIR})
            endif (FFMPEG_FOUND OR MPG123_FOUND)
            if (ENABLE_EXTERNAL_TAGS)
                install(PROGRAMS ${CMAKE_BINARY_DIR}/tags/cantata-tags DESTINATION ${MACOSX_BUNDLE_APP_DIR})
            endif (ENABLE_EXTERNAL_TAGS)
        endif (TAGLIB_FOUND)
        if (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
            install_qt5_executable(cantata.app "qjpeg;qsvg;qsvgicon;qcocoa;qsqlite;qavfmediaplayer")
        else (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
            install_qt5_executable(cantata.app "qjpeg;qsvg;qsvgicon;qcocoa;qsqlite")
        endif (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
    elseif (NOT ENABLE_UBUNTU)
        ADD_EXECUTABLE(cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
        install(TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    endif (WIN32)

    if (NOT ENABLE_UBUNTU)
        set(XDG_APPS_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/applications")
    endif (NOT ENABLE_UBUNTU)
    if (WIN32 OR APPLE)
        add_subdirectory(3rdparty/qtsingleapplication)
        target_link_libraries(cantata qtsingleapplication)
    endif (WIN32 OR APPLE)
    if (WIN32 OR (NOT APPLE AND NOT ENABLE_QT5))
        target_link_libraries(cantata qxt)
    endif (WIN32 OR (NOT APPLE AND NOT ENABLE_QT5))
endif (ENABLE_KDE)

if (WIN32)
    install(FILES tags/tag_fixes.xml context/lyrics_providers.xml context/weblinks.xml online/podcast_directories.xml scrobbling/scrobblers.xml
            DESTINATION ${CMAKE_INSTALL_PREFIX}/config/)
elseif (APPLE)
    install(FILES tags/tag_fixes.xml context/lyrics_providers.xml context/weblinks.xml online/podcast_directories.xml scrobbling/scrobblers.xml
            DESTINATION ${MACOSX_BUNDLE_RESOURCES}/config/)
elseif (NOT ENABLE_UBUNTU)
    install(FILES tags/tag_fixes.xml context/lyrics_providers.xml context/weblinks.xml online/podcast_directories.xml scrobbling/scrobblers.xml
            DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/config/)
        install(FILES mpd-interface/mpd.conf.template DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/mpd)
endif (WIN32)

if (NOT ENABLE_UBUNTU)
    add_subdirectory(po)
endif (NOT ENABLE_UBUNTU)

add_subdirectory(support)
add_subdirectory(3rdparty/qtiocompressor)
add_subdirectory(streams/icons)
add_subdirectory(online/icons)
target_link_libraries(cantata support-core qtiocompressor ${CANTATA_LIBS} ${QTLIBS} ${ZLIB_LIBRARIES})

# enable warnings
add_definitions(-DQT_NO_DEBUG_OUTPUT)

if (UNIX AND NOT APPLE)
    if (NOT ENABLE_UBUNTU)
        configure_file(cantata-remote.cmake ${CMAKE_BINARY_DIR}/cantata-remote)
        configure_file(cantata.desktop.cmake ${CMAKE_BINARY_DIR}/cantata.desktop)
        install(PROGRAMS ${CMAKE_BINARY_DIR}/cantata-remote DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/scripts)
        install(FILES ${CMAKE_BINARY_DIR}/cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
    endif (NOT ENABLE_UBUNTU)
    target_link_libraries(cantata -lpthread)
endif (UNIX AND NOT APPLE)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

if (NOT WIN32 AND NOT APPLE AND NOT ENABLE_UBUNTU AND NOT ENABLE_KDE)
    # uninstall target
    configure_file("${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
    configure_file(install_dirs.cmake ${CMAKE_BINARY_DIR}/install_dirs)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)
endif (NOT WIN32 AND NOT APPLE AND NOT ENABLE_UBUNTU AND NOT ENABLE_KDE)

if (ENABLE_KDE)
    message("-- Building KDE version")
elseif (ENABLE_UBUNTU)
    message("-- Building Ubuntu version")
elseif (ENABLE_QT5)
    message("-- Building Qt5 version")
else (ENABLE_KDE)
    message("-- Building Qt4 version")
endif (ENABLE_KDE)

macro_display_feature_log()

if (ENABLE_UBUNTU)
    set(UBUNTU_PROJECT_TYPE "ClickApp" CACHE INTERNAL "Tells QtCreator this is a Click application project")

    add_custom_target(components_QmlFiles ALL SOURCES 
        ubuntu/qml/cantata/main.qml ubuntu/qml/cantata/AboutPage.qml ubuntu/qml/cantata/ListViewPage.qml
        ubuntu/qml/cantata/SubListViewPage.qml ubuntu/qml/cantata/CurrentlyPlayingPage.qml
        ubuntu/qml/cantata/settings/SettingsPage.qml ubuntu/qml/cantata/settings/SettingsCategories.qml
        ubuntu/qml/cantata/settings/HostSettingsContent.qml ubuntu/qml/cantata/settings/HostSettingsPage.qml
        ubuntu/qml/cantata/settings/UiSettingsContent.qml ubuntu/qml/cantata/settings/UiSettingsPage.qml
        ubuntu/qml/cantata/settings/PlaybackSettingsContent.qml ubuntu/qml/cantata/settings/PlaybackSettingsPage.qml
        ubuntu/qml/cantata/components/ControlButtonsRow.qml ubuntu/qml/cantata/components/ListItemDelegate.qml
        ubuntu/qml/cantata/components/PlayQueueListItemDelegate.qml ubuntu/qml/cantata/components/LabelVisual.qml
        ubuntu/qml/cantata/components/Notification.qml ubuntu/qml/cantata/backend/SettingsBackend.qml
        ubuntu/qml/cantata/components/PageWithBottomEdge.qml ubuntu/qml/cantata/CurrentlyPlayingContent.qml
    )
    add_custom_target(click_files ALL SOURCES
        ubuntu/manifest.json
        ubuntu/cantata-ubuntu.json
        ubuntu/cantata-ubuntu.desktop
    )
    add_custom_target(images ALL SOURCES #TODO: Add other icons
         ubuntu/icons/desktop/cantata.png
    )

    install(TARGETS cantata RUNTIME DESTINATION .)
    install(FILES ubuntu/manifest.json ubuntu/cantata-ubuntu.json ubuntu/cantata-ubuntu.desktop ubuntu/icons/desktop/cantata.png DESTINATION .)

else (ENABLE_UBUNTU)
    if ((NOT ENABLE_DEVICES_SUPPORT AND NOT WIN32 AND NOT APPLE) OR (NOT ENABLE_HTTP_STREAM_PLAYBACK))
        message("-----------------------------------------------------------------------------")
        message("-- The following STANDARD options have been DISABLED.")
        message("-----------------------------------------------------------------------------")
        if (NOT ENABLE_DEVICES_SUPPORT)
            message("    * Devices Tab - Media device support (UMS, MTP, AudioCD).")
        endif (NOT ENABLE_DEVICES_SUPPORT)
        if (NOT ENABLE_HTTP_STREAM_PLAYBACK)
            message("    * MPD HTTP Streams - Playback MPD HTTP output stream via libVLC.")
        endif (NOT ENABLE_HTTP_STREAM_PLAYBACK)
        message("")
    endif ((NOT ENABLE_DEVICES_SUPPORT AND NOT WIN32 AND NOT APPLE) OR (NOT ENABLE_HTTP_STREAM_PLAYBACK))

    if ((ENABLE_DEVICES_SUPPORT AND ENABLE_REMOTE_DEVICES) OR (NOT ENABLE_KDE AND ENABLE_PROXY_CONFIG))
        message("-----------------------------------------------------------------------------")
        message("-- The following NON STANDARD options have been ENABLED.")
        message("-----------------------------------------------------------------------------")
        if (ENABLE_PROXY_CONFIG)
            message("    * Proxy Configuration")
        endif (ENABLE_PROXY_CONFIG)
        if (ENABLE_REMOTE_DEVICES)
            message("    * Remote Devices - Access sshfs, Samba, and local folders as devices. (EXPERIMENTAL)")
        endif (ENABLE_REMOTE_DEVICES)
        message("")
    endif ((ENABLE_DEVICES_SUPPORT AND ENABLE_REMOTE_DEVICES) OR (NOT ENABLE_KDE AND ENABLE_PROXY_CONFIG))

    if (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)
        message("-----------------------------------------------------------------------------")
        message("-- ENABLING MODEL TESTS!!! RUN UNDER A DEBUGGER, AND DO NOT USE AS RELEASE!!!")
        message("-----------------------------------------------------------------------------")
        message("")
    endif (ENABLE_MODEL_TEST AND NOT ENABLE_KDE AND NOT ENABLE_QT5)
endif (ENABLE_UBUNTU)
