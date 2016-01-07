install(FILES ${MACOSX_BUNDLE_ICON_FILE} DESTINATION ${MACOSX_BUNDLE_RESOURCES})

# Qt translation files...
file(GLOB qt_trans ${QT_TRANSLATIONS_DIR}/qt_*.qm)
foreach(qm ${qt_trans})
    if (NOT ${qm} MATCHES "(${QT_TRANSLATIONS_DIR}/qt_help*)")
       list(APPEND qt_translations "${qm}")
    endif (NOT ${qm} MATCHES "(${QT_TRANSLATIONS_DIR}/qt_help*)")
endforeach(qm ${qt_trans})
install(FILES ${qt_translations} DESTINATION ${MACOSX_BUNDLE_RESOURCES}/translations/)
