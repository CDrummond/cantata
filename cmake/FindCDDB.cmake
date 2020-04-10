# - Try to find CDDB
# Once done this will define
#
#  CDDB_FOUND - system has CDDB
#  CDDB_INCLUDE_DIR - the libcddb include directory
#  CDDB_LIBS - The libcddb libraries

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(_CDDB libcddb)
endif(PKG_CONFIG_FOUND)

find_path(CDDB_INCLUDE_DIR cddb/cddb.h
    ${_CDDB_INCLUDE_DIRS}
)

find_library(CDDB_LIBS NAMES cddb
    PATHS
    ${_CDDB_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CDDB DEFAULT_MSG CDDB_INCLUDE_DIR CDDB_LIBS)

mark_as_advanced(CDDB_INCLUDE_DIR CDDB_LIBS)
