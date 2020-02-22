PROJECT(cantata)

#  NOTE: If PROJECT_URL, or PROJECT_REV_URL, are changed, then cantata-dynamic, README, and
#        dbus/mpd.cantata.xml will need renaming/updating.
set(PROJECT_URL "cantata.mpd")
set(PROJECT_REV_URL "mpd.cantata")

if (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
endif ()

if (WIN32)
    # For some reason I chose mpd as the organization name for Windows builds. So, for config compatability, this is
    # left as mpd for now...
    set(ORGANIZATION_NAME "mpd")
else ()
    # Otherwise its set to the same as the project name. Not sure what else to set it to!!!
    set(ORGANIZATION_NAME ${CMAKE_PROJECT_NAME})
endif ()

cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0020 OLD)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TBZ2")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt Client for MPD")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "4")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_PACKAGE_VERSION_SPIN "")  # Use ".$number" - e.g. ".1"
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CANTATA_VERSION_WITH_SPIN "${CANTATA_VERSION_FULL}${CPACK_PACKAGE_VERSION_SPIN}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_WITH_SPIN}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_WITH_SPIN}")
set(CPACK_SOURCE_IGNORE_FILES  "/.svn/;/.git/;.gitignore;.github/;.project;CMakeLists.txt.user;README.md;/screenshots/")
include(CPack)
include(MacroLogFeature)

option(ENABLE_TAGLIB "Enable TagLib library (required for tag editing, replaygain calculation, device support, etc)" ON)
option(ENABLE_HTTP_STREAM_PLAYBACK "Enable playback of MPD HTTP streams (LibVLC or QtMultimedia)" ON)
option(ENABLE_FFMPEG "Enable ffmpeg/libav libraries (required for replaygain calculation)" ON)
option(ENABLE_MPG123 "Enable mpg123 libraries (required for replaygain calculation)" ON)
option(ENABLE_PROXY_CONFIG "Enable proxy config in settings dialog" ON)
option(ENABLE_HTTP_SERVER "Enable internal HTTP server to play non-MPD files" ON)
option(ENABLE_LIBVLC "Use libVLC for MPD HTTP stream playback (if ENABLE_HTTP_STREAM_PLAYBACK=ON)" OFF)
option(ENABLE_CATEGORIZED_VIEW "Enable support for categorised view" ON)

if (WIN32 OR APPLE OR HAIKU)
    option(ENABLE_DEVICES_SUPPORT "Enable suport for external devices" OFF)
else ()
    option(ENABLE_DEVICES_SUPPORT "Enable suport for external devices" ON)
    option(ENABLE_REMOTE_DEVICES "Enable support for remote (sshfs, samba) devices" ON)
    option(ENABLE_SIMPLE_MPD_SUPPORT "Enable support for simple (Cantata controlled) MPD" ON)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        option(ENABLE_UDISKS2 "Build UDisks2 backend, and NOT UDisks" ON)
    else()
        option(ENABLE_UDISKS2 "Build UDisks2 backend, and NOT UDisks" OFF)
    endif()
endif ()
option(ENABLE_CDPARANOIA "Enable CDParanoia libraries (this or libCDIO_paranoia required for AudioCD support)" ON)
option(ENABLE_CDIOPARANOIA "Enable libCDIO_paranoia libraries (this or CDParanoia required for AudioCD support)" ON)
option(ENABLE_CDDB "Enable CDDB libraries (either this or MusicBrianz required for AudioCD support)" ON)
option(ENABLE_MUSICBRAINZ "Enable MusicBrianz libraries (either this or CDDB required for AudioCD support)" ON)
option(ENABLE_MTP "Enable MTP library (required to support MTP devices)" ON)
option(ENABLE_AVAHI "Enable automatic mpd server discovery" ${UNIX})

# Build all apps into top-level folder, so that can run dev versions without install
if (NOT WIN32 AND NOT APPLE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

if(ENABLE_AVAHI)
    find_package(Avahi)
    macro_log_feature(AVAHI_FOUND "Avahi Support" "Automatic MPD-Server Detection" "https://www.avahi.org/" FALSE "" "")
endif()

if (AVAHI_FOUND)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/findmpddialog.cpp gui/avahidiscovery.cpp gui/avahipoll.cpp)
    set(CANTATA_UIS ${CANTATA_UIS} gui/findmpddialog.ui)

    set(CANTATA_LIBS ${CANTATA_LIBS} ${AVAHI_LIBRARIES})
