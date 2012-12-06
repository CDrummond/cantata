INCLUDE(MacroOptionalFindPackage)
INCLUDE(MacroLogFeature)

add_subdirectory( ifaces )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  ${QT_INCLUDES})

file(MAKE_DIRECTORY
   ${CMAKE_CURRENT_BINARY_DIR}/backends/hal
   ${CMAKE_CURRENT_BINARY_DIR}/backends/udev
   ${CMAKE_CURRENT_BINARY_DIR}/backends/wmi
)

set(solidlite_LIB_SRCS
   solidnamespace.cpp
   managerbase.cpp
   device.cpp
   devicemanager.cpp
   deviceinterface.cpp
   genericinterface.cpp
   block.cpp
   storagedrive.cpp
   storagevolume.cpp
   storageaccess.cpp
   portablemediaplayer.cpp
   predicate.cpp
   predicateparse.cpp
   predicate_lexer.c
   predicate_parser.c
   xdgbasedirs.cpp

   ifaces/block.cpp
   ifaces/device.cpp
   ifaces/deviceinterface.cpp
   ifaces/devicemanager.cpp
   ifaces/genericinterface.cpp
   ifaces/portablemediaplayer.cpp
   ifaces/storagedrive.cpp
   ifaces/storagevolume.cpp
   ifaces/storageaccess.cpp

   backends/shared/rootdevice.cpp
)

set(solidlite_LIB_MOC_HDRS
    block.h
    deviceinterface.h
    devicemanager_p.h
    devicenotifier.h
    device_p.h
    genericinterface.h
    portablemediaplayer.h
    storageaccess.h
    storagedrive.h
    storagevolume.h
    ifaces/device.h
    ifaces/devicemanager.h
    backends/shared/rootdevice.h
)
   macro_optional_find_package( UDev )
   macro_log_feature( UDEV_FOUND "UDev" "UDev support for Solid" "http://www.kernel.org/pub/linux/utils/kernel/hotplug/udev.html" FALSE "" "Allows Solid to use UDev to provide information about devices on Linux" )
   configure_file( config-solid.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-solid.h )


   if ( UDEV_FOUND )
      message(STATUS "Building Solid UDev backend." )
      set(solidlite_LIB_SRCS ${solidlite_LIB_SRCS}
      backends/udev/udevdevice.cpp
      backends/udev/udevmanager.cpp
      backends/udev/udevdeviceinterface.cpp
      backends/udev/udevgenericinterface.cpp
      backends/udev/udevportablemediaplayer.cpp
      backends/udev/udevblock.cpp
      backends/shared/udevqtclient.cpp
      backends/shared/udevqtdevice.cpp
      )

      set(solidlite_LIB_MOC_HDRS ${solidlite_LIB_MOC_HDRS}
      backends/udev/udevblock.h
      backends/udev/udevdevice.h
      backends/udev/udevdeviceinterface.h
      backends/udev/udevgenericinterface.h
      backends/udev/udevmanager.h
      backends/udev/udevportablemediaplayer.h
      backends/shared/udevqt.h
      )
      # check for media-player-info (runtime-only optional dependency)
      set(XDG_DATA_DIRS_ENV $ENV{XDG_DATA_DIRS})  # if(ENV{..}) does not work for me
      if(XDG_DATA_DIRS_ENV)
         find_path(MEDIAPLAYERINFO_PATH sony_psp.mpi
            PATHS ENV XDG_DATA_DIRS
            PATH_SUFFIXES "media-player-info" NO_DEFAULT_PATH
         )
      else(XDG_DATA_DIRS_ENV)
         set(XDG_DATA_DIRS "/usr/share")
         message(STATUS "Warning: environment variable XDG_DATA_DIRS not set, falling back to ${XDG_DATA_DIRS}")
         find_path(MEDIAPLAYERINFO_PATH sony_psp.mpi
            PATHS "${XDG_DATA_DIRS}"
            PATH_SUFFIXES "media-player-info" NO_DEFAULT_PATH
         )
      endif(XDG_DATA_DIRS_ENV)

      macro_log_feature(MEDIAPLAYERINFO_PATH
         "media-player-info"
         "Enables identification and querying of portable media players"
         "http://www.freedesktop.org/wiki/Software/media-player-info"
         FALSE
         ""
         "Runtime-only dependency of the udev solid backend. Support for m-p-i is included even if not found during build"
      )
   endif( UDEV_FOUND )


   message(STATUS "Building Solid HAL backend." )
   set(solidlite_LIB_SRCS ${solidlite_LIB_SRCS}
   backends/hal/halblock.cpp
   backends/hal/haldeviceinterface.cpp
   backends/hal/halfstabhandling.cpp
   backends/hal/halgenericinterface.cpp
   backends/hal/haldevice.cpp
   backends/hal/halmanager.cpp
   backends/hal/halportablemediaplayer.cpp
   backends/hal/halstorageaccess.cpp
   backends/hal/halstorage.cpp
   backends/hal/halvolume.cpp
   )
   set(solidlite_LIB_MOC_HDRS ${solidlite_LIB_MOC_HDRS}
   backends/hal/halblock.h
   backends/hal/haldevice.h
   backends/hal/haldeviceinterface.h
   backends/hal/halgenericinterface.h
   backends/hal/halmanager.h
   backends/hal/halportablemediaplayer.h
   backends/hal/halstorageaccess.h
   backends/hal/halstorage.h
   backends/hal/halvolume.h
   )

   # FIXME: this should work on more Unix systems
   if (CMAKE_SYSTEM_NAME MATCHES Linux)
      message(STATUS "Building Solid UDisks backend." )
      set(solidlite_LIB_SRCS ${solidlite_LIB_SRCS}
      backends/udisks/udisksmanager.cpp
      backends/udisks/udisksdevice.cpp
      backends/udisks/udisksblock.cpp
      backends/udisks/udisksstoragevolume.cpp
      backends/udisks/udisksdeviceinterface.cpp
      backends/udisks/udisksstoragedrive.cpp
      backends/udisks/udisksstorageaccess.cpp
      backends/udisks/udisksgenericinterface.cpp
      )
      set(solidlite_LIB_MOC_HDRS ${solidlite_LIB_MOC_HDRS}
      backends/udisks/udisksblock.h
      backends/udisks/udisksdevice.h
      backends/udisks/udisksdeviceinterface.h
      backends/udisks/udisksgenericinterface.h
      backends/udisks/udisksmanager.h
      backends/udisks/udisksstorageaccess.h
      backends/udisks/udisksstoragedrive.h
      backends/udisks/udisksstoragevolume.h
      )
   endif (CMAKE_SYSTEM_NAME MATCHES Linux)

