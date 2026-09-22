cmake_minimum_required(VERSION 3.20)

project(PyMoss VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(MOSS_FRAMEWORK_DIR ""
    CACHE PATH "Path to the Moss framework source checkout")

if(NOT MOSS_FRAMEWORK_DIR)
    set(MOSS_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Moss")
endif()

if(NOT EXISTS "${MOSS_FRAMEWORK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Moss framework not found: ${MOSS_FRAMEWORK_DIR}")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MOSS_BUILD_SMOKE_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "${MOSS_FRAMEWORK_DIR}"
    "${CMAKE_BINARY_DIR}/moss-framework"
    EXCLUDE_FROM_ALL
)

find_package(pybind11 CONFIG QUIET)

if(NOT pybind11_FOUND)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(_pymoss MODULE src/main.cpp)

target_compile_features(_pymoss PRIVATE cxx_std_17)

target_link_libraries(_pymoss PRIVATE Moss::Moss)

install(TARGETS _pymoss DESTINATION pymoss)