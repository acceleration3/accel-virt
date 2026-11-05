cmake_minimum_required(VERSION 3.24)
project(avtest C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE AVTEST_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(FILTER AVTEST_SOURCES EXCLUDE REGEX "/um/(common|vulkan|opengl|opencl|d3d|probe|inspect|sample)/")
list(APPEND AVTEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/common/diagnostics_directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/performance_clock.cpp)
file(GLOB_RECURSE AVTEST_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp)
list(REMOVE_ITEM AVTEST_SOURCES ${AVTEST_GENERATED})
set(AVTEST_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated)
list(APPEND AVTEST_SOURCES
  ${AVTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_handle_identity.cpp
  ${AVTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/d3d/d3d_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/process_heap.cpp)

if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_DLLTOOL_MACHINE i386)
  set(AV_RUNTIME_HELPERS ntdll_x86)
  set(AV_STACK_PROBE "")
  list(REMOVE_ITEM AVTEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/../icd/vulkan/non_dispatchable_handle.hpp>)
else()
  set(AV_DLLTOOL_MACHINE i386:x86-64)
  set(AV_RUNTIME_HELPERS "")
  set(AV_STACK_PROBE ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
endif()

set(AV_CRT_STUBS ${CMAKE_CURRENT_SOURCE_DIR}/crt_stubs.cpp)
set_source_files_properties(${AV_CRT_STUBS} PROPERTIES COMPILE_OPTIONS -fno-lto)

set(AVTEST_IMPORTS "")
foreach(module gdi32 kernel32 ntdll ${AV_RUNTIME_HELPERS} vulkan-1)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVTEST_IMPORTS ${library})
endforeach()
list(POP_BACK AVTEST_IMPORTS VULKAN_LOADER_IMPORT)
add_custom_target(avtest_imports DEPENDS ${AVTEST_IMPORTS} ${VULKAN_LOADER_IMPORT})

set(AVTEST_ENTRY ${CMAKE_CURRENT_SOURCE_DIR}/avtest/program_entry.cpp ${CMAKE_CURRENT_SOURCE_DIR}/avtest/guest_program.cpp)
list(REMOVE_ITEM AVTEST_SOURCES ${AVTEST_ENTRY})
add_library(avguest STATIC ${AVTEST_SOURCES})
add_dependencies(avguest avtest_imports)
target_include_directories(avguest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${AVTEST_GENERATED_DIR}/vulkan
  ${AVTEST_GENERATED_DIR}/opencl)

add_executable(avtest ${AVTEST_ENTRY})
target_link_options(avtest PRIVATE /ENTRY:avtest_entry)
target_link_libraries(avtest PRIVATE avguest ${AVTEST_IMPORTS})

set(AVCLPROBE_IMPORTS "")
foreach(module OpenCL)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}_import.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVCLPROBE_IMPORTS ${library})
endforeach()
add_custom_target(avclprobe_imports DEPENDS ${AVCLPROBE_IMPORTS})

