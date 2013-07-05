if (ENABLE_KDE_SUPPORT)
    kde4_install_icons(${ICON_INSTALL_DIR})
else (ENABLE_KDE_SUPPORT)
    if (UNIX AND NOT APPLE)
        macro (update_iconcache)
            # Update mtime of hicolor icon theme dir.
            # We don't always have touch command (e.g. on Windows), so instead create
            #  and delete a temporary file in the theme dir.
            install(CODE "
            set(DESTDIR_VALUE \"\$ENV{DESTDIR}\")
            if (NOT DESTDIR_VALUE)
                file(WRITE \"${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/temp.txt\" \"update\")
                file(REMOVE \"${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/temp.txt\")
            endif (NOT DESTDIR_VALUE)
            ")
        endmacro (update_iconcache)

        install(FILES cantata.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
        install(FILES cantata16.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/16x16/apps RENAME cantata.png)
        install(FILES cantata22.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/22x22/apps RENAME cantata.png)
        install(FILES cantata32.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/32x32/apps RENAME cantata.png)
        install(FILES cantata48.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/48x48/apps RENAME cantata.png)
        install(FILES cantata64.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/64x64/apps RENAME cantata.png)
        update_iconcache()
    endif (UNIX AND NOT APPLE)
endif (ENABLE_KDE_SUPPORT)

