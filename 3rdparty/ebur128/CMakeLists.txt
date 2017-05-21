if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(/arch:SSE2)
    add_definitions(-D __SSE2_MATH__)
endif()

set (ebur128_SRCS ebur128.c)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_library (ebur128 STATIC ${ebur128_SRCS})

