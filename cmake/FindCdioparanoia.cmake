# - Try to find cdio_paranoia
# Once done this will define
#
# CDIOPARANOIA_FOUND - system has libcdio_paranoia
# CDIOPARANOIA_INCLUDE_DIRS - the libcdio_paranoia include directory
# CDIOPARANOIA_LIBRARIES - The libcdio_paranoia libraries

include(FindPkgConfig)
include(CheckIncludeFiles)

if(PKG_CONFIG_FOUND)
  pkg_check_modules (CDIOPARANOIA libcdio_paranoia)
  list(APPEND CDIOPARANOIA_INCLUDE_DIRS ${CDIOPARANOIA_INCLUDEDIR})
endif()

if (CDIOPARANOIA_FOUND)
    check_include_files(cdio/paranoia.h HAVE_CDIO_PARANOIA_H)
    check_include_files(cdio/cdda.h HAVE_CDIO_CDDA_H)
    # Issue 1022 - sometimes its cdio/paranoia/paranoia.h
    if (NOT HAVE_CDIO_PARANOIA_H)
        check_include_files(cdio/paranoia/paranoia.h HAVE_CDIO_PARANOIA_PARANOIA_H)
    endif()
    if (NOT HAVE_CDIO_CDDA_H)
        check_include_files(cdio/paranoia/cdda.h HAVE_CDIO_PARANOIA_CDDA_H)
    endif()
    if (NOT HAVE_CDIO_PARANOIA_H AND NOT HAVE_CDIO_PARANOIA_PARANOIA_H AND NOT HAVE_CDIO_CDDA_H AND NOT HAVE_CDIO_PARANOIA_CDDA_H)
        set(CDIOPARANOIA_FOUND OFF)
        message("Failed to determine cdio/paranoia header file location")
    endif()
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CDIOPARANOIA DEFAULT_MSG CDIOPARANOIA_INCLUDE_DIRS CDIOPARANOIA_LIBRARIES)

mark_as_advanced(CDIOPARANOIA_INCLUDE_DIRS CDIOPARANOIA_LIBRARIES)