endif()

if (NOT APPLE AND NOT WIN32 AND NOT HAIKU)
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share"
        CACHE PATH "Define install directory for read-only architecture-independent data")
else ()
    set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"
        CACHE PATH "Define install directory for read-only architecture-independent data")
endif ()

if (WIN32)
    set(CANTATA_ICON_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/icons/cantata)
elseif (APPLE)
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
elseif (NOT HAIKU)
    set(CANTATA_ICON_INSTALL_PREFIX ${SHARE_INSTALL_PREFIX}/icons/hicolor)
endif ()

if (NOT WIN32 AND NOT APPLE AND ENABLE_DEVICES_SUPPORT AND ENABLE_UDISKS2)
    set(WITH_SOLID_UDISKS2 ON)
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message("-- Set build type to ${CMAKE_BUILD_TYPE}")
endif ()

if (NOT APPLE AND NOT WIN32)
    if (CANTATA_HELPERS_LIB_DIR AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LINUX_LIB_DIR ${CANTATA_HELPERS_LIB_DIR})
    else ()
        set(LINUX_LIB_DIR lib)
    endif ()
endif ()


if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT APPLE AND NOT WIN32 AND NOT HAIKU)
    set(CMAKE_INSTALL_PREFIX "/usr")
    message("-- Set install prefix to ${CMAKE_INSTALL_PREFIX}")
endif ()

if (ENABLE_TAGLIB)
    if (NOT TAGLIB_FOUND)
        set(TAGLIB_MIN_VERSION "1.6")
        find_package(Taglib)
    endif()
    macro_log_feature(TAGLIB_FOUND "TagLib" "Tag editor, file organiser, etc." "http://taglib.github.io/")
endif ()

if (TAGLIB_FOUND)
    if (ENABLE_DEVICES_SUPPORT AND (ENABLE_CDIOPARANOIA OR ENABLE_CDPARANOIA))
        # Issue 1021 - prefer libcdio_paranoia over plain cdparanoia
        if (ENABLE_CDIOPARANOIA)
            find_package(Cdioparanoia)
            macro_log_feature(CDIOPARANOIA_FOUND "libcdio_paranoia" "CD ripping." "https://www.gnu.org/software/libcdio/")
        endif()
        if (NOT CDIOPARANOIA_FOUND)
            find_package(Cdparanoia)
            macro_log_feature(CDPARANOIA_FOUND "libcdda_paranoia" "CD ripping." "http://www.xiph.org/paranoia")
        endif()
        if (CDIOPARANOIA_FOUND OR CDPARANOIA_FOUND)
            if (ENABLE_CDDB)
                find_package(CDDB)
                macro_log_feature(CDDB_FOUND "libcddb" "CD info retrieval via CDDB." "http://libcddb.sourceforge.net")
            endif ()
            if (ENABLE_MUSICBRAINZ)
                find_package(MusicBrainz5)
                macro_log_feature(MUSICBRAINZ5_FOUND "libmusicbrainz5" "CD info retrieval via MusicBrainz." "http://musicbrainz.org/doc/libmusicbrainz")
            endif ()
        endif ()
    endif ()

    set(ENABLE_TAGEDITOR_SUPPORT 1)
    set(ENABLE_TRACKORGANIZER_SUPPORT 1)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDES}")
    set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")

    check_cxx_source_compiles("#include <asftag.h>
                            int main() { TagLib::ASF::Tag tag; return 0;}" TAGLIB_ASF_FOUND)
    if (NOT TAGLIB_ASF_FOUND)
        message("TagLib does not have ASF support compiled in.")
    endif ()

    check_cxx_source_compiles("#include <mp4tag.h>
                            int main() { TagLib::MP4::Tag tag(0, 0); return 0;}" TAGLIB_MP4_FOUND)
    if (NOT TAGLIB_MP4_FOUND)
        message("TagLib does not have MP4 support compiled in.")
    endif ()

    check_cxx_source_compiles("#include <opusfile.h>
                            int main() { TagLib::Ogg::Opus::File *f=0; return 0;}" TAGLIB_OPUS_FOUND)
    if (NOT TAGLIB_OPUS_FOUND)
        message("TagLib does not have OPUS support compiled in.")
    endif ()

    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_LIBRARIES)

    #include(CheckTagLibFileName)
    #check_taglib_filename(COMPLEX_TAGLIB_FILENAME)

    add_definitions(${TAGLIB_CFLAGS})

    if (ENABLE_FFMPEG)
        find_package(FFMPEG)
        macro_log_feature(FFMPEG_FOUND "libavcodec/libavutil/libavformat" "ReplayGain calculation." "http://ffmpeg.org")
    endif ()
    if (ENABLE_MPG123)
        find_package(MPG123)
        macro_log_feature(MPG123_FOUND "libmpg123" "ReplayGain calculation." "http://www.mpg123.de")
    endif ()
    if (ENABLE_MTP AND ENABLE_DEVICES_SUPPORT AND NOT WIN32 AND NOT APPLE)
        find_package(Mtp)
        macro_log_feature(MTP_FOUND "libmtp" "MTP Device Support." "http://libmtp.sourceforge.net")
    endif ()
else (TAGLIB_FOUND)
   set(ENABLE_DEVICES_SUPPORT OFF)
endif ()

if (NOT MTP_FOUND)
   set(ENABLE_UNCACHED_MTP OFF)
endif ()

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5Sql REQUIRED)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_STANDARD 11)

