set(ICON_FILES
svg/audio-x-generic.svg
svg/information.svg
svg/folder-temp.svg
svg/fork.svg
svg/inode-directory.svg
svg/network-server-database.svg
svg/preferences-desktop-keyboard.svg
svg/preferences-other.svg
svg/speaker.svg
svg/key.svg
svg64/dialog-error.svg
svg64/dialog-information.svg
svg64/dialog-question.svg
svg64/dialog-warning.svg
)

if (ENABLE_PROXY_CONFIG)
    set(ICON_FILES svg/preferences-system-network.svg ${ICON_FILES})
endif (ENABLE_PROXY_CONFIG)

if (ENABLE_DEVICES_SUPPORT)
    set(ICON_FILES svg/drive-removable-media-usb-pendrive.svg svg/multimedia-player.svg ${ICON_FILES})
    if (ENABLE_REMOTE_DEVICES)
        set(ICON_FILES svg/folder-network.svg svg/folder-samba.svg ${ICON_FILES})
    endif (ENABLE_REMOTE_DEVICES)
endif (ENABLE_DEVICES_SUPPORT)

if (WIN32 OR APPLE)
    set(CANTATA_ICON_THEME_PATH ${CANTATA_ICON_INSTALL_PREFIX})
else (WIN32 OR APPLE)
    set(CANTATA_ICON_THEME_PATH ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/icons/cantata)
endif (WIN32 OR APPLE)

foreach(ICON ${ICON_FILES})
    get_filename_component(ICON_PATH ${ICON} PATH)
    install(FILES ${ICON} DESTINATION ${CANTATA_ICON_THEME_PATH}/${ICON_PATH})
endforeach(ICON ${ICON_FILES})
install(FILES AUTHORS index.theme LICENSE DESTINATION ${CANTATA_ICON_THEME_PATH})

install(FILES ../cantata128.png DESTINATION ${CANTATA_ICON_THEME_PATH}/128 RENAME cantata.png)
if (NOT WIN32 AND NOT APPLE)
    install(FILES ../cantata.svg DESTINATION ${CANTATA_ICON_THEME_PATH}/svg)
endif (NOT WIN32 AND NOT APPLE)

