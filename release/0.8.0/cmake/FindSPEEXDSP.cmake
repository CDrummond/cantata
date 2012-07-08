find_package(PkgConfig)
pkg_check_modules(PC_SPEEXDSP QUIET speexdsp)

find_path(SPEEXDSP_INCLUDE_DIR speex/speex_resampler.h
          HINTS ${PC_SPEEXDSP_INCLUDEDIR} ${PC_SPEEXDSP_INCLUDE_DIRS})
find_library(SPEEXDSP_LIBRARY speexdsp
             HINTS ${PC_SPEEXDSP_LIBDIR} ${PC_SPEEXDSP_LIBRARY_DIRS})

set(SPEEXDSP_LIBRARIES ${SPEEXDSP_LIBRARY})
set(SPEEXDSP_INCLUDE_DIRS ${SPEEXDSP_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SPEEXDSP DEFAULT_MSG SPEEXDSP_LIBRARY SPEEXDSP_INCLUDE_DIR)
mark_as_advanced(SPEEXDSP_INCLUDE_DIR SPEEXDSP_LIBRARY)
