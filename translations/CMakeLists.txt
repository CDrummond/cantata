file(GLOB TS_FILES *.ts)
foreach (TS_FILE ${TS_FILES})
    get_filename_component(TRANS ${TS_FILE} NAME_WE)
    list(APPEND CANTATA_TRANS "${TRANS}")
endforeach (TS_FILE ${TS_FILES})

find_program(LRELEASE_EXECUTABLE lrelease)

if (NOT LRELEASE_EXECUTABLE)
    message("------\n"
            "NOTE: lrelease not found. Translations will *not* be installed\n"
            "------\n")
else (NOT LRELEASE_EXECUTABLE)
    set(catalogname cantata)
    add_custom_target(translations ALL)
    foreach(TRANS ${CANTATA_TRANS})
        if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TRANS}.ts)
            set(CUR_TS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${TRANS}.ts)
            set(CUR_QM_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TRANS}.qm)
            add_custom_command(TARGET translations
                COMMAND ${LRELEASE_EXECUTABLE} ARGS ${CUR_TS_FILE} -qm ${CUR_QM_FILE}
                DEPENDS ${CUR_TS_FILE})
            if (WIN32 OR ENABLE_UBUNTU)
                install(FILES ${CUR_QM_FILE} DESTINATION ${CMAKE_INSTALL_PREFIX}/translations/)
            elseif (APPLE)
                install(FILES ${CUR_QM_FILE} DESTINATION ${MACOSX_BUNDLE_RESOURCES}/translations/)
            else (WIN32 OR APPLE OR ENABLE_UBUNTU)
                install(FILES ${CUR_QM_FILE} DESTINATION ${SHARE_INSTALL_PREFIX}/cantata/translations/)
            endif (WIN32 OR ENABLE_UBUNTU)
        endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TRANS}.po)
    endforeach(TRANSLATION ${CANTATA_TRANS})
endif (NOT LRELEASE_EXECUTABLE)

