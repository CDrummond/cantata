# - Try to find LAME
# Once done this will define
#
#  LAME_FOUND - system has UDev
#  LAME_INCLUDE_DIR - the libudev include directory
#  LAME_LIBS - The libudev libraries

find_path(LAME_INCLUDE_DIR lame/lame.h)
find_library(LAME_LIBS mp3lame)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LAME DEFAULT_MSG LAME_INCLUDE_DIR LAME_LIBS)

mark_as_advanced(LAME_INCLUDE_DIR LAME_LIBS)
