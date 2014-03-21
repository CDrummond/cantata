if (WIN32)
    set(STREAM_PROVIDER_PREFIX ${CMAKE_INSTALL_PREFIX}/streams)
else (WIN32)
    set(STREAM_PROVIDER_PREFIX ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/streams)
endif (WIN32)

install(FILES 1.fm.png DESTINATION ${STREAM_PROVIDER_PREFIX}/1.fm RENAME icon.png)
install(FILES 1.fm.xml.gz DESTINATION ${STREAM_PROVIDER_PREFIX}/1.fm RENAME streams.xml.gz)

install(FILES ../../online/icons/jamendo.svg DESTINATION ${STREAM_PROVIDER_PREFIX}/Jamendo RENAME icon.svg)
install(FILES jamendo.xml.gz DESTINATION ${STREAM_PROVIDER_PREFIX}/Jamendo RENAME streams.xml.gz)
install(FILES ../listenlive.xml DESTINATION ${STREAM_PROVIDER_PREFIX})
