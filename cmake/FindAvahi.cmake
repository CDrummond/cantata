# Try to find the Avahi libraries and headers
# Once done this will define:
#
#  AVAHI_FOUND          - system has the avahi libraries
#  AVAHI_INCLUDE_DIRS   - the avahi include directories
#  AVAHI_LIBRARIES      - The libraries needed to use avahi


find_library(AVAHI_COMMON_LIB avahi-common PATHS ${COMMON_LIB_DIR})
find_library(AVAHI_CLIENT_LIB avahi-client PATHS ${CLIENT_LIB_DIR})

if(AVAHI_COMMON_LIB AND AVAHI_CLIENT_LIB)
    set(AVAHI_LIBRARIES ${AVAHI_COMMON_LIB} ${AVAHI_CLIENT_LIB})
    message(STATUS "Avahi-Libs found: ${AVAHI_LIBRARIES}")
endif()

find_path(COMMON_INCLUDE_DIR watch.h  PATH_SUFFIXES avahi-common HINTS ${COMMON_INCLUDE_DIR})
find_path(CLIENT_INCLUDE_DIR client.h PATH_SUFFIXES avahi-client HINTS ${CLIENT_INCLUDE_DIR})

if(COMMON_INCLUDE_DIR AND CLIENT_INCLUDE_DIR)
    set(AVAHI_INCLUDE_DIRS ${COMMON_INCLUDE_DIR} ${CLIENT_INCLUDE_DIR})
    message(STATUS "Avahi-Include-Dirs found: ${AVAHI_INCLUDE_DIRS}")
endif()

if(AVAHI_LIBRARIES AND AVAHI_INCLUDE_DIRS)
    set(AVAHI_FOUND TRUE)
endif()