set(QTCORELIBS ${Qt5Core_LIBRARIES})
set(QTNETWORKLIBS ${Qt5Network_LIBRARIES})
set(QTGUILIBS ${Qt5Gui_LIBRARIES})
set(QTLIBS ${QTCORELIBS} ${Qt5Widgets_LIBRARIES} ${QTNETWORKLIBS} ${QTGUILIBS} ${Qt5Xml_LIBRARIES} ${Qt5Concurrent_LIBRARIES} ${Qt5Svg_LIBRARIES} ${Qt5Sql_LIBRARIES})
set(QTINCLUDES ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${Qt5Xml_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS}
    ${Qt5Svg_INCLUDE_DIRS} ${Qt5Sql_INCLUDE_DIRS})
add_definitions(${Qt5Widgets_DEFINITIONS} ${Qt5Network_DEFINITIONS} ${Qt5Xml_DEFINITIONS} ${Qt5Concurrent_DEFINITIONS} ${Qt5Svg_DEFINITIONS} ${Qt5Sql_DEFINITIONS})
if (Qt5_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
endif ()

if (APPLE OR WIN32 OR HAIKU)
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    exec_program(${QT_QMAKE_EXECUTABLE} ARGS -query QT_INSTALL_TRANSLATIONS OUTPUT_VARIABLE QT_TRANSLATIONS_DIR)
else ()
    find_package(Qt5DBus REQUIRED)
    set(QT_QTDBUS_FOUND 1) # required for config.h !!!
    set(QTLIBS ${QTLIBS} ${Qt5DBus_LIBRARIES})
    set(QTINCLUDES ${QTINCLUDES} ${Qt5DBus_INCLUDE_DIRS})
    add_definitions(${Qt5DBus_DEFINITIONS})
endif ()

if (ENABLE_HTTP_STREAM_PLAYBACK)
    if (ENABLE_LIBVLC)
        find_package(LIBVLC REQUIRED)
        macro_log_feature(LIBVLC_FOUND "libVLC" "MPD HTTP stream playback." "http://videolan.org")
        add_definitions(-DLIBVLC_FOUND)
        include_directories(${LIBVLC_INCLUDE_DIR})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${LIBVLC_LIBRARY})
    else ()
        find_package(Qt5Multimedia REQUIRED)
        macro_log_feature(Qt5Multimedia_FOUND "Qt5Multimedia" "MPD HTTP stream playback." "http://qtproject.org")
        include_directories(${Qt5Multimedia_INCLUDE_DIRS})
        add_definitions(${Qt5Multimedia_DEFINITIONS})
        set(CANTATA_LIBS ${CANTATA_LIBS} ${Qt5Multimedia_LIBRARIES})
    endif ()
    set(CANTATA_SRCS ${CANTATA_SRCS} mpd-interface/httpstream.cpp)
endif ()

find_package(ZLIB REQUIRED)

