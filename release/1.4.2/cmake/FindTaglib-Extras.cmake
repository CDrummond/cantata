# - Try to find the Taglib-Extras library
# Once done this will define
#
#  TAGLIB-EXTRAS_FOUND - system has the taglib library
#  TAGLIB-EXTRAS_CFLAGS - the taglib cflags
#  TAGLIB-EXTRAS_LIBRARIES - The libraries needed to use taglib

# Copyright (c) 2006, Laurent Montel, <montel@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(NOT TAGLIB-EXTRAS_MIN_VERSION)
  set(TAGLIB-EXTRAS_MIN_VERSION "1.0")
endif(NOT TAGLIB-EXTRAS_MIN_VERSION)

if(NOT WIN32)
    find_program(TAGLIB-EXTRASCONFIG_EXECUTABLE NAMES taglib-extras-config PATHS
       ${BIN_INSTALL_DIR}
    )
endif(NOT WIN32)

#reset vars
set(TAGLIB-EXTRAS_LIBRARIES)
set(TAGLIB-EXTRAS_CFLAGS)

# if taglib-extras-config has been found
if(TAGLIB-EXTRASCONFIG_EXECUTABLE)

  exec_program(${TAGLIB-EXTRASCONFIG_EXECUTABLE} ARGS --version RETURN_VALUE _return_VALUE OUTPUT_VARIABLE TAGLIB-EXTRAS_VERSION)

  if(TAGLIB-EXTRAS_VERSION STRLESS "${TAGLIB-EXTRAS_MIN_VERSION}")
     message(STATUS "TagLib-Extras version too old: version searched :${TAGLIB-EXTRAS_MIN_VERSION}, found ${TAGLIB-EXTRAS_VERSION}")
     set(TAGLIB-EXTRAS_FOUND FALSE)
  else(TAGLIB-EXTRAS_VERSION STRLESS "${TAGLIB-EXTRAS_MIN_VERSION}")

     exec_program(${TAGLIB-EXTRASCONFIG_EXECUTABLE} ARGS --libs RETURN_VALUE _return_VALUE OUTPUT_VARIABLE TAGLIB-EXTRAS_LIBRARIES)

     exec_program(${TAGLIB-EXTRASCONFIG_EXECUTABLE} ARGS --cflags RETURN_VALUE _return_VALUE OUTPUT_VARIABLE TAGLIB-EXTRAS_CFLAGS)

     if(TAGLIB-EXTRAS_LIBRARIES AND TAGLIB-EXTRAS_CFLAGS)
        set(TAGLIB-EXTRAS_FOUND TRUE)
     endif(TAGLIB-EXTRAS_LIBRARIES AND TAGLIB-EXTRAS_CFLAGS)
     string(REGEX REPLACE " *-I" ";" TAGLIB-EXTRAS_INCLUDES "${TAGLIB-EXTRAS_CFLAGS}")
  endif(TAGLIB-EXTRAS_VERSION STRLESS "${TAGLIB-EXTRAS_MIN_VERSION}") 
  mark_as_advanced(TAGLIB-EXTRAS_CFLAGS TAGLIB-EXTRAS_LIBRARIES TAGLIB-EXTRAS_INCLUDES)

else(TAGLIB-EXTRASCONFIG_EXECUTABLE)

  find_path(TAGLIB-EXTRAS_INCLUDES
    NAMES
    tfile_helper.h
    PATH_SUFFIXES taglib-extras
    PATHS
    ${KDE4_INCLUDE_DIR}
    ${INCLUDE_INSTALL_DIR}
  )

    IF(NOT WIN32)
      # on non-win32 we don't need to take care about WIN32_DEBUG_POSTFIX

      FIND_LIBRARY(TAGLIB-EXTRAS_LIBRARIES tag-extras PATHS ${KDE4_LIB_DIR} ${LIB_INSTALL_DIR})

    ELSE(NOT WIN32)

      # 1. get all possible libnames
      SET(args PATHS ${KDE4_LIB_DIR} ${LIB_INSTALL_DIR})             
      SET(newargs "")               
      SET(libnames_release "")      
      SET(libnames_debug "")        

      LIST(LENGTH args listCount)

        # just one name
        LIST(APPEND libnames_release "tag-extras")
        LIST(APPEND libnames_debug   "tag-extrasd")

        SET(newargs ${args})

      # search the release lib
      FIND_LIBRARY(TAGLIB-EXTRAS_LIBRARIES_RELEASE
                   NAMES ${libnames_release}
                   ${newargs}
      )

      # search the debug lib
      FIND_LIBRARY(TAGLIB-EXTRAS_LIBRARIES_DEBUG
                   NAMES ${libnames_debug}
                   ${newargs}
      )

      IF(TAGLIB-EXTRAS_LIBRARIES_RELEASE AND TAGLIB-EXTRAS_LIBRARIES_DEBUG)

        # both libs found
        SET(TAGLIB-EXTRAS_LIBRARIES optimized ${TAGLIB-EXTRAS_LIBRARIES_RELEASE}
                        debug     ${TAGLIB-EXTRAS_LIBRARIES_DEBUG})

      ELSE(TAGLIB-EXTRAS_LIBRARIES_RELEASE AND TAGLIB-EXTRAS_LIBRARIES_DEBUG)

        IF(TAGLIB-EXTRAS_LIBRARIES_RELEASE)

          # only release found
          SET(TAGLIB-EXTRAS_LIBRARIES ${TAGLIB-EXTRAS_LIBRARIES_RELEASE})

        ELSE(TAGLIB-EXTRAS_LIBRARIES_RELEASE)

          # only debug (or nothing) found
          SET(TAGLIB-EXTRAS_LIBRARIES ${TAGLIB-EXTRAS_LIBRARIES_DEBUG})

        ENDIF(TAGLIB-EXTRAS_LIBRARIES_RELEASE)

      ENDIF(TAGLIB-EXTRAS_LIBRARIES_RELEASE AND TAGLIB-EXTRAS_LIBRARIES_DEBUG)

      MARK_AS_ADVANCED(TAGLIB-EXTRAS_LIBRARIES_RELEASE)
      MARK_AS_ADVANCED(TAGLIB-EXTRAS_LIBRARIES_DEBUG)

    ENDIF(NOT WIN32)
  
  INCLUDE(FindPackageMessage)
  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Taglib-Extras DEFAULT_MSG TAGLIB-EXTRAS_INCLUDES TAGLIB-EXTRAS_LIBRARIES)

endif(TAGLIB-EXTRASCONFIG_EXECUTABLE)


if(TAGLIB-EXTRAS_FOUND)
  if(NOT Taglib-Extras_FIND_QUIETLY AND TAGLIB-EXTRASCONFIG_EXECUTABLE)
    message(STATUS "Taglib-Extras found: ${TAGLIB-EXTRAS_LIBRARIES}")
  endif(NOT Taglib-Extras_FIND_QUIETLY AND TAGLIB-EXTRASCONFIG_EXECUTABLE)
else(TAGLIB-EXTRAS_FOUND)
  if(Taglib-Extras_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find Taglib-Extras")
  endif(Taglib-Extras_FIND_REQUIRED)
endif(TAGLIB-EXTRAS_FOUND)