file(GLOB AVVKPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/probe/vk/*.cpp)
list(APPEND AVVKPROBE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/probe/cl/windows_cl_entry.cpp)
add_executable(avvkprobe ${AVVKPROBE_SOURCES})
target_link_options(avvkprobe PRIVATE /ENTRY:avvkprobe_entry)
target_link_libraries(avvkprobe PRIVATE avguest ${AVTEST_IMPORTS} ${VULKAN_LOADER_IMPORT}
                                         ${AVCLPROBE_IMPORTS})
add_dependencies(avvkprobe avclprobe_imports)

file(GLOB_RECURSE AVVK_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/vulkan/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp)
list(REMOVE_ITEM AVVK_SOURCES ${AVTEST_GENERATED})
list(APPEND AVVK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_icd_entries.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp PROPERTIES COMPILE_OPTIONS -fno-lto)
add_library(avvk SHARED ${AVVK_SOURCES} ${AV_STACK_PROBE} ${AV_CRT_STUBS})
add_dependencies(avvk avtest_imports)
set_target_properties(avvk PROPERTIES PREFIX "")
target_include_directories(avvk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${AVTEST_GENERATED_DIR}/vulkan)
target_link_options(avvk PRIVATE /ENTRY:avvk_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/vulkan/avvk.def)
set_property(TARGET avvk APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/avvk.def)
target_link_libraries(avvk PRIVATE ${AVTEST_IMPORTS})

file(GLOB AVINSPECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inspect/*.cpp)
add_executable(avinspect ${AVINSPECT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/common/process_heap.cpp)
target_link_options(avinspect PRIVATE /ENTRY:avinspect_entry)
target_link_libraries(avinspect PRIVATE avguest ${AVTEST_IMPORTS})

set(AVGLPROBE_IMPORTS "")
foreach(module opengl32 user32)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVGLPROBE_IMPORTS ${library})
endforeach()
add_custom_target(avglprobe_imports DEPENDS ${AVGLPROBE_IMPORTS})
list(GET AVGLPROBE_IMPORTS 1 USER32_IMPORT)

file(GLOB_RECURSE AVGL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opengl/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opengl/*.cpp)
list(REMOVE_ITEM AVGL_SOURCES ${AVTEST_GENERATED})
list(APPEND AVGL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/opengl/gl_icd_entries.cpp
  ${AVTEST_GENERATED_DIR}/opengl/gl_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/glmem/glmem_dispatch_client.cpp)
add_library(avgl SHARED ${AVGL_SOURCES} ${AV_STACK_PROBE} ${AV_CRT_STUBS})
add_dependencies(avgl avtest_imports avglprobe_imports)
set_target_properties(avgl PROPERTIES PREFIX "")
target_include_directories(avgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${AVTEST_GENERATED_DIR}/opengl
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opengl)
target_link_options(avgl PRIVATE /ENTRY:avgl_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/opengl/avgl.def)
set_property(TARGET avgl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opengl/avgl.def)
target_link_libraries(avgl PRIVATE ${AVTEST_IMPORTS} ${USER32_IMPORT})

file(GLOB_RECURSE AVCL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opencl/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opencl/*.cpp)
list(REMOVE_ITEM AVCL_SOURCES ${AVTEST_GENERATED}
  ${CMAKE_CURRENT_SOURCE_DIR}/common/gdi_window_surface.cpp)
list(APPEND AVCL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp)
add_library(avcl SHARED ${AVCL_SOURCES} ${AV_STACK_PROBE} ${AV_CRT_STUBS})
add_dependencies(avcl avtest_imports)
set_target_properties(avcl PROPERTIES PREFIX "")
target_include_directories(avcl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/opencl/compat
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${AVTEST_GENERATED_DIR}/opencl
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opencl)
target_link_options(avcl PRIVATE /ENTRY:avcl_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/opencl/avcl.def)
set_property(TARGET avcl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opencl/avcl.def)
target_link_libraries(avcl PRIVATE ${AVTEST_IMPORTS})

file(GLOB AV_VENDOR_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/vendor/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp)
list(APPEND AV_VENDOR_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/adapter_facts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/adapter_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/adapter_string.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/escape/umd_adapter_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/wire/wire_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/wire/wire_writer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/srw_lock.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/run_once.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/performance_clock.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/vendor_record_layout.cpp)
foreach(vendor adl nvapi)
  if(vendor STREQUAL "adl")
    set(vendor_target atiadlxx)
  else()
    set(vendor_target nvapi64)
  endif()
  add_library(${vendor_target} SHARED ${AV_VENDOR_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/${vendor}_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/${vendor}_exports.cpp
    ${AV_STACK_PROBE} ${AV_CRT_STUBS})
  add_dependencies(${vendor_target} avtest_imports)
  set_target_properties(${vendor_target} PROPERTIES PREFIX "")
  target_include_directories(${vendor_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  target_link_options(${vendor_target} PRIVATE /ENTRY:av${vendor}_entry
                      /DEF:${CMAKE_CURRENT_SOURCE_DIR}/vendor/${vendor}.def)
  set_property(TARGET ${vendor_target} APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/${vendor}.def)
  target_link_libraries(${vendor_target} PRIVATE ${AVTEST_IMPORTS})
endforeach()

file(GLOB AVGLPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/gl/*.cpp)
add_executable(avglprobe ${AVGLPROBE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/probe/windows_image_sink.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/probe/probe_window.cpp)
add_dependencies(avglprobe avglprobe_imports)
target_link_options(avglprobe PRIVATE /ENTRY:avglprobe_entry)
target_link_libraries(avglprobe PRIVATE avguest ${AVTEST_IMPORTS} ${AVGLPROBE_IMPORTS})


file(GLOB AVCLPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/cl/*.cpp)
add_executable(avclprobe ${AVCLPROBE_SOURCES})
add_dependencies(avclprobe avclprobe_imports)
target_compile_definitions(avclprobe PRIVATE CL_TARGET_OPENCL_VERSION=300)
target_link_options(avclprobe PRIVATE /ENTRY:avclprobe_entry)
target_link_libraries(avclprobe PRIVATE avguest ${AVTEST_IMPORTS} ${AVCLPROBE_IMPORTS})

set(AVD3DPROBE_IMPORTS "")
foreach(module d3d9 d3d11 d3d12 dxgi)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVD3DPROBE_IMPORTS ${library})
endforeach()
add_custom_target(avd3dprobe_imports DEPENDS ${AVD3DPROBE_IMPORTS})

file(GLOB AVD3DPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/d3d/*.cpp)
foreach(probe d3d9 d3d11 d3d12)
  add_executable(av${probe}probe ${AVD3DPROBE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/probe/windows_image_sink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/probe/probe_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/probe/gdi_window_frame.cpp)
  add_dependencies(av${probe}probe avd3dprobe_imports avglprobe_imports)
  target_link_options(av${probe}probe PRIVATE /ENTRY:av${probe}probe_entry)
  target_link_libraries(av${probe}probe PRIVATE avguest ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS}
    ${USER32_IMPORT})
endforeach()

file(GLOB AVOVERLAYPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/overlay/*.cpp)
add_executable(avoverlayprobe ${AVOVERLAYPROBE_SOURCES})
add_dependencies(avoverlayprobe avd3dprobe_imports avglprobe_imports)
target_link_options(avoverlayprobe PRIVATE /ENTRY:avoverlayprobe_entry)
target_link_libraries(avoverlayprobe PRIVATE avguest ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS} ${USER32_IMPORT})

add_executable(avd3d11sample ${CMAKE_CURRENT_SOURCE_DIR}/sample/avd3d11sample.cpp)
add_dependencies(avd3d11sample avd3dprobe_imports avglprobe_imports)
target_link_options(avd3d11sample PRIVATE /ENTRY:avd3d11sample_entry)
target_link_libraries(avd3d11sample PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS} ${USER32_IMPORT})

file(GLOB AVD3D11SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d11/*.cpp)
list(FILTER AVD3D11SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avd3d11samples ${AVD3D11SAMPLES_SOURCES})
add_dependencies(avd3d11samples avd3dprobe_imports avglprobe_imports)
target_compile_options(avd3d11samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avd3d11samples PRIVATE /ENTRY:avd3d11samples_entry /STACK:8388608,8388608)
target_link_libraries(avd3d11samples PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS})

file(GLOB AVCLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opencl/*.cpp)
list(FILTER AVCLSAMPLES_SOURCES EXCLUDE REGEX "(host|loopback)_platform\\.cpp$")
add_executable(avclsamples ${AVCLSAMPLES_SOURCES})
add_dependencies(avclsamples avtest_imports)
target_include_directories(avclsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(avclsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avclsamples PRIVATE /ENTRY:avclsamples_entry /STACK:8388608,8388608)
target_link_libraries(avclsamples PRIVATE ${AVTEST_IMPORTS})

file(GLOB AVGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opengl/*.cpp)
list(FILTER AVGLSAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avglsamples ${AVGLSAMPLES_SOURCES})
add_dependencies(avglsamples avglprobe_imports)
target_compile_options(avglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avglsamples PRIVATE /ENTRY:avglsamples_entry /STACK:8388608,8388608)
target_link_libraries(avglsamples PRIVATE ${AVTEST_IMPORTS} ${AVGLPROBE_IMPORTS})

file(GLOB AVCLGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/clgl/*.cpp)
add_executable(avclglsamples ${AVCLGLSAMPLES_SOURCES})
add_dependencies(avclglsamples avtest_imports avglprobe_imports)
target_include_directories(avclglsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(avclglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avclglsamples PRIVATE /ENTRY:avclglsamples_entry /STACK:8388608,8388608)
target_link_libraries(avclglsamples PRIVATE ${AVTEST_IMPORTS} ${AVGLPROBE_IMPORTS})

file(GLOB AVD3D12SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d12/*.cpp)
list(FILTER AVD3D12SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avd3d12samples ${AVD3D12SAMPLES_SOURCES})
add_dependencies(avd3d12samples avd3dprobe_imports avglprobe_imports)
target_compile_options(avd3d12samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avd3d12samples PRIVATE /ENTRY:avd3d12samples_entry /STACK:8388608,8388608)
target_link_libraries(avd3d12samples PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS})

add_dependencies(avvk avglprobe_imports)
target_link_libraries(avvk PRIVATE ${USER32_IMPORT})
add_dependencies(avvkprobe avglprobe_imports)
target_link_libraries(avvkprobe PRIVATE ${USER32_IMPORT})

set(OPENCL_LOADER_TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/opencl-loader)
if(EXISTS ${OPENCL_LOADER_TREE}/loader/icd.c)
  set(OPENCL_LOADER_IMPORTS "")
  foreach(module cfgmgr32 advapi32 ole32)
    set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
    add_custom_command(OUTPUT ${library}
      COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
    list(APPEND OPENCL_LOADER_IMPORTS ${library})
  endforeach()
  add_custom_target(opencl_loader_imports DEPENDS ${OPENCL_LOADER_IMPORTS})

  add_library(OpenCL SHARED
    ${OPENCL_LOADER_TREE}/loader/icd.c
    ${OPENCL_LOADER_TREE}/loader/icd_dispatch.c
    ${OPENCL_LOADER_TREE}/loader/icd_dispatch_generated.c
    ${OPENCL_LOADER_TREE}/loader/windows/icd_windows.c
    ${OPENCL_LOADER_TREE}/loader/windows/icd_windows_apppackage.c
    ${OPENCL_LOADER_TREE}/loader/windows/icd_windows_dxgk.c
    ${OPENCL_LOADER_TREE}/loader/windows/icd_windows_envvars.c
    ${OPENCL_LOADER_TREE}/loader/windows/icd_windows_hkr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/opencl_loader/loader_crt.c)
  add_dependencies(OpenCL avtest_imports opencl_loader_imports)
  set_target_properties(OpenCL PROPERTIES PREFIX "")
  target_include_directories(OpenCL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/opencl_loader/compat
    ${OPENCL_LOADER_TREE}/loader
    ${OPENCL_LOADER_TREE}/loader/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
  target_compile_definitions(OpenCL PRIVATE CL_TARGET_OPENCL_VERSION=300 UNICODE _UNICODE)
  target_compile_options(OpenCL PRIVATE /FIwindows.h /FIav_loader_prelude.h /FIstdlib.h /FIstring.h /FIstdio.h)
  target_link_options(OpenCL PRIVATE /NODEFAULTLIB /ENTRY:av_opencl_loader_entry
    /DEF:${OPENCL_LOADER_TREE}/loader/windows/OpenCL.def)
  target_link_libraries(OpenCL PRIVATE ${AVTEST_IMPORTS} ${OPENCL_LOADER_IMPORTS})
else()
  message(STATUS "OpenCL loader: skipped, no pinned tree; run tools/fetch_opencl_loader.py")
endif()

file(GLOB_RECURSE AVUMD_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/poll_budget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/d3d/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d/*.cpp)
list(REMOVE_ITEM AVUMD_SOURCES ${AVTEST_GENERATED})
list(FILTER AVUMD_SOURCES EXCLUDE REGEX "gdi_window_surface\\.cpp$")
list(APPEND AVUMD_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/d3d/d3d_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/d3d12/d3d12_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/d3d9/d3d9_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp)
add_library(avumd SHARED ${AVUMD_SOURCES} ${AV_STACK_PROBE} ${AV_CRT_STUBS})
add_dependencies(avumd avtest_imports)
set_target_properties(avumd PROPERTIES PREFIX "")
target_include_directories(avumd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(avumd PRIVATE -imsvc/windows-kit/wdk-shared)
target_link_options(avumd PRIVATE /ENTRY:avumd_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/d3d/avumd.def)
set_property(TARGET avumd APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/d3d/avumd.def)
target_link_libraries(avumd PRIVATE ${AVTEST_IMPORTS})

if(AV_GUEST_ARCH STREQUAL "x86")
  foreach(driver avvk avgl avcl avumd)
    set_target_properties(${driver} PROPERTIES OUTPUT_NAME ${driver}32)
  endforeach()
  set_target_properties(atiadlxx PROPERTIES OUTPUT_NAME atiadlxy)
  set_target_properties(nvapi64 PROPERTIES OUTPUT_NAME nvapi)
endif()
