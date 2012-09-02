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
set(CPACK_PACKAGE_VERSION_MINOR "8")
set(CPACK_PACKAGE_VERSION_PATCH "3")
set(CPACK_PACKAGE_CONTACT "Craig Drummond <craig.p.drummond@gmail.com>")
set(CANTATA_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
set(CANTATA_VERSION_FULL "${CANTATA_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CANTATA_VERSION_FULL}")
include(CPack)

OPTION(ENABLE_TAGLIB "Enable TagLib library (required for tag editing, replaygain calculation, device support, etc)" ON)
OPTION(ENABLE_TAGLIB_EXTRAS "Enable TagLib-Extras library (required for tag editing, replaygain calculation, device support, etc)" ON)
OPTION(ENABLE_PHONON "Enable Phonon library (required to playback MPD HTTP streams)" ON)
OPTION(ENABLE_KDE "Enable KDE libraries (required for device support, and to use KDE dialogs)" ON)
OPTION(ENABLE_MTP "Enable MTP library (required to support MTP devices)" ON)
OPTION(ENABLE_WEBKIT "Enable WebKit library (required for artist/album information)" ON)
OPTION(ENABLE_FFMPEG "Enable ffmpeg/libav libraries (required for replaygain calculation)" ON)
OPTION(ENABLE_MPG123 "Enable mpg123 libraries (required for replaygain calculation)" ON)
OPTION(ENABLE_SPEEXDSP "Enable SpeexDSP libraries (used to speed-up replaygain calculation)" ON)

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
    application.cpp
    main.cpp
    gui/mainwindow.cpp
    gui/preferencesdialog.cpp
    gui/settings.cpp
    gui/covers.cpp
    gui/interfacesettings.cpp
    gui/externalsettings.cpp
    gui/playbacksettings.cpp
    gui/serverplaybacksettings.cpp
    gui/serversettings.cpp
    gui/streamspage.cpp
    gui/streamdialog.cpp
    gui/streamcategorydialog.cpp
    gui/librarypage.cpp
    gui/albumspage.cpp
    gui/folderpage.cpp
    gui/infopage.cpp
    gui/serverinfopage.cpp
    gui/playlistspage.cpp
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
    models/proxymodel.cpp
    mpd/mpdconnection.cpp
    mpd/mpdparseutils.cpp
    mpd/mpdstats.cpp
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
    widgets/groupedview.cpp
    widgets/messagewidget.cpp
    widgets/actionitemdelegate.cpp
    widgets/textbrowser.cpp
    widgets/coverwidget.cpp
    widgets/icon.cpp
    widgets/volumecontrol.cpp
    lyrics/lyricspage.cpp
    lyrics/lyricsettings.cpp
    lyrics/ultimatelyricsprovider.cpp
    lyrics/ultimatelyricsreader.cpp
    lyrics/songinfoprovider.cpp
    lyrics/lyricsdialog.cpp
    network/networkaccessmanager.cpp
    network/network.cpp
    libmaia/maiaObject.cpp
    libmaia/maiaFault.cpp
    libmaia/maiaXmlRpcClient.cpp
    devices/utils.cpp
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
    gui/serverplaybacksettings.h
    gui/serversettings.h
    gui/preferencesdialog.h
    gui/interfacesettings.h
    gui/externalsettings.h
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
    mpd/mpdstats.h
    mpd/mpdstatus.h
    widgets/fancytabwidget.h
    widgets/treeview.h
    widgets/listview.h
    widgets/itemview.h
    widgets/autohidingsplitter.h
    widgets/timeslider.h
    widgets/actionlabel.h
    widgets/playqueueview.h
    widgets/groupedview.h
    widgets/messagewidget.h
    widgets/coverwidget.h
    widgets/volumecontrol.h
    lyrics/lyricspage.h
    lyrics/lyricsettings.h
    lyrics/ultimatelyricsprovider.h
    lyrics/songinfoprovider.h
    lyrics/lyricsdialog.h
    network/networkaccessmanager.h
    network/network.h
    libmaia/maiaObject.h
    libmaia/maiaFault.h
    libmaia/maiaXmlRpcClient.h
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
    gui/serverplaybacksettings.ui
    gui/serversettings.ui
    lyrics/lyricspage.ui
    lyrics/lyricsettings.ui
    widgets/itemview.ui
)

