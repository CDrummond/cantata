if (CANTATA_CS_TRANSLATION)
    LIST(APPEND CANTATA_LANGUAGES "cs")
endif (CANTATA_CS_TRANSLATION)

if (CANTATA_DE_TRANSLATION)
    LIST(APPEND CANTATA_LANGUAGES "de")
endif (CANTATA_DE_TRANSLATION)

if (CANTATA_ES_TRANSLATION)
    LIST(APPEND CANTATA_LANGUAGES "es")
endif (CANTATA_ES_TRANSLATION)

if (CANTATA_PL_TRANSLATION)
    LIST(APPEND CANTATA_LANGUAGES "pl")
endif (CANTATA_PL_TRANSLATION)

IF( ENABLE_KDE_SUPPORT )
    find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

    if(NOT GETTEXT_MSGFMT_EXECUTABLE)
        message("------\n"
                "NOTE: msgfmt not found. Translations will *not* be installed\n"
                "------\n")
    else(NOT GETTEXT_MSGFMT_EXECUTABLE)
        set(catalogname cantata)
        add_custom_target(translations ALL)
        foreach(_lang ${CANTATA_LANGUAGES})
            set(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
            add_custom_command(TARGET translations
                    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_gmoFile} ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po
                    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
            install(FILES ${_gmoFile} DESTINATION ${LOCALE_INSTALL_DIR}/${_langCode}/LC_MESSAGES/ RENAME ${catalogname}.mo)
        endforeach(_lang ${CANTATA_LANGUAGES})
    endif(NOT GETTEXT_MSGFMT_EXECUTABLE)
else (ENABLE_KDE_SUPPORT)
    find_program(LCONVERT_EXECUTABLE lconvert)
    find_program(LRELEASE_EXECUTABLE lrelease)

    if(NOT LCONVERT_EXECUTABLE OR NOT LRELEASE_EXECUTABLE)
        message("------\n"
                "NOTE: lconvert and/or lrelease not found. Translations will *not* be installed\n"
                "------\n")
    else(NOT LCONVERT_EXECUTABLE OR NOT LRELEASE_EXECUTABLE)
        set(catalogname cantata)
        add_custom_target(translations ALL)
        foreach(_lang ${CANTATA_LANGUAGES})
            set(_poFile ${_lang}.po)
            set(_tsFile ${CMAKE_CURRENT_BINARY_DIR}/cantata_${_lang}.ts)
            set(_qmFile ${CMAKE_CURRENT_BINARY_DIR}/cantata_${_lang}.qm)
            add_custom_command(TARGET translations
                COMMAND ${LCONVERT_EXECUTABLE} ARGS ${_poFile} -o ${_tsFile} -of ts
                DEPENDS ${_poFile}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            add_custom_command(TARGET translations
                COMMAND ${LRELEASE_EXECUTABLE} ARGS ${_tsFile} -qm ${_qmFile}
                DEPENDS ${_tsFile})
            install(FILES ${_qmFile} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/cantata/translations/)
        endforeach(_lang ${CANTATA_LANGUAGES})
    endif(NOT LCONVERT_EXECUTABLE OR NOT LRELEASE_EXECUTABLE)
endif (ENABLE_KDE_SUPPORT)

