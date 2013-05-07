configure_file(cantata.iss.cmake ${CMAKE_CURRENT_BINARY_DIR}/cantata.iss)
install(FILES Cantata.README Qt4.LICENSE.LGPL_V2 Qt4.README TagLib.README DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ../LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX} RENAME Cantata.LICENSE)
# Qt translation files...
file(GLOB qt_trans ${QT_TRANSLATIONS_DIR}/qt_*.qm)
foreach(qm ${qt_trans})
    if (NOT ${qm} MATCHES "(${QT_TRANSLATIONS_DIR}/qt_help*)")
       list(APPEND qt_translations "${qm}")
    endif (NOT ${qm} MATCHES "(${QT_TRANSLATIONS_DIR}/qt_help*)")
endforeach(qm ${qt_trans})
install(FILES ${qt_translations} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations/)

# Qt image formats...
install(FILES ${QT_PLUGINS_DIR}/imageformats/qjpeg4.dll ${QT_PLUGINS_DIR}/imageformats/qsvg4.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/imageformats/)

# Qt, taglib, and zlib libraries...
set (CANTATA_WIN_LIBS
    ${QT_BINARY_DIR}/QtCore4.dll ${QT_BINARY_DIR}/QtGui4.dll ${QT_BINARY_DIR}/QtNetwork4.dll ${QT_BINARY_DIR}/QtSvg4.dll ${QT_BINARY_DIR}/QtXml4.dll
    ${ZLIB_INCLUDE_DIR}/../bin/zlib1.dll ${TAGLIB_INCLUDES}/../bin/libtag.dll ${CANTATA_MINGW_LIBS})
install(FILES ${CANTATA_WIN_LIBS} DESTINATION ${CMAKE_INSTALL_PREFIX})

if (CANTATA_OXYGEN_DIR)
    add_subdirectory(icons)
endif (CANTATA_OXYGEN_DIR)
