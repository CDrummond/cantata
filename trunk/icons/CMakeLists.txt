#if (ENABLE_KDE_SUPPORT)
#    kde4_install_icons( ${DATA_INSTALL_DIR}/cantata/icons)
#else (ENABLE_KDE_SUPPORT)
    if (WIN32 OR APPLE)
        install(FILES cantata.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/icons)
    else (WIN32 OR APPLE)
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

        install(FILES cantata.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
        install(FILES cantata16.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/16x16/apps RENAME cantata.png)
        install(FILES cantata22.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/22x22/apps RENAME cantata.png)
        install(FILES cantata24.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/24x24/apps RENAME cantata.png)
        install(FILES cantata32.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/32x32/apps RENAME cantata.png)
        install(FILES cantata48.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/48x48/apps RENAME cantata.png)
        install(FILES cantata64.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/64x64/apps RENAME cantata.png)

        update_iconcache(hicolor)

        if (EXISTS /etc/lsb-release)
            file(READ "/etc/lsb-release" LSB_RELEASE_CONTENTS)
            string(REGEX MATCH "DISTRIB_ID=Ubuntu" IS_UBUNTU ${LSB_RELEASE_CONTENTS})
            if (IS_UBUNTU)
                set(INSTALL_UBUNTU_ICONS_DEFAULT ON)
            else(IS_UBUNTU)
                set(INSTALL_UBUNTU_ICONS_DEFAULT OFF)
            endif(IS_UBUNTU)
        else(EXISTS /etc/lsb-release)
            set(INSTALL_UBUNTU_ICONS_DEFAULT OFF)
        endif(EXISTS /etc/lsb-release)
        option(INSTALL_UBUNTU_ICONS "Install the Ubuntu themed monochrome panel icons" ${INSTALL_UBUNTU_ICONS_DEFAULT})

        if (INSTALL_UBUNTU_ICONS)
            install(FILES trayicon-mono-dark.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/ubuntu-mono-light/apps/22 RENAME cantata-panel.svg)
            install(FILES trayicon-mono-light.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/ubuntu-mono-dark/apps/22 RENAME cantata-panel.svg)
            update_iconcache(ubuntu-mono-light)
            update_iconcache(ubuntu-mono-dark)
        endif (INSTALL_UBUNTU_ICONS)

    endif (WIN32 OR APPLE)
#endif (ENABLE_KDE_SUPPORT)

