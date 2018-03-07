set(AVAHI_SRCS avahi.cpp avahiservice.cpp)

qt5_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.Server.xml )
qt5_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.ServiceBrowser.xml)
qt5_add_dbus_interfaces(AVAHI_SRCS org.freedesktop.Avahi.ServiceResolver.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${QTINCLUDES})
add_library(avahi STATIC ${AVAHI_SRCS})
