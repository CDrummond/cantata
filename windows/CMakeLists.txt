set(WINDOWS_APP_NAME Cantata)
set(WINDOWS_COMPANY_NAME Cantata)
set(WINDOWS_URL "http://${PROJECT_URL}")

if (CANTATA_SSL_LIBS)
    set(EXTRA_WIN_LIBS ${EXTRA_WIN_LIBS} ${CANTATA_SSL_LIBS})
    set(CANTATA_SSL_WIN_NSIS_INSTALL "file \"libeay32.dll\"\n    file \"ssleay32.dll\"")
endif (CANTATA_SSL_LIBS)

FILE(TO_CMAKE_PATH $ENV{QTDIR} QT_DIR)
set(QT_BINARY_DIR ${QT_DIR}/bin)
set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)

configure_file(cantata.nsi.cmake ${CMAKE_CURRENT_BINARY_DIR}/cantata.nsi)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cantata.nsi cantata.ico LICENSE.txt "Cantata README.txt" DESTINATION ${CMAKE_INSTALL_PREFIX})

# Qt image formats...
install(FILES ${QT_PLUGINS_DIR}/imageformats/qjpeg.dll ${QT_PLUGINS_DIR}/imageformats/qsvg.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/imageformats/)

# Qt iconengines...
install(FILES ${QT_PLUGINS_DIR}/iconengines/qsvgicon.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/iconengines/)

# Qt windows platform plugin...
install(FILES ${QT_PLUGINS_DIR}/platforms/qwindows.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/platforms/)

# Qt SQL driver...
install(FILES ${QT_PLUGINS_DIR}/sqldrivers/qsqlite.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/sqldrivers/)

# Qt MultiMedia
install(FILES ${QT_PLUGINS_DIR}/mediaservice/dsengine.dll ${QT_PLUGINS_DIR}/mediaservice/qtmedia_audioengine.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/mediaservice/)

# Qt, taglib, and zlib libraries...
set(EXTRA_WIN_LIBS ${EXTRA_WIN_LIBS}
    ${QT_BINARY_DIR}/Qt5Core.dll ${QT_BINARY_DIR}/Qt5Gui.dll ${QT_BINARY_DIR}/Qt5Network.dll ${QT_BINARY_DIR}/Qt5Svg.dll ${QT_BINARY_DIR}/Qt5Xml.dll
    ${QT_BINARY_DIR}/Qt5Widgets.dll ${QT_BINARY_DIR}/Qt5WinExtras.dll ${QT_BINARY_DIR}/Qt5Sql.dll
    ${QT_BINARY_DIR}/libgcc_s_dw2-1.dll ${QT_BINARY_DIR}/libstdc++-6.dll ${QT_BINARY_DIR}/libwinpthread-1.dll
    ${ZLIB_INCLUDE_DIR}/../bin/libz-1.dll ${TAGLIB_INCLUDES}/../bin/libtag.dll)

if (ENABLE_HTTP_STREAM_PLAYBACK)
    set(EXTRA_WIN_LIBS ${EXTRA_WIN_LIBS} ${QT_BINARY_DIR}/Qt5Multimedia.dll)
endif()

# Qt translation files...
file(GLOB qt_trans ${QT_TRANSLATIONS_DIR}/qt_*.qm)
foreach(qm ${qt_trans})
    if (NOT ${qm} MATCHES "(${QT_TRANSLATIONS_DIR}/qt_help*)")
        list(APPEND qt_translations "${qm}")
    endif ()
endforeach()
install(FILES ${qt_translations} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations/)

install(FILES ${EXTRA_WIN_LIBS} DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX} RENAME "Cantata License (GPL V3).txt")
install(FILES ../icons/cantata32.png DESTINATION ${CMAKE_INSTALL_PREFIX}/icons/ RENAME cantata.png)
