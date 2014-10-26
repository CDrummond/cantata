# - Try to find QJSON
# Once done this will define
#
#  QJSON_FOUND - system has QJson
#  QJSON_INCLUDE_DIR - the libqjson include directory
#  QJSON_LIBRARIES - The libqjson libraries

find_path(QJSON_INCLUDE_DIR qjson/parser.h)
find_library(QJSON_LIBRARIES qjson)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QJSON DEFAULT_MSG QJSON_INCLUDE_DIR QJSON_LIBRARIES)

mark_as_advanced(QJSON_INCLUDE_DIR QJSON_LIBRARIES)
