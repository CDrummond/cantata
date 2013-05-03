file(GLOB svg_files *.svg)
file(GLOB png_files *.png)
install(FILES ${svg_files} ${png_files} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/streamicons/)
