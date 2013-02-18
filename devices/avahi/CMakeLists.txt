SET( AVAHI_SRCS
    avahi.cpp
    avahiservice.cpp
)

SET( AVAHI_MOC_HDRS
    avahi.h
    avahiservice.h
)

qt4_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.Server.xml )
qt4_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.ServiceBrowser.xml)
qt4_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.ServiceResolver.xml)

if (ENABLE_QT5)
    QT5_WRAP_CPP( AVAHI_MOC_SRCS ${AVAHI_MOC_HDRS} )
else (ENABLE_QT5)
    QT4_WRAP_CPP( AVAHI_MOC_SRCS ${AVAHI_MOC_HDRS} )
endif (ENABLE_QT5)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${QTINCLUDES} )
add_library( avahi STATIC ${AVAHI_MOC_SRCS} ${AVAHI_SRCS} )
