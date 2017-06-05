# - Try to find cdio_paranoia
# Once done this will define
#
# CDIOPARANOIA_FOUND - system has libcdio_paranoia
# CDIOPARANOIA_INCLUDE_DIRS - the libcdio_paranoia include directory
# CDIOPARANOIA_LIBRARIES - The libcdio_paranoia libraries

include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules (CDIOPARANOIA libcdio_paranoia)
  list(APPEND CDIOPARANOIA_INCLUDE_DIRS ${CDIOPARANOIA_INCLUDEDIR})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CDIOPARANOIA DEFAULT_MSG CDIOPARANOIA_INCLUDE_DIRS CDIOPARANOIA_LIBRARIES)

mark_as_advanced(CDIOPARANOIA_INCLUDE_DIRS CDIOPARANOIA_LIBRARIES)