if (ENABLE_TAGLIB)
    set(TAGLIB_MIN_VERSION "1.6")
    find_package(Taglib)
endif (ENABLE_TAGLIB)

if (TAGLIB_FOUND)
    SET( CANTATA_SRCS ${CANTATA_SRCS}
        gui/httpserversettings.cpp
        gui/tageditor.cpp
        devices/deviceoptions.cpp
        devices/trackorganiser.cpp
        http/httpserver.cpp
        http/httpsocket.cpp
        devices/filenameschemedialog.cpp
        devices/tags.cpp
    )

    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS}
        gui/tageditor.h
        http/httpsocket.h
        devices/filenameschemedialog.h
        devices/trackorganiser.h
    )

    SET( CANTATA_UIS ${CANTATA_UIS}
        gui/httpserversettings.ui
        gui/tageditor.ui
        devices/filenameschemedialog.ui
        devices/trackorganiser.ui
    )
endif (TAGLIB_FOUND)

if (NOT WIN32)
    SET( CANTATA_SRCS ${CANTATA_SRCS}
        dbus/mpris.cpp
        dbus/dockmanager.cpp
        dynamic/dynamic.cpp
        dynamic/dynamicpage.cpp
        dynamic/dynamicproxymodel.cpp
        dynamic/dynamicruledialog.cpp
        dynamic/dynamicrulesdialog.cpp
    )

    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS}
        dbus/mpris.h
        dbus/dockmanager.h
        dynamic/dynamic.h
        dynamic/dynamicpage.h
        dynamic/dynamicruledialog.h
        dynamic/dynamicrulesdialog.h
    )
    SET( CANTATA_UIS ${CANTATA_UIS}
        dynamic/dynamicpage.ui
        dynamic/dynamicrule.ui
        dynamic/dynamicrules.ui
)
endif(NOT WIN32)

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
                     ${CMAKE_SOURCE_DIR}/dynamic
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
)

# this command finds Qt4 libraries and sets all required variables
FIND_PACKAGE( Qt4 REQUIRED QtCore QtGui QtXml QtNetwork )

if (ENABLE_PHONON)
    FIND_PACKAGE( Phonon )
endif (ENABLE_PHONON)

if(NOT WIN32)
    qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
    qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.root.xml dbus/mpris.h Mpris)
    qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.kde.cantata.xml gui/mainwindow.h MainWindow)
    qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockItem.xml )
    qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockManager.xml)
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
    if(FFMPEG_FOUND AND MPG123_FOUND)
        cantata_check_for_sse()
        set (ENABLE_REPLAYGAIN_SUPPORT 1)
        if (ENABLE_SPEEXDSP)
            find_package(SPEEXDSP)
        endif (ENABLE_SPEEXDSP)
        set(USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ffmpeginput.cpp replaygain/mpg123input.cpp)
        include_directories(${CMAKE_SOURCE_DIR}/replaygain )
        SET(CANTATA_SRCS ${CANTATA_SRCS} replaygain/ebur128.c replaygain/scanner.cpp replaygain/rgdialog.cpp replaygain/tagreader.cpp replaygain/jobcontroller.cpp )
        SET(CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} replaygain/scanner.h replaygain/rgdialog.h replaygain/tagreader.h replaygain/jobcontroller.h )
    endif(FFMPEG_FOUND AND MPG123_FOUND)
endif (TAGLIB_FOUND)

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
# TODO: Remove this IF(...)
IF( ENABLE_KDE_SUPPORT )
add_subdirectory(po)
ENDIF( ENABLE_KDE_SUPPORT )

