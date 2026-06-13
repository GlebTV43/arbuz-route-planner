cmake_minimum_required(VERSION 3.12)
project(labwork7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL ON CACHE BOOL "Use Windows native SSL" FORCE)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/data" DATA_DIR_FOR_CPP)
add_compile_definitions(PROJECT_DATA_PATH="${DATA_DIR_FOR_CPP}")

include(FetchContent)

FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.x
)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(lib)
add_subdirectory(bin)
add_subdirectory(tests)

include(tests/coverage.cmake)
