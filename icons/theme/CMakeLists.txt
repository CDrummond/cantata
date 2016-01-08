set(ICON_FILES
svg/audio-x-generic.svg
svg/information.svg
svg/dynamic-playlist.svg
svg/folder-downloads.svg
svg/folder-temp.svg
svg/fork.svg
svg/inode-directory.svg
svg/playlist.svg
svg/preferences-desktop-keyboard.svg
svg/preferences-other.svg
svg/speaker.svg
svg64/dialog-error.svg
svg64/dialog-information.svg
svg64/dialog-question.svg
svg64/dialog-warning.svg
)

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

if (NOT WIN32 AND NOT APPLE)
    install(FILES ../cantata.svg DESTINATION ${CANTATA_ICON_THEME_PATH}/svg)
endif (NOT WIN32 AND NOT APPLE)

