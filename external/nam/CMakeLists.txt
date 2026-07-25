cmake_minimum_required(VERSION 3.22)
project(nam_core CXX)

# Copied into the NeuralAmpModelerCore source tree by FetchContent (see root CMakeLists).
# Built as a static lib; the app must link it with /WHOLEARCHIVE (MSVC) or
# --whole-archive so ConfigParserHelper static registrars are not stripped.

if(NOT DEFINED EIGEN_INCLUDE_DIR OR EIGEN_INCLUDE_DIR STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/eigen/Eigen/Core")
        set(EIGEN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/eigen")
    else()
        message(FATAL_ERROR "nam_core: EIGEN_INCLUDE_DIR is not set and Dependencies/eigen is missing")
    endif()
endif()

set(NAM_SOURCES
    NAM/activations.cpp
    NAM/container.cpp
    NAM/conv1d.cpp
    NAM/convnet.cpp
    NAM/dsp.cpp
    NAM/get_dsp.cpp
    NAM/lstm.cpp
    NAM/ring_buffer.cpp
    NAM/util.cpp
    NAM/wavenet/a2_fast.cpp
    NAM/wavenet/model.cpp
    NAM/wavenet/slimmable.cpp
)

# SkeletonHive helper — registers parsers into THIS lib's ConfigParserRegistry.
set(NAM_ENSURE_PARSERS_SRC "")
foreach(_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/nam_ensure_parsers.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../nam/nam_ensure_parsers.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nam_ensure_parsers.cpp")
    if(EXISTS "${_candidate}")
        set(NAM_ENSURE_PARSERS_SRC "${_candidate}")
        break()
    endif()
endforeach()

# When this CMakeLists is copied into the FetchContent tree, also try the
# repo's external/nam copy via an explicit cache variable from the parent.
if(NAM_ENSURE_PARSERS_SRC STREQUAL "" AND DEFINED SKELETONHIVE_NAM_ENSURE_PARSERS_SRC)
    if(EXISTS "${SKELETONHIVE_NAM_ENSURE_PARSERS_SRC}")
        set(NAM_ENSURE_PARSERS_SRC "${SKELETONHIVE_NAM_ENSURE_PARSERS_SRC}")
    endif()
endif()

if(NAM_ENSURE_PARSERS_SRC STREQUAL "")
    message(FATAL_ERROR "nam_core: nam_ensure_parsers.cpp not found")
endif()

add_library(nam_core STATIC ${NAM_SOURCES} ${NAM_ENSURE_PARSERS_SRC})

target_include_directories(nam_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/NAM>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/nlohmann>
        $<BUILD_INTERFACE:${EIGEN_INCLUDE_DIR}>
)

target_compile_features(nam_core PUBLIC cxx_std_20)

# NAM_SAMPLE_FLOAT matches JUCE float buffers.
# EIGEN_MAX_ALIGN_BYTES=0 avoids MSVC misaligned Eigen member crashes (NAM Core #67).
target_compile_definitions(nam_core
    PUBLIC
        NAM_SAMPLE_FLOAT
        NAM_ENABLE_A2_FAST
        EIGEN_MAX_ALIGN_BYTES=0
)

if(WIN32)
    target_compile_definitions(nam_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(nam_core PRIVATE /W0 /wd4244 /wd4267 /wd4996)
else()
    target_compile_options(nam_core PRIVATE -w)
endif()