if (APPLE)
    set(CANTATA_SRCS ${CANTATA_SRCS} mac/dockmenu.cpp mac/macnotify.mm)
    find_package(IOKit)
    if (IOKIT_FOUND)
        set(CANTATA_LIBS ${CANTATA_LIBS} ${IOKIT_LIBRARY})
        set(CANTATA_SRCS ${CANTATA_SRCS} mac/powermanagement.cpp)
    endif ()
    find_package(Foundation REQUIRED)
    set(CANTATA_LIBS ${CANTATA_LIBS} ${FOUNDATION_LIBRARY})
    find_package(Qt5MacExtras)
    if (Qt5MacExtras_FOUND)
        set(QTINCLUDES ${QTINCLUDES} ${Qt5MacExtras_INCLUDE_DIRS})
        set(QTLIBS ${QTLIBS} ${Qt5MacExtras_LIBRARIES})
        add_definitions(${Qt5MacExtras_DEFINITIONS})
        set(QT_MAC_EXTRAS_FOUND 1)
    endif ()
elseif (WIN32)
    find_package(Qt5WinExtras REQUIRED)
    set(QTINCLUDES ${QTINCLUDES} ${Qt5WinExtras_INCLUDE_DIRS})
    set(QTLIBS ${QTLIBS} ${Qt5WinExtras_LIBRARIES})
    add_definitions(${Qt5WinExtras_DEFINITIONS})
    set(CANTATA_SRCS ${CANTATA_SRCS} windows/thumbnailtoolbar.cpp)
endif ()

# MusicBrainz requires exceptions to be enabled!
if (MUSICBRAINZ5_FOUND)
    message("-- Enabling exceptions")
    if (CMAKE_COMPILER_IS_GNUCXX)
        add_definitions("-fexceptions -UQT_NO_EXCEPTIONS")
    endif (CMAKE_COMPILER_IS_GNUCXX)
    if (CMAKE_C_COMPILER MATCHES "icc")
        add_definitions(-fexceptions)
    endif ()
    if (MSVC)
        add_definitions(-EHsc)
    endif ()
endif ()

include_directories(${CMAKE_SOURCE_DIR}/3rdparty ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})

