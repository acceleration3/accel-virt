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
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# rvkvk.dll's, rvkvkprobe.exe's and rvkinspect.exe's shims are their own
# targets below.
list(FILTER RVKTEST_SOURCES EXCLUDE REGEX "/um/(vulkan|probe|inspect)/")
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

# Import libraries naming exactly what the programs call; the SDK's own are
# not in the kit. vulkan-1 is the Khronos loader, which only the probe calls.
set(RVKTEST_IMPORTS "")
foreach(module gdi32 kernel32 ntdll vulkan-1)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m i386:x86-64 -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND RVKTEST_IMPORTS ${library})
endforeach()
list(POP_BACK RVKTEST_IMPORTS VULKAN_LOADER_IMPORT)
add_custom_target(rvktest_imports DEPENDS ${RVKTEST_IMPORTS} ${VULKAN_LOADER_IMPORT})

# The suite and the probe share everything but their entry points, as one
# archive the linker takes only what each program calls from.
set(RVKTEST_ENTRY ${CMAKE_CURRENT_SOURCE_DIR}/program_entry.cpp ${CMAKE_CURRENT_SOURCE_DIR}/guest_program.cpp)
list(REMOVE_ITEM RVKTEST_SOURCES ${RVKTEST_ENTRY})
add_library(rvkguest STATIC ${RVKTEST_SOURCES})
add_dependencies(rvkguest rvktest_imports)
target_include_directories(rvkguest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${RVKTEST_GENERATED_DIR}/vulkan
  ${RVKTEST_GENERATED_DIR}/opencl)

add_executable(rvktest ${RVKTEST_ENTRY})
target_link_options(rvktest PRIVATE /ENTRY:rvktest_entry)
target_link_libraries(rvktest PRIVATE rvkguest ${RVKTEST_IMPORTS})

# rvkvkprobe.exe, feature 045's proof through the Khronos loader.
file(GLOB RVKVKPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/*.cpp)
add_executable(rvkvkprobe ${RVKVKPROBE_SOURCES})
target_link_options(rvkvkprobe PRIVATE /ENTRY:rvkvkprobe_entry)
target_link_libraries(rvkvkprobe PRIVATE rvkguest ${RVKTEST_IMPORTS} ${VULKAN_LOADER_IMPORT})

# rvkvk.dll, the Vulkan ICD (feature 045): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32 and the shim in vulkan/. The generated
# default handle_to_wire of the dispatchable types is not linked: icd_hooks.cpp
# answers the host's own handle for those.
file(GLOB_RECURSE RVKVK_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
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

# rvkinspect.exe, the guest's reader of trace tables and state dumps
# (docs/trace.md): InspectProgram over the Windows host.
file(GLOB RVKINSPECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inspect/*.cpp)
add_executable(rvkinspect ${RVKINSPECT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/process_heap.cpp)
target_link_options(rvkinspect PRIVATE /ENTRY:rvkinspect_entry)
target_link_libraries(rvkinspect PRIVATE rvkguest ${RVKTEST_IMPORTS})
