# - Try to find the CD Paranoia libraries
# Once done this will define
#
#  CDPARANOIA_FOUND       - system has cdparanoia
#  CDPARANOIA_INCLUDE_DIR - the cdparanoia include directory
#  CDPARANOIA_LIBRARIES   - Link these to use cdparanoia
#

# Copyright (c) 2006, Richard Laerkaeng, <richard@goteborg.utfors.se>
# Copyright (c) 2007, Allen Winter, <winter@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(CheckCSourceCompiles)

if (CDPARANOIA_INCLUDE_DIR AND CDPARANOIA_LIBRARIES)
  # in cache already
  SET(CDPARANOIA_FOUND TRUE)

else (CDPARANOIA_INCLUDE_DIR AND CDPARANOIA_LIBRARIES)

  FIND_PATH(CDPARANOIA_INCLUDE_DIR cdda_interface.h PATH_SUFFIXES cdda)

  FIND_LIBRARY(CDPARANOIA_LIBRARY NAMES cdda_paranoia)
  FIND_LIBRARY(CDPARANOIA_IF_LIBRARY NAMES cdda_interface)

  IF (CDPARANOIA_LIBRARY AND CDPARANOIA_IF_LIBRARY)
    SET(CDPARANOIA_LIBRARIES ${CDPARANOIA_LIBRARY} ${CDPARANOIA_IF_LIBRARY} "-lm")
  ENDIF (CDPARANOIA_LIBRARY AND CDPARANOIA_IF_LIBRARY)

  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Cdparanoia DEFAULT_MSG
                                    CDPARANOIA_LIBRARIES CDPARANOIA_INCLUDE_DIR)

  MARK_AS_ADVANCED(CDPARANOIA_INCLUDE_DIR CDPARANOIA_LIBRARIES)

  if (CDPARANOIA_FOUND)
      set(CMAKE_REQUIRED_INCLUDES ${CDPARANOIA_INCLUDE_DIR})
      set(CMAKE_REQUIRED_LIBRARIES ${CDPARANOIA_LIBRARIES})
      check_c_source_compiles("#include <cdda_interface.h>
                               #include <cdda_paranoia.h>
                              int main() { paranoia_cachemodel_size(0, 0); return 0; }"
                              CDPARANOIA_HAS_CACHEMODEL_SIZE)
  endif (CDPARANOIA_FOUND)
endif (CDPARANOIA_INCLUDE_DIR AND CDPARANOIA_LIBRARIES)
