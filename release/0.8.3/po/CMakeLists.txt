if (NOT CANTATA_TRANSLATIONS)
    set(CANTATA_TRANSLATIONS "all")
endif (NOT CANTATA_TRANSLATIONS)

foreach(trans ${CANTATA_TRANSLATIONS})
    if (trans STREQUAL "all")
        file(GLOB avail_pofiles *.po)
        foreach(PO_FILE ${avail_pofiles})
            get_filename_component(lang ${PO_FILE} NAME_WE)
            list(APPEND CANTATA_LANGUAGES "${lang}")
        endforeach(PO_FILE ${avail_pofiles})
    else (trans STREQUAL "all")
        list(APPEND CANTATA_LANGUAGES "${trans}")
    endif (trans STREQUAL "all")
endforeach(trans ${CANTATA_TRANSLATIONS})

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
            if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
                set(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
                add_custom_command(TARGET translations
                        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_gmoFile} ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po
                        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
                install(FILES ${_gmoFile} DESTINATION ${LOCALE_INSTALL_DIR}/${_lang}/LC_MESSAGES/ RENAME ${catalogname}.mo)
            endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
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
            if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
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
            endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_lang}.po)
        endforeach(_lang ${CANTATA_LANGUAGES})
    endif(NOT LCONVERT_EXECUTABLE OR NOT LRELEASE_EXECUTABLE)
endif (ENABLE_KDE_SUPPORT)

