# - Try to find CDDB
# Once done this will define
#
#  CDDB_FOUND - system has UDev
#  CDDB_INCLUDE_DIR - the libudev include directory
#  CDDB_LIBS - The libudev libraries

find_path(CDDB_INCLUDE_DIR cddb/cddb.h)
find_library(CDDB_LIBS cddb)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CDDB DEFAULT_MSG CDDB_INCLUDE_DIR CDDB_LIBS)

mark_as_advanced(CDDB_INCLUDE_DIR CDDB_LIBS)
