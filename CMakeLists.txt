cmake_minimum_required( VERSION 2.6 )

PROJECT( cantata )

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt4/KDE4 Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "0")
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
    gui/updatedialog.cpp
    gui/covers.cpp
    gui/interfacesettings.cpp
    gui/externalsettings.cpp
    gui/playbacksettings.cpp
    gui/serversettings.cpp
    gui/outputsettings.cpp
    gui/streamspage.cpp
    gui/streamdialog.cpp
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
    mpd/mpdconnection.cpp
    mpd/mpdparseutils.cpp
    mpd/mpdstats.cpp
    mpd/mpdstatus.cpp
    mpd/song.cpp
    widgets/treeview.cpp
    widgets/fancytabwidget.cpp
    widgets/listview.cpp
    widgets/itemview.cpp
    widgets/timeslider.cpp
    lyrics/lyricspage.cpp
    lyrics/lyricsettings.cpp
    lyrics/ultimatelyricsprovider.cpp
    lyrics/ultimatelyricsreader.cpp
    lyrics/songinfoprovider.cpp
    network/networkaccessmanager.cpp
    network/network.cpp
    dbus/mpris.cpp
    dbus/dockmanager.cpp
)

SET( CANTATA_MOC_HDRS
    gui/mainwindow.h
    gui/updatedialog.h
    gui/covers.h
    gui/folderpage.h
    gui/librarypage.h
    gui/albumspage.h
    gui/playlistspage.h
    gui/streamspage.h
    gui/infopage.h
    gui/serverinfopage.h
    gui/streamdialog.h
    gui/playbacksettings.h
    gui/outputsettings.h
    gui/preferencesdialog.h
    gui/settings.h
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
    widgets/fancytabwidget.h
    widgets/treeview.h
    widgets/listview.h
    widgets/itemview.h
    widgets/timeslider.h
    lyrics/lyricspage.h
    lyrics/lyricsettings.h
    lyrics/ultimatelyricsprovider.h
    lyrics/songinfoprovider.h
    network/networkaccessmanager.h
    network/network.h
    dbus/mpris.h
    dbus/dockmanager.h
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
    gui/outputsettings.ui
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
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
)

SET( QT_USE_QTNETWORK TRUE )
SET( QT_USE_QTXML TRUE )

# this command finds Qt4 libraries and sets all required variables
FIND_PACKAGE( Qt4 REQUIRED QtCore QtGui QtXml QtNetwork )
FIND_PACKAGE( KDE4 )

qt4_add_dbus_adaptor(CANTATA_SRCS dbus/org.mpris.MediaPlayer2.Player.xml dbus/mpris.h Mpris)
qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockItem.xml )
qt4_add_dbus_interfaces(CANTATA_SRCS dbus/net.launchpad.DockManager.xml)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

IF( NOT DEFINED WANT_KDE_SUPPORT )
    SET( WANT_KDE_SUPPORT ON )
ENDIF( NOT DEFINED WANT_KDE_SUPPORT )

IF( NOT DEFINED ENABLE_WEBKIT )
    SET( ENABLE_WEBKIT TRUE )
ENDIF( NOT DEFINED ENABLE_WEBKIT )

IF( KDE4_FOUND )
    IF( WANT_KDE_SUPPORT )
        MESSAGE("KDE 4 has been found, will build extra features...")
        ADD_DEFINITIONS( -DENABLE_KDE_SUPPORT )
        SET( ENABLE_KDE_SUPPORT TRUE )
    ELSE( WANT_KDE_SUPPORT )
        MESSAGE("KDE 4 has been found, but won't be used by user request")
    ENDIF( WANT_KDE_SUPPORT )
ENDIF( KDE4_FOUND )

IF( ENABLE_KDE_SUPPORT )
    include_directories( ${KDE4_INCLUDES} )
    add_definitions( ${KDE4_DEFINITIONS} )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    KDE4_ADD_UI_FILES( CANTATA_UI_HDRS ${CANTATA_UIS} )
    KDE4_ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} )
    TARGET_LINK_LIBRARIES( cantata ${KDE4_KDEWEBKIT_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS}
    ${QT_QTWEBKIT_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} Maia )
    install( TARGETS cantata ${INSTALL_TARGETS_DEFAULT_ARGS} )
    install( FILES cantataui.rc DESTINATION ${DATA_INSTALL_DIR}/cantata )
    install( FILES cantata.notifyrc DESTINATION ${DATA_INSTALL_DIR}/cantata )
    ADD_SUBDIRECTORY( icons )
    add_subdirectory(po)
ELSE( ENABLE_KDE_SUPPORT )
    SET( CANTATA_SRCS ${CANTATA_SRCS} widgets/dirrequester.cpp widgets/lineedit.cpp network/networkproxyfactory.cpp network/proxysettings.cpp )
    SET( CANTATA_MOC_HDRS ${CANTATA_MOC_HDRS} widgets/dirrequester.h widgets/lineedit.h network/proxysettings.h )
    SET( CANTATA_UIS ${CANTATA_UIS} network/proxysettings.ui )
    SET( CANTATA_RCS ${CANTATA_RCS} cantata_qt.qrc )
    QT4_ADD_RESOURCES( CANTATA_RC_SRCS ${CANTATA_RCS} )
    QT4_WRAP_UI( CANTATA_UI_HDRS ${CANTATA_UIS} )
    INCLUDE( ${QT_USE_FILE} )
    QT4_WRAP_CPP( CANTATA_MOC_SRCS ${CANTATA_MOC_HDRS} )
    ADD_EXECUTABLE( cantata ${CANTATA_SRCS} ${CANTATA_MOC_SRCS} ${CANTATA_RC_SRCS} ${CANTATA_UI_HDRS} )
    TARGET_LINK_LIBRARIES( cantata ${QT_QTWEBKIT_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} Maia )
    install( TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
    ADD_DEFINITIONS( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
ENDIF( ENABLE_KDE_SUPPORT )

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

ADD_SUBDIRECTORY( libmaia )

IF( UNIX AND NOT APPLE )
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
ENDIF( UNIX AND NOT APPLE )
