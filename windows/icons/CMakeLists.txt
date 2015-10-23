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
16/media-optical.png
22/media-optical.png
32/media-optical.png
48/media-optical.png
64/media-optical.png
128/media-optical.png
256/media-optical.png
)

foreach(ICON ${ICON_FILES})
    get_filename_component(ICON_PATH ${ICON} PATH)
    install(FILES ${ICON} DESTINATION ${CANTATA_ICON_INSTALL_PREFIX}/${ICON_PATH})
endforeach(ICON ${ICON_FILES})
install(FILES AUTHORS index.theme LICENSE DESTINATION ${CANTATA_ICON_INSTALL_PREFIX})
