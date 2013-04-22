# add_custom_command (OUTPUT ${qjson_SOURCE_DIR}/lib/json_parser.cc
#                    PRE_BUILD
#                    COMMAND bison -t -o json_parser.cc -d json_parser.yy
#                    DEPENDS json_parser.yy
#                    WORKING_DIRECTORY ${qjson_SOURCE_DIR}/lib/
#                    )

# To regenerate json_scanner.cc use:
# flex json_scanner.yy

IF (NOT ENABLE_QT5)
  qt4_wrap_cpp(qjson_MOC_SRCS ${qjson_MOC_HDRS})
ENDIF(NOT ENABLE_QT5)

set (qjson_SRCS parser.cpp qobjecthelper.cpp json_scanner.cpp json_parser.cc)
set (qjson_HEADERS parser.h qobjecthelper.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES} )

add_library (qjson STATIC ${qjson_SRCS} ${qjson_MOC_SRCS} ${qjson_HEADERS})