if(APPLE)
   find_package(IOKit REQUIRED)

   message(STATUS "-- Building Solid IOKit backend." )
   set(solidlite_LIB_SRCS ${solidlite_LIB_SRCS}
   backends/iokit/iokitmanager.cpp
   backends/iokit/iokitdevice.cpp
   backends/iokit/cfhelper.cpp
   backends/iokit/iokitdeviceinterface.cpp
   backends/iokit/iokitgenericinterface.cpp
   )
   set(solidlite_LIB_MOC_HDRS ${solidlite_LIB_MOC_HDRS}
   backends/iokit/iokitdevice.h
   backends/iokit/iokitdeviceinterface.h
   backends/iokit/iokitgenericinterface.h
   backends/iokit/iokitmanager.h
   )
endif(APPLE)

if(WIN32)
   include(CheckIncludeFileCXX)
   check_include_file_cxx(wbemidl.h HAVE_WBEM)
   FIND_LIBRARY(WBEM_LIBRARIES NAMES wbemuuid wbemuuidd)
   if(HAVE_WBEM AND WBEM_LIBRARIES)
      set(HAVE_WBEM True)
      message(STATUS "Found wbemuuid library: ${WBEM_LIBRARIES}")
   else(HAVE_WBEM AND WBEM_LIBRARIES)
      set(HAVE_WBEM False)
   endif(HAVE_WBEM AND WBEM_LIBRARIES)
   if(HAVE_WBEM AND NOT WINCE)
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_WBEM")
       message(STATUS "-- Building Solid WMI backend." )

       set(solidlite_LIB_SRCS ${solidlite_LIB_SRCS}
       backends/wmi/wmiblock.cpp
       backends/wmi/wmideviceinterface.cpp
       backends/wmi/wmigenericinterface.cpp
       backends/wmi/wmidevice.cpp
       backends/wmi/wmimanager.cpp
       backends/wmi/wmiportablemediaplayer.cpp
       backends/wmi/wmiquery.cpp
       backends/wmi/wmistorageaccess.cpp
       backends/wmi/wmistorage.cpp
       backends/wmi/wmivolume.cpp

       )
       set(solidlite_LIB_MOC_HDRS ${solidlite_LIB_MOC_HDRS}
       backends/wmi/wmiblock.h
       backends/wmi/wmidevice.h
       backends/wmi/wmideviceinterface.h
       backends/wmi/wmigenericinterface.h
       backends/wmi/wmimanager.h
       backends/wmi/wmiportablemediaplayer.h
       backends/wmi/wmistorageaccess.h
       backends/wmi/wmistorage.h
       backends/wmi/wmivolume.h
       )
    endif(HAVE_WBEM AND NOT WINCE)
