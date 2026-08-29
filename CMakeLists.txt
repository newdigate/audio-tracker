cmake_minimum_required(VERSION 3.16)
project(audio_tracker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(audio_tracker STATIC
    src/io/stream.cpp
    src/io/memory_stream.cpp
    src/io/file_stream.cpp
    src/model.cpp
    src/xm/xm_delta.cpp
)

target_include_directories(audio_tracker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(audio_tracker PUBLIC cxx_std_17)

enable_testing()
add_subdirectory(tests)