IF( ENABLE_KDE_SUPPORT )
    if (TAGLIB_FOUND)
        set (ENABLE_TAG_EDITOR_SUPPORT 1)
        set (ENABLE_DEVICES_SUPPORT 1)
        if (ENABLE_MTP)
            find_package(Mtp)
        endif (ENABLE_MTP)

        SET( CANTATA_SRCS ${CANTATA_SRCS} devices/mediadevicecache.cpp devices/devicespage.cpp
                devices/device.cpp devices/fsdevice.cpp devices/umsdevice.cpp
                models/devicesmodel.cpp devices/filetyperesolver.cpp devices/actiondialog.cpp devices/devicepropertieswidget.cpp
                devices/devicepropertiesdialog.cpp devices/encoders.cpp
                devices/transcodingjob.cpp devices/valueslider.cpp devices/syncdialog.cpp devices/synccollectionwidget.cpp)
        SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mediadevicecache.h devices/devicespage.h
                devices/device.h devices/fsdevice.h devices/umsdevice.h models/devicesmodel.h
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
        endif (ENABLE_REMOTE_DEVICES)
        if (MTP_FOUND)
            SET( CANTATA_SRCS ${CANTATA_SRCS} devices/mtpdevice.cpp )
            SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} devices/mtpdevice.h )
        endif (MTP_FOUND)
    endif (TAGLIB_FOUND)

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
    TARGET_LINK_LIBRARIES( cantata ${KDE4_KDEWEBKIT_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS} )
    if (MTP_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${MTP_LIBRARIES})
        include_directories( ${MTP_INCLUDE_DIR})
    endif (MTP_FOUND)
    if (ENABLE_REMOTE_DEVICES)
        TARGET_LINK_LIBRARIES(cantata ${KDE4_KFILE_LIBS})
    endif (ENABLE_REMOTE_DEVICES)
    install( TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS} )
    install( PROGRAMS dynamic/cantata-dynamic DESTINATION ${LIBEXEC_INSTALL_DIR})
    install( FILES cantataui.rc DESTINATION ${DATA_INSTALL_DIR}/cantata )
    install( FILES cantata.notifyrc DESTINATION ${DATA_INSTALL_DIR}/cantata )
