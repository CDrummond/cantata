# add_custom_command (OUTPUT ${qjson_SOURCE_DIR}/lib/json_parser.cc
#                    PRE_BUILD
#                    COMMAND bison -t -o json_parser.cc -d json_parser.yy
#                    DEPENDS json_parser.yy
#                    WORKING_DIRECTORY ${qjson_SOURCE_DIR}/lib/
#                    )

# To regenerate json_scanner.cc use:
# flex json_scanner.yy

IF (ENABLE_QT5)
  qt5_wrap_cpp(qjson_MOC_SRCS ${qjson_MOC_HDRS})
ELSE (ENABLE_QT5)
  if (ENABLE_KDE)
    add_definitions(${KDE4_ENABLE_EXCEPTIONS})
  else (ENABLE_KDE)
    if (CMAKE_COMPILER_IS_GNUCXX)
      add_definitions("-fexceptions -UQT_NO_EXCEPTIONS")
    endif (CMAKE_COMPILER_IS_GNUCXX)
    if (CMAKE_C_COMPILER MATCHES "icc")
      add_definitions(-fexceptions)
    endif (CMAKE_C_COMPILER MATCHES "icc")
    if (MSVC)
      add_definitions(-EHsc)
    endif (MSVC)
  endif (ENABLE_KDE)
  qt4_wrap_cpp(qjson_MOC_SRCS ${qjson_MOC_HDRS})
ENDIF(ENABLE_QT5)

set (qjson_SRCS parser.cpp qobjecthelper.cpp json_scanner.cpp json_parser.cc)
set (qjson_HEADERS parser.h qobjecthelper.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

add_library (qjson STATIC ${qjson_SRCS} ${qjson_MOC_SRCS} ${qjson_HEADERS})