set(CANTATA_SRCS ${CANTATA_SRCS}
    gui/settings.cpp gui/application.cpp gui/initialsettingswizard.cpp gui/mainwindow.cpp gui/preferencesdialog.cpp gui/customactionssettings.cpp
    gui/interfacesettings.cpp gui/playbacksettings.cpp gui/serversettings.cpp gui/librarypage.cpp gui/customactions.cpp
    gui/folderpage.cpp gui/trayitem.cpp gui/cachesettings.cpp gui/coverdialog.cpp gui/searchpage.cpp gui/stdactions.cpp
    gui/main.cpp gui/covers.cpp gui/currentcover.cpp gui/mpdbrowsepage.cpp gui/localfolderpage.cpp gui/multimediakeysinterface.cpp
    gui/apikeys.cpp gui/apikeyssettings.cpp
    devices/deviceoptions.cpp
    db/librarydb.cpp db/mpdlibrarydb.cpp
    widgets/treeview.cpp widgets/listview.cpp widgets/itemview.cpp widgets/autohidingsplitter.cpp widgets/nowplayingwidget.cpp
    widgets/actionlabel.cpp widgets/playqueueview.cpp widgets/groupedview.cpp widgets/actionitemdelegate.cpp widgets/textbrowser.cpp
    widgets/volumeslider.cpp widgets/menubutton.cpp widgets/icons.cpp widgets/toolbutton.cpp widgets/wizardpage.cpp
    widgets/searchwidget.cpp widgets/messageoverlay.cpp widgets/basicitemdelegate.cpp widgets/sizegrip.cpp
    widgets/servicestatuslabel.cpp widgets/spacerwidget.cpp widgets/songdialog.cpp widgets/stretchheaderview.cpp
    widgets/tableview.cpp widgets/thinsplitterhandle.cpp widgets/coverwidget.cpp widgets/ratingwidget.cpp widgets/notelabel.cpp
    widgets/selectorlabel.cpp widgets/titlewidget.cpp widgets/multipagewidget.cpp widgets/singlepagewidget.cpp widgets/stackedpagewidget.cpp
    widgets/mirrormenu.cpp widgets/genrecombo.cpp widgets/volumecontrol.cpp
    context/lyricsettings.cpp context/ultimatelyricsprovider.cpp context/ultimatelyrics.cpp context/lyricsdialog.cpp
    context/contextwidget.cpp context/view.cpp context/artistview.cpp context/albumview.cpp context/songview.cpp context/contextengine.cpp
    context/wikipediaengine.cpp context/wikipediasettings.cpp context/othersettings.cpp context/contextsettings.cpp context/togglelist.cpp
    context/lastfmengine.cpp context/metaengine.cpp context/onlineview.cpp
    streams/streamspage.cpp streams/digitallyimportedsettings.cpp streams/streamssettings.cpp streams/streamdialog.cpp streams/tar.cpp
    streams/streamproviderlistdialog.cpp streams/streamfetcher.cpp
    models/streamsproxymodel.cpp models/streamsearchmodel.cpp models/digitallyimported.cpp models/musiclibraryitemroot.cpp
    models/musiclibraryitemartist.cpp models/musiclibraryitemalbum.cpp models/musiclibraryproxymodel.cpp models/playlistsmodel.cpp
    models/playlistsproxymodel.cpp models/playqueuemodel.cpp models/proxymodel.cpp models/actionmodel.cpp models/musiclibraryitem.cpp
    models/browsemodel.cpp models/searchmodel.cpp models/streamsmodel.cpp models/searchproxymodel.cpp models/sqllibrarymodel.cpp
    models/mpdlibrarymodel.cpp models/mpdsearchmodel.cpp models/playqueueproxymodel.cpp models/localbrowsemodel.cpp
    mpd-interface/mpdconnection.cpp mpd-interface/mpdparseutils.cpp mpd-interface/mpdstats.cpp mpd-interface/mpdstatus.cpp
    mpd-interface/song.cpp mpd-interface/cuefile.cpp
    network/networkaccessmanager.cpp network/networkproxyfactory.cpp
    playlists/dynamicplaylists.cpp playlists/playlistproxymodel.cpp playlists/dynamicplaylistspage.cpp playlists/playlistruledialog.cpp
    playlists/playlistrulesdialog.cpp playlists/playlistspage.cpp playlists/storedplaylistspage.cpp playlists/rulesplaylists.cpp
    playlists/smartplaylists.cpp playlists/smartplaylistspage.cpp
    online/onlineservicespage.cpp online/onlinedbservice.cpp online/jamendoservice.cpp online/onlinedbwidget.cpp online/onlineservice.cpp
    online/jamendosettingsdialog.cpp online/magnatuneservice.cpp online/magnatunesettingsdialog.cpp online/soundcloudservice.cpp
    online/onlinesearchwidget.cpp online/podcastservice.cpp online/rssparser.cpp online/opmlparser.cpp online/podcastsearchdialog.cpp
    online/podcastsettingsdialog.cpp online/podcastwidget.cpp online/onlinesearchservice.cpp
    db/onlinedb.cpp
    scrobbling/scrobbler.cpp scrobbling/pausabletimer.cpp scrobbling/scrobblingsettings.cpp scrobbling/scrobblinglove.cpp
    http/httpserver.cpp)
set(CANTATA_UIS ${CANTATA_UIS}
    gui/initialsettingswizard.ui gui/mainwindow.ui gui/interfacesettings.ui gui/playbacksettings.ui gui/serversettings.ui gui/coverdialog.ui
    context/togglelist.ui context/othersettings.ui
    streams/digitallyimportedsettings.ui streams/streamssettings.ui
    playlists/playlistrule.ui playlists/playlistrules.ui
    widgets/itemview.ui scrobbling/scrobblingsettings.ui)

if (ENABLE_CATEGORIZED_VIEW)
    set(CANTATA_SRCS ${CANTATA_SRCS} widgets/categorizedview.cpp)
endif ()
if (ENABLE_SIMPLE_MPD_SUPPORT)
    set(CANTATA_SRCS ${CANTATA_SRCS} mpd-interface/mpduser.cpp)
endif ()

set(CANTATA_LIBS ${CANTATA_LIBS} support)

if (WIN32)
    # Not installed for windows - script uses sym-links...
elseif (APPLE)
    install(PROGRAMS playlists/cantata-dynamic DESTINATION ${MACOSX_BUNDLE_RESOURCES}/scripts/)
else ()
    install(PROGRAMS playlists/cantata-dynamic DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/scripts)
endif ()

if (ENABLE_HTTP_SERVER)
    set(CANTATA_SRCS ${CANTATA_SRCS} http/httpsocket.cpp)
