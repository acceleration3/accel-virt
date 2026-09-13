# rvktest.exe, the in-guest D3DKMT suite. Configured by the top-level build as
# an external project with cmake/windows_user_toolchain.cmake. The suite is
# src/guest; this directory translates it to Windows.
cmake_minimum_required(VERSION 3.24)
project(rvktest CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE RVKTEST_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# This program has no C runtime, and the generated wire includes <cstdint>: the
# Vulkan wire belongs to the ICD feature 045 builds, not to the D3DKMT suite.
file(GLOB_RECURSE RVKTEST_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp)
list(REMOVE_ITEM RVKTEST_SOURCES ${RVKTEST_GENERATED})

# Import libraries naming exactly what the program calls; the SDK's own are
# not in the kit.
set(RVKTEST_IMPORTS "")
foreach(module gdi32 kernel32 ntdll)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m i386:x86-64 -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND RVKTEST_IMPORTS ${library})
endforeach()
add_custom_target(rvktest_imports DEPENDS ${RVKTEST_IMPORTS})

add_executable(rvktest ${RVKTEST_SOURCES})
add_dependencies(rvktest rvktest_imports)
target_include_directories(rvktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rvktest PRIVATE ${RVKTEST_IMPORTS})
