if (NOT HAIKU AND NOT WIN32 AND NOT APPLE)
    macro (update_iconcache ICON_THEME)
        # Update mtime of hicolor icon theme dir.
        # We don't always have touch command (e.g. on Windows), so instead create
        #  and delete a temporary file in the theme dir.
        install(CODE "
        set(DESTDIR_VALUE \"\$ENV{DESTDIR}\")
        if (NOT DESTDIR_VALUE)
            file(WRITE \"${CMAKE_INSTALL_PREFIX}/share/icons/${ICON_THEME}/temp.txt\" \"update\")
            file(REMOVE \"${CMAKE_INSTALL_PREFIX}/share/icons/${ICON_THEME}/temp.txt\")
        endif (NOT DESTDIR_VALUE)
        ")
    endmacro (update_iconcache)

    install(FILES cantata.svg DESTINATION ${CANTATA_ICON_INSTALL_PREFIX}/scalable/apps)
    install(FILES cantata-symbolic.svg DESTINATION ${CANTATA_ICON_INSTALL_PREFIX}/symbolic/apps)
    foreach (SIZE 16 22 24 32 48 64 128 256 512)
        install(FILES cantata${SIZE}.png DESTINATION ${CANTATA_ICON_INSTALL_PREFIX}/${SIZE}x${SIZE}/apps RENAME cantata.png)
           endforeach()
    update_iconcache(hicolor)

    #if (EXISTS /etc/lsb-release)
    #    file(READ "/etc/lsb-release" LSB_RELEASE_CONTENTS)
    #    string(REGEX MATCH "DISTRIB_ID=Ubuntu" IS_UBUNTU ${LSB_RELEASE_CONTENTS})
    #	if (IS_UBUNTU)
    #        set(INSTALL_UBUNTU_ICONS_DEFAULT ON)
    #    else(IS_UBUNTU)
    #        set(INSTALL_UBUNTU_ICONS_DEFAULT OFF)
    #    endif(IS_UBUNTU)
    #else(EXISTS /etc/lsb-release)
        set(INSTALL_UBUNTU_ICONS_DEFAULT OFF)
    #endif(EXISTS /etc/lsb-release)
    option(INSTALL_UBUNTU_ICONS "Install the Ubuntu Yaru icons" ${INSTALL_UBUNTU_ICONS_DEFAULT})

    if (INSTALL_UBUNTU_ICONS)
        add_subdirectory(yaru)
        update_iconcache(Yaru)
    endif ()
endif ()
