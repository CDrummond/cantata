file(GLOB svg_pofiles *.svg)
install(FILES ${svg_pofiles} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/streamicons/)
