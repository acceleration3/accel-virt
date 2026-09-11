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

add_executable(rvkgpu ${RVKGPU_SOURCES})
set_target_properties(rvkgpu PROPERTIES SUFFIX .sys)
target_include_directories(rvkgpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
