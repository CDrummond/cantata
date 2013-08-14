# - Try to find the libmtp library
# Once done this will define
#
#  MTP_FOUND - system has libmtp
#  MTP_INCLUDE_DIR - the libmtp include directory
#  MTP_LIBRARIES - Link these to use libmtp
#  MTP_DEFINITIONS - Compiler switches required for using libmtp
#

if (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND MTP_VERSION_OKAY)

  # in cache already
  SET(MTP_FOUND TRUE)

else (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND MTP_VERSION_OKAY)
  if(NOT WIN32)
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    INCLUDE(FindPkgConfig)
  
    pkg_check_modules(_MTP libmtp)
  
    set(MTP_DEFINITIONS ${_MTP_CFLAGS})
  endif(NOT WIN32)
  FIND_PATH(MTP_INCLUDE_DIR libmtp.h
    ${_MTP_INCLUDE_DIRS}
  )
  
  FIND_LIBRARY(MTP_LIBRARIES NAMES mtp
    PATHS
    ${_MTP_LIBRARY_DIRS}
  )

  exec_program(${PKG_CONFIG_EXECUTABLE} ARGS --atleast-version=1.0.0 libmtp OUTPUT_VARIABLE _pkgconfigDevNull RETURN_VALUE MTP_VERSION_OKAY)
  
  if (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND MTP_VERSION_OKAY STREQUAL "0")
     set(MTP_FOUND TRUE)
  endif (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND MTP_VERSION_OKAY STREQUAL "0")
  
  if (MTP_FOUND)
    if (NOT Mtp_FIND_QUIETLY)
      message(STATUS "Found MTP: ${MTP_LIBRARIES}")
    endif (NOT Mtp_FIND_QUIETLY)
  else (MTP_FOUND)
    if (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND NOT MTP_VERSION_OKAY STREQUAL "0")
      message(STATUS "Found MTP but version requirements not met")
    endif (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND NOT MTP_VERSION_OKAY STREQUAL "0")
    if (Mtp_FIND_REQUIRED)
      message(FATAL_ERROR "Could NOT find MTP")
    endif (Mtp_FIND_REQUIRED)
  endif (MTP_FOUND)
  
  MARK_AS_ADVANCED(MTP_INCLUDE_DIR MTP_LIBRARIES MTP_VERSION_OKAY)
  
endif (MTP_INCLUDE_DIR AND MTP_LIBRARIES AND MTP_VERSION_OKAY)
