find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

if(NOT GETTEXT_MSGFMT_EXECUTABLE)
    message("------\n"
            "NOTE: msgfmt not found. Translations will *not* be installed\n"
            "------\n")
else(NOT GETTEXT_MSGFMT_EXECUTABLE)
    set(catalogname cantata)
    add_custom_target(translations ALL)
    file(GLOB PO_FILES *.po)
    foreach(_poFile ${PO_FILES})
        get_filename_component(_poFileName ${_poFile} NAME)
        string(REGEX REPLACE "^${catalogname}_?" "" _langCode ${_poFileName} )
        string(REGEX REPLACE "\\.po$" "" _langCode ${_langCode} )
        if(_langCode)
            get_filename_component(_lang ${_poFile} NAME_WE)
            set(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
            add_custom_command(TARGET translations
                    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_gmoFile} ${_poFile}
                    DEPENDS ${_poFile})
            install(FILES ${_gmoFile} DESTINATION ${LOCALE_INSTALL_DIR}/${_langCode}/LC_MESSAGES/ RENAME ${catalogname}.mo)
        endif( _langCode )
    endforeach(_poFile ${PO_FILES})
endif(NOT GETTEXT_MSGFMT_EXECUTABLE)

