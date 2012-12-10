set( CANTATA_MOUNTER_SRCS main.cpp mounter.cpp )
set( CANTATA_MOUNTER_MOC_HDRS mounter.h )
qt4_wrap_cpp( CANTATA_MOUNTER_MOC_SRCS ${CANTATA_MOUNTER_MOC_HDRS} )
qt4_add_dbus_adaptor(CANTATA_MOUNTER_SRCS com.googlecode.cantata.mounter.xml mounter.h Mounter )

add_executable( cantata-mounter ${CANTATA_MOUNTER_SRCS} ${CANTATA_MOUNTER_MOC_SRCS} )
install( TARGETS cantata-mounter RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib )
add_definitions( -DQT_NO_STL -DQT_NO_CAST_TO_ASCII -Wall -Wextra )
configure_file(com.googlecode.cantata.mounter.service.cmake ${CMAKE_CURRENT_BINARY_DIR}/com.googlecode.cantata.mounter.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.googlecode.cantata.mounter.service DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services/ )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/com.googlecode.cantata.mounter.conf DESTINATION /etc/dbus-1/system.d/ )

target_link_libraries(cantata-mounter ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY})
include_directories( ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR})
