# avinspector: a live debugger for an accel-virt stack, on the host.
#
#   cmake -S tools/inspector -B build-inspector -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-inspector -j28
#   ./build-inspector/avinspector
#
# It is an accel-lib application (accel-lib's UI on its Vulkan renderer), so it is built apart from
# accel-virt's own tree and follows accel-lib's conventions rather than the freestanding rules of
# src/. ACCEL_LIB_DIR names the accel-lib checkout it builds against.
cmake_minimum_required(VERSION 3.24)
project(avinspector CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(ACCEL_LIB_DIR "$ENV{HOME}/Coding/accel-lib" CACHE PATH "The accel-lib checkout the inspector is built on")
if(NOT EXISTS "${ACCEL_LIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "ACCEL_LIB_DIR (${ACCEL_LIB_DIR}) is not an accel-lib checkout")
endif()
# What the inspector needs of accel-lib: its UI on one renderer, nothing it would only test.
set(ACCEL_RENDER_BACKEND vulkan CACHE STRING "" FORCE)
set(ACCEL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ACCEL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ACCEL_PROFILER OFF CACHE BOOL "" FORCE)
add_subdirectory("${ACCEL_LIB_DIR}" accel-lib EXCLUDE_FROM_ALL)

# The accel-virt tree the scope names are read from: every AV_SCOPE("...") in it, hashed as the
# trace tables hash them.
get_filename_component(AV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src" ABSOLUTE)

file(GLOB_RECURSE INSPECTOR_DATA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/data/*.cpp")
add_library(avinspector-data STATIC ${INSPECTOR_DATA_SOURCES})
target_include_directories(avinspector-data PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_link_libraries(avinspector-data PUBLIC accel::lib)
target_compile_definitions(avinspector-data PRIVATE AV_SOURCE_DIR="${AV_SOURCE_DIR}")
target_compile_options(avinspector-data PRIVATE -fno-exceptions -Wall -Wextra)

file(GLOB_RECURSE INSPECTOR_UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/ui/*.cpp")
add_executable(avinspector "${CMAKE_CURRENT_SOURCE_DIR}/source/Main.cpp" ${INSPECTOR_UI_SOURCES})
target_link_libraries(avinspector PRIVATE avinspector-data)
target_compile_options(avinspector PRIVATE -fno-exceptions -Wall -Wextra)

# The parsers and the history, over recorded bytes: no window, no accel-virt process.
file(GLOB INSPECTOR_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(avinspector-tests "${ACCEL_LIB_DIR}/tests/TestMain.cpp" ${INSPECTOR_TEST_SOURCES})
target_include_directories(avinspector-tests PRIVATE "${ACCEL_LIB_DIR}/tests")
target_link_libraries(avinspector-tests PRIVATE avinspector-data)
target_compile_options(avinspector-tests PRIVATE -fno-exceptions)
enable_testing()
add_test(NAME avinspector-tests COMMAND avinspector-tests)