endif ()

if (QT_QTDBUS_FOUND)
    set(CANTATA_SRCS ${CANTATA_SRCS} dbus/gnomemediakeys.cpp)
    set_SOURCE_FILES_PROPERTIES(dbus/org.gnome.SettingsDaemon.xml dbus/org.gnome.SettingsDaemon.MediaKeys.xml PROPERTIES NO_NAMESPACE TRUE)
    set(CANTATA_SRCS ${CANTATA_SRCS} dbus/mpris.cpp dbus/powermanagement.cpp)
    set(CANTATA_SRCS ${CANTATA_SRCS} dbus/notify.cpp)

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
endif ()

if (NOT WIN32 AND NOT APPLE)
    set(CANTATA_SRCS ${CANTATA_SRCS} devices/mountpoints.cpp)
endif ()

set(CANTATA_RCS cantata.qrc cantata_media.qrc)

if (TAGLIB_FOUND)
    set(CANTATA_SRCS ${CANTATA_SRCS}  tags/tageditor.cpp tags/trackorganiser.cpp devices/filenameschemedialog.cpp widgets/tagspinbox.cpp)
    set(CANTATA_UIS ${CANTATA_UIS} tags/tageditor.ui tags/trackorganiser.ui devices/filenameschemedialog.ui)

    # Cantata still links to taglib, even if external tag reader/writer is used, because JamendoService uses taglib for ID3 genres.
    set(CANTATA_LIBS ${CANTATA_LIBS} ${TAGLIB_LIBRARIES})
    include_directories(${TAGLIB_INCLUDES})
    if (APPLE OR OPENBSD)
        # for some reason under osx we get full path no include/taglib?
        include_directories(${TAGLIB_INCLUDES}/..)
    endif ()
    if (ENABLE_DEVICES_SUPPORT)
        set(CANTATA_LIBS ${CANTATA_LIBS} solidlite)
    endif ()

    if (FFMPEG_FOUND OR MPG123_FOUND)
        set(CANTATA_SRCS ${CANTATA_SRCS} replaygain/albumscanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp replaygain/jobcontroller.cpp)
        set(ENABLE_REPLAYGAIN_SUPPORT 1)
        add_subdirectory(replaygain)
    endif ()
    set(CANTATA_SRCS ${CANTATA_SRCS} tags/taghelperiface.cpp)
    add_subdirectory(tags)

    if (ENABLE_DEVICES_SUPPORT)
        add_subdirectory(3rdparty/solid-lite)
        if (MTP_FOUND)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp)
            include_directories(${MTP_INCLUDE_DIR})
            set(CANTATA_LIBS ${CANTATA_LIBS} ${MTP_LIBRARIES})
        endif ()
        if (CDDB_FOUND OR MUSICBRAINZ5_FOUND)
            if (CDDB_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/cddbinterface.cpp)
                include_directories(${CDDB_INCLUDE_DIR})
                set(CANTATA_LIBS ${CANTATA_LIBS} ${CDDB_LIBS})
            endif ()
            if (MUSICBRAINZ5_FOUND)
                set(CANTATA_SRCS ${CANTATA_SRCS} devices/musicbrainz.cpp)
                include_directories(${MUSICBRAINZ5_INCLUDE_DIRS})
                set(CANTATA_LIBS ${CANTATA_LIBS} ${MUSICBRAINZ5_LIBRARIES})
            endif ()
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/audiocddevice.cpp devices/cddbselectiondialog.cpp
                 devices/cdparanoia.cpp devices/audiocdsettings.cpp devices/extractjob.cpp devices/albumdetailsdialog.cpp)
            set(CANTATA_UIS ${CANTATA_UIS} devices/albumdetails.ui devices/audiocdsettings.ui)
            # If CDDB/MusicBrainz5 found - then CDParanoia must have been!
            if (CDIOPARANOIA_FOUND)
                set(CANTATA_LIBS ${CANTATA_LIBS} ${CDIOPARANOIA_LIBRARIES})
                include_directories(${CDIOPARANOIA_INCLUDE_DIRS})
            else()
                set(CANTATA_LIBS ${CANTATA_LIBS} ${CDPARANOIA_LIBRARIES})
                include_directories(${CDPARANOIA_INCLUDE_DIR})
            endif()
        endif ()

        set(CANTATA_SRCS ${CANTATA_SRCS} devices/devicespage.cpp devices/filejob.cpp
                devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp devices/splitlabelwidget.cpp
                models/devicesmodel.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
                devices/devicepropertiesdialog.cpp devices/encoders.cpp devices/freespaceinfo.cpp
                devices/transcodingjob.cpp devices/valueslider.cpp devices/syncdialog.cpp
                devices/synccollectionwidget.cpp
                models/musiclibrarymodel.cpp)
        set(CANTATA_UIS ${CANTATA_UIS} devices/actiondialog.ui devices/devicepropertieswidget.ui
                devices/synccollectionwidget.ui)

        if (ENABLE_REMOTE_DEVICES)
            set(CANTATA_SRCS ${CANTATA_SRCS} devices/remotefsdevice.cpp devices/remotedevicepropertiesdialog.cpp
                devices/remotedevicepropertieswidget.cpp)
            set(CANTATA_UIS ${CANTATA_UIS} devices/remotedevicepropertieswidget.ui)
        endif ()
    else (ENABLE_DEVICES_SUPPORT)
        set(CANTATA_SRCS ${CANTATA_SRCS} devices/device.cpp)
    endif ()
