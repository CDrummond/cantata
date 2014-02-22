include(MacroLogFeature)
find_package(SPEEXDSP)
macro_log_feature(SPEEXDSP_FOUND "SpeexDSP" "Used to speedup/improve ReplayGain calculation." "http://www.speex.org")
set(USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})

macro(ebur128_check_for_sse)
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
endmacro(ebur128_check_for_sse)

ebur128_check_for_sse()

set (ebur128_SRCS ebur128.c)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_library (ebur128 STATIC ${ebur128_SRCS})
if (SPEEXDSP_FOUND)
    include_directories(${SPEEXDSP_INCLUDE_DIRS})
    target_link_libraries(ebur128 ${SPEEXDSP_LIBRARIES})
endif (SPEEXDSP_FOUND)