ELSE( ENABLE_KDE_SUPPORT )
    SET( CANTATA_SRCS ${CANTATA_SRCS} widgets/dirrequester.cpp widgets/lineedit.cpp network/networkproxyfactory.cpp network/proxysettings.cpp widgets/kmessagewidget.cpp
            widgets/dialog.cpp widgets/messagebox.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} application.h widgets/dirrequester.h widgets/lineedit.h network/proxysettings.h widgets/kmessagewidget.h widgets/urllabel.h
            widgets/dialog.h )
    SET( CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui )
    SET( CANTATA_RCS ${CANTATA_RCS} cantata_qt.qrc )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    QT4_WRAP_UI( CANTATA_UI_HDRS ${CANTATA_UIS} )
    INCLUDE( ${QT_USE_FILE} )
    QT4_WRAP_CPP( CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS} )

    if (WIN32)
        IF( MINGW )
            # resource compilation for MinGW
            ADD_CUSTOM_COMMAND( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o
                                COMMAND windres.exe -I${CMAKE_CURRENT_SOURCE_DIR} -i${CMAKE_SOURCE_DIR}/icons/cantataico.rc
                                    -o ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o )
            SET(CANTATA_SRCS ${CANTATA_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/cantataico.o)
        ELSE( MINGW )
            SET(CANTATA_SRCS ${CANTATA_SRCS} icons/cantataico.rc)
        ENDIF( MINGW )
        ADD_EXECUTABLE( cantata WIN32 ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} ${CANTATA_PO})
    else (WIN32)
        ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS})
        install( PROGRAMS dynamic/cantata-dynamic DESTINATION bin)
    endif (WIN32)

    install( TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
    ADD_DEFINITIONS( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
    SET( XDG_APPS_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/applications" )

    add_subdirectory(qtsingleapplication)
    TARGET_LINK_LIBRARIES(cantata qtsingleapplication)
    include_directories(${CMAKE_BINARY_DIR}/qtsingleapplication)
ENDIF( ENABLE_KDE_SUPPORT )

ADD_SUBDIRECTORY( icons )

TARGET_LINK_LIBRARIES(cantata ${QT_QTWEBKIT_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTDBUS_LIBRARY})
include_directories( ${QT_INCLUDES})

if (TAGLIB_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${TAGLIB_LIBRARIES})
    include_directories(${TAGLIB_INCLUDES})
    if (TAGLIB-EXTRAS_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${TAGLIB-EXTRAS_LIBRARIES})
        include_directories( ${TAGLIB-EXTRAS_INCLUDES})
    endif (TAGLIB-EXTRAS_FOUND)

    if(FFMPEG_FOUND AND MPG123_FOUND)
        TARGET_LINK_LIBRARIES(cantata ${FFMPEG_LIBRARIES} ${MPG123_LIBRARIES})
        include_directories(${FFMPEG_INCLUDE_DIRS} ${MPG123_INCLUDE_DIRS})
        if(SPEEXDSP_FOUND)
            TARGET_LINK_LIBRARIES(cantata ${SPEEXDSP_LIBRARIES})
            include_directories( ${SPEEXDSP_INCLUDE_DIRS})
        endif(SPEEXDSP_FOUND)
    endif(FFMPEG_FOUND AND MPG123_FOUND)
endif (TAGLIB_FOUND)

if (PHONON_FOUND)
    TARGET_LINK_LIBRARIES(cantata ${PHONON_LIBS})
    include_directories( ${PHONON_INCLUDES})
endif (PHONON_FOUND)

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

IF( UNIX AND NOT APPLE)
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
ENDIF( UNIX AND NOT APPLE)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

message("")
IF( ENABLE_KDE_SUPPORT )
    message("INFO: Building with KDE support")
    if (TAGLIB_FOUND)
        message("      Extra enabled features:")
        message("        - UMS device sync")
        if (MTP_FOUND)
            message("        - MTP device sync")
        endif (MTP_FOUND)
        if (ENABLE_REMOTE_DEVICES)
            message("        - Remote device sync (EXPERIMENTAL)")
        endif (ENABLE_REMOTE_DEVICES)
        if( NOT TAGLIB_FOUND OR NOT MTP_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
            message("      Disabled features:")
            if (NOT TAGLIB_FOUND)
                message("        - Tag Editor (taglib required)")
                message("        - Track Organizer (taglib required)")
                message("        - Commandline support (taglib required)")
                message("        - HTTP server support (taglib required)")
            endif (NOT TAGLIB_FOUND)
            if (NOT MTP_FOUND)
                message("        - MTP device sync (taglib and libmtp required)")
            endif (NOT MTP_FOUND)
            #if (NOT ENABLE_REMOTE_DEVICES)
            #    message("        - Remote device sync (EXPERIMENTAL)")
            #endif (NOT ENABLE_REMOTE_DEVICES)
            if(NOT FFMPEG_FOUND OR NOT MPG123_FOUND)
                message("        - ReplayGain calculation (taglib, ffmpeg, and mpg123 required)")
            endif(NOT FFMPEG_FOUND OR NOT MPG123_FOUND)
        endif( NOT TAGLIB_FOUND OR NOT MTP_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
    endif (TAGLIB_FOUND)
ELSE( ENABLE_KDE_SUPPORT )
    if (NOT WIN32)
        message("INFO: Building WITHOUT KDE support - Device sync support DISABLED")
        if( NOT TAGLIB_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
            message("      Disabled features:")
            if (NOT TAGLIB_FOUND)
                message("        - Tag Editor (taglib required)")
                message("        - Track Organizer (taglib required)")
                message("        - Commandline support (taglib required)")
                message("        - HTTP server support (taglib required)")
            endif (NOT TAGLIB_FOUND)
            message("        - ReplayGain calculation (taglib, ffmpeg, and mpg123 required)")
        endif( NOT TAGLIB_FOUND OR NOT FFMPEG_FOUND OR NOT MPG123_FOUND )
    endif (NOT WIN32)
    message("")
    message("*************************************************************")
    message("*************************************************************")
    message("** NOTE: You NEED to have the Oxygen icons installed!!     **")
    message("**       If not, Cantata may have *lots* of missing icons. **")
    message("*************************************************************")
    message("*************************************************************")
    message("")
ENDIF( ENABLE_KDE_SUPPORT )
message("")
