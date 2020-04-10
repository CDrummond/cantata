# - Try to find UDev
# Once done this will define
#
#  UDEV_FOUND - system has UDev
#  UDEV_INCLUDE_DIR - the libudev include directory
#  UDEV_LIBS - The libudev libraries

# Copyright (c) 2010, Rafael Fernández López, <ereslibre@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
include(FindPkgConfig)
pkg_check_modules(_UDEV libudev)

find_path(UDEV_INCLUDE_DIR libudev.h
    ${_UDEV_INCLUDE_DIRS}
)

find_library(UDEV_LIBS NAMES udev
    PATHS
    ${_UDEV_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UDev DEFAULT_MSG UDEV_INCLUDE_DIR UDEV_LIBS)

mark_as_advanced(UDEV_INCLUDE_DIR UDEV_LIBS)