else (TAGLIB_FOUND)
    set(ENABLE_DEVICES_SUPPORT OFF)
    set(ENABLE_REMOTE_DEVICES OFF)
endif ()

if (ENABLE_PROXY_CONFIG)
    set(CANTATA_SRCS ${CANTATA_SRCS} network/proxysettings.cpp)
    set(CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui)
endif ()

if (WIN32 OR APPLE OR HAIKU)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/singleapplication.cpp)
endif ()
if (WIN32)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_win.cpp)
elseif (APPLE)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_mac.cpp)
elseif (HAIKU)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_haiku.cpp)
else ()
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/application_qt.cpp)
endif ()
if (WIN32)
    add_definitions(-DWIN32)
    add_subdirectory(windows)
    if (NOT CANTATA_WINDOWS_INSTALLER_DEST)
        set(CANTATA_WINDOWS_INSTALLER_DEST z:\ )
    endif ()
    if (MINGW)
        # resource compilation for MinGW
        ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o
            COMMAND windres.exe -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/windows/cantataico.rc
            -o ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
        set(CANTATA_SRCS ${CANTATA_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
    else ()
        set(CANTATA_SRCS ${CANTATA_SRCS} windows/cantataico.rc)
    endif ()
elseif (APPLE)
    add_definitions(-D__APPLE__)
endif ()

set(CANTATA_SRCS ${CANTATA_SRCS} gui/shortcutssettingspage.cpp gui/mediakeys.cpp)

if (WIN32 OR APPLE)
    set(CANTATA_RCS ${CANTATA_RCS} cantata_icon.qrc)
else()
    add_subdirectory(icons)
endif()

QT5_ADD_RESOURCES(CANTATA_RC_SRCS ${CANTATA_RCS})
QT5_WRAP_UI(CANTATA_UI_HDRS ${CANTATA_UIS})

if (WIN32)
    add_definitions(-DQXT_STATIC)
    add_subdirectory(3rdparty/qxt)
    set(CANTATA_SRCS ${CANTATA_SRCS} gui/qxtmediakeys.cpp)
    set(CMAKE_BUILD_TYPE "Release")
    ADD_EXECUTABLE(cantata WIN32 ${CANTATA_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
    install(TARGETS cantata DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif (APPLE)
    ADD_EXECUTABLE(cantata MACOSX_BUNDLE ${CANTATA_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})

    set(BREW_OPENSSL_PATH /usr/local/opt/openssl/lib)
    if (EXISTS ${BREW_OPENSSL_PATH}/libcrypto.1.0.0.dylib AND EXISTS ${BREW_OPENSSL_PATH}/libssl.1.0.0.dylib)
        install(FILES ${BREW_OPENSSL_PATH}/libcrypto.1.0.0.dylib DESTINATION ${MACOSX_BUNDLE_APP_DIR} RENAME libcrypto.dylib)
        install(FILES ${BREW_OPENSSL_PATH}/libssl.1.0.0.dylib DESTINATION ${MACOSX_BUNDLE_APP_DIR} RENAME libssl.dylib)
    endif()
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
        # to the build libs not the packaged ones!
        if (FFMPEG_FOUND OR MPG123_FOUND)
            install(PROGRAMS ${CMAKE_BINARY_DIR}/replaygain/cantata-replaygain DESTINATION ${MACOSX_BUNDLE_APP_DIR})
        endif (FFMPEG_FOUND OR MPG123_FOUND)
        install(PROGRAMS ${CMAKE_BINARY_DIR}/tags/cantata-tags DESTINATION ${MACOSX_BUNDLE_APP_DIR})
    endif (TAGLIB_FOUND)
    if (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
        install_qt5_executable(cantata.app "qjpeg;qsvg;qsvgicon;qcocoa;qsqlite;qavfmediaplayer")
    else (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
        install_qt5_executable(cantata.app "qjpeg;qsvg;qsvgicon;qcocoa;qsqlite")
    endif (ENABLE_HTTP_STREAM_PLAYBACK AND NOT ENABLE_LIBVLC)
elseif (HAIKU)
    ADD_EXECUTABLE(cantata ${CANTATA_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
    install(TARGETS cantata DESTINATION ${CMAKE_INSTALL_PREFIX})
else ()
    ADD_EXECUTABLE(cantata ${CANTATA_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
    install(TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif ()

if (ENABLE_CATEGORIZED_VIEW)
    add_subdirectory(3rdparty/kcategorizedview)
    target_link_libraries(cantata kcategorizedview)
endif ()

if (WIN32 OR APPLE OR HAIKU)
    add_subdirectory(3rdparty/qtsingleapplication)
    target_link_libraries(cantata qtsingleapplication)
else ()
    set(XDG_APPS_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/applications")
endif ()

if (WIN32)
    target_link_libraries(cantata qxt)
endif (WIN32)

add_subdirectory(translations)
add_subdirectory(support)
add_subdirectory(3rdparty/qtiocompressor)
add_subdirectory(streams/icons)
add_subdirectory(online/icons)
target_link_libraries(cantata support-core qtiocompressor ${CANTATA_LIBS} ${QTLIBS} ${ZLIB_LIBRARIES})

# enable warnings
add_definitions(-DQT_NO_DEBUG_OUTPUT)

if (UNIX AND NOT APPLE)
    if (NOT HAIKU)
        configure_file(cantata-remote.cmake ${CMAKE_BINARY_DIR}/cantata-remote)
        install(PROGRAMS ${CMAKE_BINARY_DIR}/cantata-remote DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/scripts)
        install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
    endif ()
    target_link_libraries(cantata -lpthread)
endif ()

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

if (NOT WIN32 AND NOT APPLE AND NOT HAIKU)
    # uninstall target
    configure_file("${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
    configure_file(install_dirs.cmake ${CMAKE_BINARY_DIR}/install_dirs)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)
endif ()

macro_display_feature_log()

if ( (NOT WIN32 AND NOT APPLE AND ( (NOT ENABLE_SIMPLE_MPD_SUPPORT)  OR (NOT ENABLE_DEVICES_SUPPORT) OR (ENABLE_DEVICES_SUPPORT AND NOT ENABLE_REMOTE_DEVICES) ) ) OR (NOT ENABLE_HTTP_STREAM_PLAYBACK) OR (NOT ENABLE_PROXY_CONFIG))
    message("-----------------------------------------------------------------------------")
    message("-- The following STANDARD options have been DISABLED.")
    message("-----------------------------------------------------------------------------")
    if (NOT WIN32 AND NOT APPLE)
        if (NOT ENABLE_SIMPLE_MPD_SUPPORT)
	        message("    * Simple/Personal (Cantata controlled) MPD instance support")
        endif()
        if (NOT ENABLE_DEVICES_SUPPORT)
            message("    * Devices Tab - Media device support (UMS, MTP, AudioCD).")
        elseif (NOT ENABLE_REMOTE_DEVICES)
            message("    * Remote Devices - Access sshfs, Samba, and local folders as devices. (EXPERIMENTAL)")
        endif ()
    endif()
    if (NOT ENABLE_HTTP_STREAM_PLAYBACK)
        message("    * MPD HTTP Streams - Playback MPD HTTP output stream via libVLC.")
    endif ()
    if (NOT ENABLE_PROXY_CONFIG)
        message("    * Proxy Configuration")
    endif ()

    message("")
endif ()
