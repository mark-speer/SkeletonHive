cmake_minimum_required(VERSION 3.22)
project(aubio C)

option(WITH_DEPENDENCIES "Adds extra dependencies (sndfile/ffmpeg)" OFF)
option(BUILD_TOOLS "Build and install tools" OFF)

add_definitions(
    -DHAVE_STDLIB_H=1
    -DHAVE_STDIO_H=1
    -DHAVE_MATH_H=1
    -DHAVE_STRING_H=1
    -DHAVE_LIMITS_H=1
    -DHAVE_STDARG_H=1
    -DHAVE_ERRNO_H=1
    -DHAVE_C99_VARARGS_MACROS=1
    -D_CRT_SECURE_NO_WARNINGS=1
)

if(WITH_DEPENDENCIES)
    add_definitions(
        -DHAVE_SNDFILE=1
        -DHAVE_WAVWRITE=1
        -DHAVE_WAVREAD=1
        -DHAVE_LIBAV=1
        -DHAVE_SWRESAMPLE=1
    )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE SOURCES src/*.c)

if(WIN32 AND NOT MINGW)
    set_source_files_properties(src/io/sink_wavwrite.c PROPERTIES COMPILE_FLAGS /FIWinsock2.h)
endif()

add_library(aubio STATIC ${SOURCES})

target_include_directories(aubio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/aubio>
)

if(WIN32)
    target_compile_definitions(aubio PRIVATE HAVE_WIN_HACKS=1)
    target_link_libraries(aubio PUBLIC ws2_32)
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h" "")
target_include_directories(aubio PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

if(MSVC)
    target_compile_options(aubio PRIVATE /W3)
else()
    target_compile_options(aubio PRIVATE -w)
endif()
