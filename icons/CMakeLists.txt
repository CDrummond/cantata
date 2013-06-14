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
        endmacro (UPDATE_ICONCACHE)

        install(FILES cantata.svg DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
        update_iconcache()
    endif (UNIX AND NOT APPLE)
endif (ENABLE_KDE_SUPPORT)

