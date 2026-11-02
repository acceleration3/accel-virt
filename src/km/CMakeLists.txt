cmake_minimum_required(VERSION 3.24)
project(avgpu CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE AVGPU_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../miniport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE AVGPU_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/vulkan/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/renderer/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/d3d/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/client/*.cpp)
list(REMOVE_ITEM AVGPU_SOURCES ${AVGPU_GENERATED})

add_executable(avgpu ${AVGPU_SOURCES})
set_target_properties(avgpu PROPERTIES SUFFIX .sys)
target_include_directories(avgpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
