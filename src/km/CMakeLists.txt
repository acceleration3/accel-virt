# rvkgpu.sys. Configured by the top-level build as an external project with
# cmake/windows_kernel_toolchain.cmake, because one CMake project cannot hold
# two C++ compilers. Every flag comes from the toolchain.
cmake_minimum_required(VERSION 3.24)
project(rvkgpu CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE RVKGPU_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# The kernel has no C++ standard headers, and the generated wire includes
# <cstdint>. Nothing generated belongs in the miniport anyway: a graphics API's
# commands are decoded on the host, never here (feature 063). proto/vulkan/ is
# the same rule by hand: what it holds reads an application's Vulkan
# structures, which no miniport ever sees, and it includes Vulkan's own headers
# (feature 047).
file(GLOB_RECURSE RVKGPU_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/vulkan/*.cpp)
list(REMOVE_ITEM RVKGPU_SOURCES ${RVKGPU_GENERATED})

add_executable(rvkgpu ${RVKGPU_SOURCES})
set_target_properties(rvkgpu PROPERTIES SUFFIX .sys)
target_include_directories(rvkgpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
