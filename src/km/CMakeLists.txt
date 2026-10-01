# avgpu.sys. Configured by the top-level build as an external project with
# cmake/windows_kernel_toolchain.cmake, because one CMake project cannot hold
# two C++ compilers. Every flag comes from the toolchain.
cmake_minimum_required(VERSION 3.24)
project(avgpu CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE AVGPU_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# The kernel has no C++ standard headers, and the generated wire includes
# <cstdint>. Nothing generated belongs in the miniport anyway: a graphics API's
# commands are decoded on the host, never here (feature 063). proto/vulkan/ is
# the same rule by hand: what it holds reads an application's Vulkan
# structures, which no miniport ever sees, and it includes Vulkan's own headers
# (feature 047).
file(GLOB_RECURSE AVGPU_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/vulkan/*.cpp)
list(REMOVE_ITEM AVGPU_SOURCES ${AVGPU_GENERATED})

add_executable(avgpu ${AVGPU_SOURCES})
set_target_properties(avgpu PROPERTIES SUFFIX .sys)
target_include_directories(avgpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
