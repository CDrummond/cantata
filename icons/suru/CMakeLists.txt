set(CANTATA_SURU_ICON_INSTALL_PREFIX ${SHARE_INSTALL_PREFIX}/icons/Suru)

install(FILES ../cantata-symbolic.svg DESTINATION ${CANTATA_SURU_ICON_INSTALL_PREFIX}/scalable/apps)
install(FILES ../cantata.svg DESTINATION ${CANTATA_SURU_ICON_INSTALL_PREFIX}/scalable/apps RENAME cantata-panel.svg)

foreach (SIZE 16 24 32 48 256)
    foreach (DPI "" @2x)
        install(FILES cantata-${SIZE}x${SIZE}${DPI}.png DESTINATION ${CANTATA_SURU_ICON_INSTALL_PREFIX}/${SIZE}x${SIZE}${DPI}/apps RENAME cantata.png)
    endforeach()
endforeach()

