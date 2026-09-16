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
# rvkvk.dll's shim is its own target below.
list(FILTER RVKTEST_SOURCES EXCLUDE REGEX "/um/vulkan/")
# Of the generated tree, only the guest's encoders: the rvk scenarios send real
# Vulkan and OpenCL streams (044). The server dispatch is the host's, and the
# OpenGL client does not link before its ICD helpers exist (050).
file(GLOB_RECURSE RVKTEST_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp)
list(REMOVE_ITEM RVKTEST_SOURCES ${RVKTEST_GENERATED})
set(RVKTEST_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated)
list(APPEND RVKTEST_SOURCES
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_handle_default.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_dispatchable_handle_default.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_host_image_default.cpp
  ${RVKTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp)

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
target_include_directories(rvktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${RVKTEST_GENERATED_DIR}/vulkan
  ${RVKTEST_GENERATED_DIR}/opencl)
target_link_libraries(rvktest PRIVATE ${RVKTEST_IMPORTS})

# rvkvk.dll, the Vulkan ICD (feature 045): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32 and the shim in vulkan/. The generated
# default handle_to_wire of the dispatchable types is not linked: icd_hooks.cpp
# answers the host's own handle for those.
file(GLOB_RECURSE RVKVK_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp)
list(REMOVE_ITEM RVKVK_SOURCES ${RVKTEST_GENERATED})
list(APPEND RVKVK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/suite/adapter_search.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_icd_entries.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_handle_default.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_host_image_default.cpp)

add_library(rvkvk SHARED ${RVKVK_SOURCES})
add_dependencies(rvkvk rvktest_imports)
set_target_properties(rvkvk PROPERTIES PREFIX "")
target_include_directories(rvkvk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${RVKTEST_GENERATED_DIR}/vulkan)
target_link_options(rvkvk PRIVATE /ENTRY:rvkvk_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/vulkan/rvkvk.def)
set_property(TARGET rvkvk APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/rvkvk.def)
target_link_libraries(rvkvk PRIVATE ${RVKTEST_IMPORTS})
