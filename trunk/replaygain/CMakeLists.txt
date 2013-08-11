macro(cantata_check_for_sse)
    # check for SSE extensions
    include(CheckCXXSourceRuns)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_REQUIRED_FLAGS "-msse3")
        check_cxx_source_runs("
            #include <pmmintrin.h>
            int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals); b = _mm_hadd_pd(a,a); _mm_storeu_pd(vals, b); return 0; }"
            HAS_SSE3_EXTENSIONS)
        if (HAS_SSE3_EXTENSIONS)
            add_definitions("-msse3 -mfpmath=sse")
        else (HAS_SSE3_EXTENSIONS)
            # No sse3, check sse2
            set(CMAKE_REQUIRED_FLAGS "-msse2")
            check_cxx_source_runs("
                #include <emmintrin.h>
                int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals); b = _mm_add_pd(a,a);  _mm_storeu_pd(vals,b); return 0; }"
                HAS_SSE2_EXTENSIONS)
            if (HAS_SSE2_EXTENSIONS)
                add_definitions("-msse2 -mfpmath=sse")
            else (HAS_SSE2_EXTENSIONS)
                # No sse2, check sse
                set(CMAKE_REQUIRED_FLAGS "-msse")
                check_cxx_source_runs("
                    #include <xmmintrin.h>
                    int main() { __m128 a, b; float vals[4] = {0}; a = _mm_loadu_ps(vals); b = a; b = _mm_add_ps(a,b); _mm_storeu_ps(vals,b); return 0; }"
                    HAS_SSE_EXTENSIONS)
                if (HAS_SSE_EXTENSIONS)
                    add_definitions("-msse -mfpmath=sse")
                endif (HAS_SSE_EXTENSIONS)
            endif (HAS_SSE2_EXTENSIONS)
        endif (HAS_SSE3_EXTENSIONS)
        set(CMAKE_REQUIRED_FLAGS)
    elseif (MSVC)
        check_cxx_source_runs("
            #include <emmintrin.h>
            int main() { __m128d a, b; double vals[2] = {0}; a = _mm_loadu_pd(vals);  b = _mm_add_pd(a,a);  _mm_storeu_pd(vals,b); return 0; }"
            HAS_SSE2_EXTENSIONS)
    endif (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
endmacro(cantata_check_for_sse)

if (FFMPEG_FOUND OR MPG123_FOUND)
    cantata_check_for_sse()
    include_directories(${QTINCLUDES} ${ZLIB_INCLUDE_DIRS})
    if (FFMPEG_FOUND)
        set(CANTATA_RG_SRCS ${CANTATA_RG_SRCS} ffmpeginput.cpp)
        include_directories(${FFMPEG_INCLUDE_DIRS})
    endif (FFMPEG_FOUND)
    if (MPG123_FOUND)
        set(CANTATA_RG_SRCS ${CANTATA_RG_SRCS} mpg123input.cpp)
        include_directories(${MPG123_INCLUDE_DIRS})
    endif (MPG123_FOUND)
    if (SPEEXDSP_FOUND)
        include_directories(${SPEEXDSP_INCLUDE_DIRS})
    endif (SPEEXDSP_FOUND)

    include_directories(${CMAKE_SOURCE_DIR}/replaygain ${CMAKE_SOURCE_DIR}/support)
    set(CANTATA_RG_SRCS ${CANTATA_RG_SRCS} main.cpp ebur128.c replaygain.cpp trackscanner.cpp jobcontroller.cpp ../support/thread.cpp)
    set(CANTATA_RG_MOC_HDRS ${CANTATA_RG_MOC_HDRS} replaygain.h trackscanner.h jobcontroller.h ../support/thread.h)

    if (ENABLE_KDE_SUPPORT)
        kde4_add_executable(cantata-replaygain ${CANTATA_RG_SRCS} ${CANTATA_RG_MOC_SRCS})
        install(TARGETS cantata-replaygain RUNTIME DESTINATION lib/cantata)
    else (ENABLE_KDE_SUPPORT)
        if (ENABLE_QT5)
            QT5_WRAP_CPP(CANTATA_RG_MOC_SRCS ${CANTATA_RG_MOC_HDRS})
        else (ENABLE_QT5)
            INCLUDE(${QT_USE_FILE})
            QT4_WRAP_CPP(CANTATA_RG_MOC_SRCS ${CANTATA_RG_MOC_HDRS})
        endif (ENABLE_QT5)

        if (WIN32)
            set(CMAKE_BUILD_TYPE "Release")
            ADD_EXECUTABLE(cantata-replaygain WIN32 ${CANTATA_RG_SRCS} ${CANTATA_RG_MOC_SRCS} ${CANTATA_PO})
            install(TARGETS cantata-replaygain DESTINATION ${CMAKE_INSTALL_PREFIX})
        else (WIN32)
            ADD_EXECUTABLE(cantata-replaygain ${CANTATA_RG_SRCS} ${CANTATA_RG_MOC_SRCS})
            install(TARGETS cantata-replaygain RUNTIME DESTINATION lib/cantata)
        endif (WIN32)
    endif (ENABLE_KDE_SUPPORT)

    if (NOT ENABLE_KDE_SUPPORT AND NOT WIN32)
        target_link_libraries(cantata-replaygain X11)
    endif (NOT ENABLE_KDE_SUPPORT AND NOT WIN32)
    if (FFMPEG_FOUND)
        target_link_libraries(cantata-replaygain ${FFMPEG_LIBRARIES})
    endif (FFMPEG_FOUND)
    if (MPG123_FOUND)
        target_link_libraries(cantata-replaygain ${MPG123_LIBRARIES})
    endif (MPG123_FOUND)
    if (SPEEXDSP_FOUND)
        target_link_libraries(cantata-replaygain ${SPEEXDSP_LIBRARIES})
    endif (SPEEXDSP_FOUND)
    if (TAGLIB-EXTRAS_FOUND)
        target_link_libraries(cantata-replaygain ${TAGLIB-EXTRAS_LIBRARIES})
        include_directories(${TAGLIB-EXTRAS_INCLUDES})
    endif (TAGLIB-EXTRAS_FOUND)
    target_link_libraries(cantata-replaygain ${QTCORELIBS} ${TAGLIB_LIBRARIES})
endif (FFMPEG_FOUND OR MPG123_FOUND)

