cmake_minimum_required(VERSION 3.10)
# set -DCMAKE_POLICY_VERSION_MINIMUM=3.5
# set policy version to 3.5 to allow for use of cmake_policy in the debian.cmake file, which is used to set policies that are required for compatibility with older cmake versions used in debian stable

project("GuitarMidi-Core")

option(WITH_TRACING_INFO "Build with tracing logs" OFF)
option(WITH_AUDIO_OUTPUT "Build with audio output support" OFF)
set(RING_BUFFER_SIZE "8" CACHE STRING "Number of frames to store in the ring buffer") 
set(NUM_INFERENCE_THREADS "4" CACHE STRING "Number of threads to use for inference")

message("Ring buffer size: ${RING_BUFFER_SIZE}")
message("Number of inference threads: ${NUM_INFERENCE_THREADS}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
configure_file(include/guitarmidi/config.hpp.in include/guitarmidi/config.hpp)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)


include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Build only the BSD-licensed SpeexDSP resampler rather than the full DSP suite.
# Pin the upstream release for reproducible ARM64, Windows, and Linux builds.
include(FetchContent)
FetchContent_Declare(guitarmidi_speexdsp
    GIT_REPOSITORY https://github.com/xiph/speexdsp.git
    GIT_TAG SpeexDSP-1.2.1
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(guitarmidi_speexdsp)
if(NOT guitarmidi_speexdsp_POPULATED)
    FetchContent_Populate(guitarmidi_speexdsp)
endif()

add_library(guitarmidi_speex_resampler STATIC
    "${guitarmidi_speexdsp_SOURCE_DIR}/libspeexdsp/resample.c")
set_target_properties(guitarmidi_speex_resampler PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(guitarmidi_speex_resampler PUBLIC
    "${guitarmidi_speexdsp_SOURCE_DIR}/include"
    "${guitarmidi_speexdsp_SOURCE_DIR}/include/speex"
    "${guitarmidi_speexdsp_SOURCE_DIR}/libspeexdsp")
target_compile_definitions(guitarmidi_speex_resampler PUBLIC
    OUTSIDE_SPEEX RANDOM_PREFIX=guitarmidi FLOATING_POINT)

# Bundle the upstream BSD notice with every distributable LV2 plugin.
install(FILES "${guitarmidi_speexdsp_SOURCE_DIR}/COPYING"
    DESTINATION guitarmidi-pistomp.lv2
    RENAME SPEEXDSP-LICENSE.txt)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(resampling_regression tests/resampling_regression.cpp)
    target_include_directories(resampling_regression PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(resampling_regression PRIVATE guitarmidi_speex_resampler)
    add_test(NAME resampling_regression COMMAND resampling_regression)
endif()

#find_package(OpenMP REQUIRED)
# pkg_check_modules(Pango REQUIRED pango)

# add_subdirectory(tflite_custom_op_example)

# Read TF_VERSION file to get TensorFlow version
file(READ "${TENSORFLOW_SOURCE_DIR}/tensorflow/tf_version.bzl" TF_VERSION_BZL)
#message ("TF_VERSION_BZL contents: ${TF_VERSION_BZL}")
string(REGEX MATCH "TF_VERSION\\s* = \\s*\"([0-9]+)\\.([0-9]+)\\.([0-9]+)\"" _ "${TF_VERSION_BZL}")

if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "Could not determine Tensorflow version from tf_version.bzl contents: ${TF_VERSION_BZL}")
endif()

set(TF_MAJOR ${CMAKE_MATCH_1})
set(TF_MINOR ${CMAKE_MATCH_2})
set(TF_PATCH ${CMAKE_MATCH_3})
set(TF_VERSION_SUFFIX "")

message("Building with Tensorflow version: ${TF_MAJOR}.${TF_MINOR}.${TF_PATCH}${TF_VERSION_SUFFIX}")


# Provide compile-time definitions in the recommended CMake form (no leading -D).
# This makes macros available globally; for target-specific definitions prefer
# `target_compile_definitions(<target> PRIVATE ...)` below.
## compile-time definitions will be added to the `guitarmidi` target below


option(LINK_TFLITE_FLEX "Enable tensorflowlite_flex library linkage" OFF)

set(TENSORFLOW_SOURCE_DIR "${CMAKE_SOURCE_DIR}/ext/tensorflow" CACHE PATH
  "Directory that contains the TensorFlow project"
)

message("TensorFlow source dir: ${TENSORFLOW_SOURCE_DIR}")
set(TFLITE_ENABLE_XNNPACK ON CACHE BOOL "Enable XNNPACK" FORCE)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "" FORCE)

include_directories(${TENSORFLOW_SOURCE_DIR}  ${TENSORFLOW_SOURCE_DIR}/tensorflow/lite/delegates/xnnpack)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_subdirectory(
  "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
  "${CMAKE_CURRENT_BINARY_DIR}/tensorflow-lite"
  EXCLUDE_FROM_ALL
)

set(SRC
    src/fretboard.cpp
    src/fretboardrepresentation.cpp
    src/guitarnote.cpp
    src/filterbank.cpp
    src/guitarstring.cpp
    src/noteinferencer.cpp
    src/modelinferencer.cpp
)

# The existing logging.cpp uses POSIX clock_gettime().
# It is not needed for normal operation when tracing is disabled.
if(NOT WIN32)
    list(APPEND SRC src/logging.cpp)
endif()

add_library(guitarmidi_core STATIC ${SRC})
if(GUITARMIDI_SYSEX_DIAGNOSTICS)
    target_compile_definitions(
        guitarmidi_core PRIVATE
        GUITARMIDI_SYSEX_DIAGNOSTICS
    )
endif()

if(GUITARMIDI_INFERENCE_DIAGNOSTICS)
    target_compile_definitions(
        guitarmidi_core PRIVATE
        GUITARMIDI_INFERENCE_DIAGNOSTICS
    )
endif()

set_target_properties(guitarmidi_core PROPERTIES POSITION_INDEPENDENT_CODE TRUE)



target_link_libraries(guitarmidi_core PUBLIC
    tensorflow-lite
    guitarmidi_speex_resampler
)

