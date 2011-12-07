cmake_minimum_required( VERSION 2.6 )

PROJECT( cantata )

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(DEBIAN_PACKAGE_DESCRIPTION "Qt4/KDE4 Client for MPD")
set(DEBIAN_PACKAGE_SECTION "kde4")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
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
    gui/playbacksettings.cpp
    gui/serversettings.cpp
    gui/outputsettings.cpp
    gui/streamspage.cpp
    gui/streamdialog.cpp
    gui/librarypage.cpp
    gui/folderpage.cpp
    gui/playlistspage.cpp
    models/musiclibraryitem.cpp
    models/musiclibraryitemroot.cpp
    models/musiclibraryitemartist.cpp
    models/musiclibraryitemalbum.cpp
    models/musiclibraryitemsong.cpp
    models/musiclibrarymodel.cpp
    models/musiclibraryproxymodel.cpp
    models/playlistsmodel.cpp
    models/playlistsproxymodel.cpp
    models/playlisttablemodel.cpp
    models/playlisttableproxymodel.cpp
    models/dirviewmodel.cpp
    models/dirviewproxymodel.cpp
    models/dirviewitem.cpp
    models/dirviewitemfile.cpp
    models/dirviewitemdir.cpp
    models/dirviewitemroot.cpp
    models/streamsmodel.cpp
    models/streamsproxymodel.cpp
    mpd/mpdconnection.cpp
    mpd/mpdparseutils.cpp
    mpd/mpdstats.cpp
    mpd/mpdstatus.cpp
    mpd/song.cpp
    widgets/treeview.cpp
    widgets/fancytabwidget.cpp
    lyrics/lyricspage.cpp
    lyrics/lyricsettings.cpp
    lyrics/ultimatelyricsprovider.cpp
    lyrics/ultimatelyricsreader.cpp
    lyrics/songinfoprovider.cpp
    network/networkaccessmanager.cpp
)

SET( CANTATA_MOC_HDRS
    gui/mainwindow.h
    gui/updatedialog.h
    gui/covers.h
    gui/folderpage.h
    gui/librarypage.h
    gui/playlistspage.h
    gui/streamspage.h
    gui/streamdialog.h
    gui/interfacesettings.h
    gui/playbacksettings.h
    gui/serversettings.h
    gui/outputsettings.h
    gui/preferencesdialog.h
    models/musiclibrarymodel.h
    models/musiclibraryproxymodel.h
    models/playlistsmodel.h
    models/playlistsproxymodel.h
    models/playlisttablemodel.h
    models/playlisttableproxymodel.h
    models/dirviewmodel.h
    models/dirviewproxymodel.h
    models/streamsproxymodel.h
    mpd/mpdconnection.h
    mpd/song.h
    widgets/fancytabwidget.h
    widgets/treeview.h
    lyrics/lyricspage.h
    lyrics/lyricsettings.h
    lyrics/ultimatelyricsprovider.h
    lyrics/songinfoprovider.h
    network/networkaccessmanager.h
)

SET( CANTATA_UIS
    gui/mainwindow.ui
    gui/folderpage.ui
    gui/librarypage.ui
    gui/playlistspage.ui
    gui/streamspage.ui
    gui/interfacesettings.ui
    gui/playbacksettings.ui
    gui/serversettings.ui
    gui/outputsettings.ui
    lyrics/lyricspage.ui
    lyrics/lyricsettings.ui
)

SET( CANTATA_RCS cantata.qrc )

include_directories( ${CMAKE_SOURCE_DIR}/libmaia
                     ${CMAKE_SOURCE_DIR}/gui
                     ${CMAKE_SOURCE_DIR}/mpd
                     ${CMAKE_SOURCE_DIR}/models
                     ${CMAKE_SOURCE_DIR}/widgets
                     ${CMAKE_SOURCE_DIR}/lyrics
                     ${CMAKE_SOURCE_DIR}/network
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
)

SET( QT_USE_QTNETWORK TRUE )
SET( QT_USE_QTXML TRUE )

# this command finds Qt4 libraries and sets all required variables
FIND_PACKAGE( Qt4 REQUIRED QtCore QtGui QtXml QtNetwork )
FIND_PACKAGE( KDE4 )

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

IF( NOT DEFINED WANT_KDE_SUPPORT )
    SET( WANT_KDE_SUPPORT ON )
ENDIF( NOT DEFINED WANT_KDE_SUPPORT )

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
    TARGET_LINK_LIBRARIES( cantata ${QT_LIBRARIES} ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} Maia )
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
    TARGET_LINK_LIBRARIES( cantata ${QT_LIBRARIES} Maia )
    TARGET_LINK_LIBRARIES( cantata ${QT_LIBRARIES}  ${KDE4_KIO_LIBS} Maia )
    install( TARGETS cantata RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
    ADD_DEFINITIONS( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
ENDIF( ENABLE_KDE_SUPPORT )

# enable warnings
ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT )

ADD_SUBDIRECTORY( libmaia )

IF( UNIX AND NOT APPLE )
    install(FILES cantata.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
ENDIF( UNIX AND NOT APPLE )
