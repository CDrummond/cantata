set(CANTATA_MOUNTER_SRCS main.cpp mounter.cpp)
qt5_add_dbus_adaptor(CANTATA_MOUNTER_SRCS mpd.cantata.mounter.xml mounter.h Mounter )

add_executable( cantata-mounter ${CANTATA_MOUNTER_SRCS} )
install(TARGETS cantata-mounter RUNTIME DESTINATION lib/cantata)
configure_file(mpd.cantata.mounter.service.cmake ${CMAKE_CURRENT_BINARY_DIR}/mpd.cantata.mounter.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mpd.cantata.mounter.service DESTINATION ${SHARE_INSTALL_PREFIX}/dbus-1/system-services/ )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mpd.cantata.mounter.conf DESTINATION /etc/dbus-1/system.d/ )

target_link_libraries(cantata-mounter ${Qt5Core_LIBRARIES} ${Qt5DBus_LIBRARIES})
include_directories( ${QTINCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR})

install(FILES ${_qmFile} DESTINATION ${SHARE_INSTALL_PREFIX}/cantata/)
install(PROGRAMS mount.cifs.wrapper DESTINATION ${SHARE_INSTALL_PREFIX}/cantata/scripts/ )