endif(WIN32)

set(solidlite_OPTIONAL_LIBS)

if(WIN32)
   set(solidlite_OPTIONAL_LIBS ${solidlite_OPTIONAL_LIBS} ${KDEWIN_LIBRARY})
   if(HAVE_WBEM)
      set(solidlite_OPTIONAL_LIBS ${solidlite_OPTIONAL_LIBS} ${WBEM_LIBRARIES})
   endif(HAVE_WBEM)
endif(WIN32)

if(APPLE)
   set(solidlite_OPTIONAL_LIBS ${IOKIT_LIBRARY})
endif(APPLE)

QT4_WRAP_CPP(solidlite_LIB_MOC_SRCS ${solidlite_LIB_MOC_HDRS} )
add_library(solidlite ${solidlite_LIB_SRCS} ${solidlite_LIB_MOC_SRCS})
# set_target_properties(solidlite PROPERTIES AUTOMOC TRUE)

if ( UDEV_FOUND )
   set(solidlite_OPTIONAL_LIBS ${solidlite_OPTIONAL_LIBS} ${UDEV_LIBS})
endif ( UDEV_FOUND )

target_link_libraries(solidlite ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} ${QT_QTXML_LIBRARY} ${QT_QTGUI_LIBRARY} ${solidlite_OPTIONAL_LIBS} )
target_link_libraries(solidlite LINK_INTERFACE_LIBRARIES ${QT_CORE_LIBRARY} )

if (WINCE)
   target_link_libraries(solidlite ${WCECOMPAT_LIBRARIES})
endif(WINCE)

# set_target_properties(solidlite PROPERTIES
#    VERSION ${GENERIC_LIB_VERSION}
#    SOVERSION ${GENERIC_LIB_SOVERSION}
# )

#set(lexer_FILE predicate_lexer)
#set(parser_FILE predicate_parser)

#find_package(Flex)
#macro_log_feature(FLEX_FOUND
#   "Flex"
#   "Allows the Solid predicate parser to be updated"
#   "http://flex.sourceforge.net"
#   FALSE
#   ""
#   "Required by the UpdateSolidPredicateParser target (mainly useful for developers)")

#find_program(BISON_EXECUTABLE bison)
#macro_log_feature(BISON_EXECUTABLE
#   "Bison"
#   "Allows the Solid predicate parser to be updated"
#   "http://www.gnu.org/software/bison"
#   FALSE
#   ""
#   "Required by the UpdateSolidPredicateParser target (mainly useful for developers)")
#mark_as_advanced(BISON_EXECUTABLE)      # don't show it in the simple view in cmake-gui/ccmake

# if (FLEX_EXECUTABLE AND BISON_EXECUTABLE)
#
#    add_custom_target(UpdateSolidPredicateParser
#                      COMMAND ${FLEX_EXECUTABLE} -P Solid -o${lexer_FILE}.c ${lexer_FILE}.l
#                      COMMAND ${BISON_EXECUTABLE} -p Solid -d -b ${parser_FILE} ${parser_FILE}.y
#                      COMMAND ${CMAKE_COMMAND} -E copy ${parser_FILE}.tab.c ${CMAKE_CURRENT_SOURCE_DIR}/${parser_FILE}.c
#                      COMMAND ${CMAKE_COMMAND} -E copy ${parser_FILE}.tab.h ${CMAKE_CURRENT_SOURCE_DIR}/${parser_FILE}.h
#                      COMMAND ${CMAKE_COMMAND} -E remove ${parser_FILE}.tab.c ${parser_FILE}.tab.h
#                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#
# else (FLEX_EXECUTABLE AND BISON_EXECUTABLE)
#    add_custom_target(UpdateSolidPredicateParser
#                      COMMAND echo "flex and/or bison not found, so target UpdateSolidPredicateParser inactive")
# endif (FLEX_EXECUTABLE AND BISON_EXECUTABLE)

