# rvktest.exe, the in-guest D3DKMT suite. Configured by the top-level build as
# an external project with cmake/windows_user_toolchain.cmake. The suite is
# src/guest; this directory translates it to Windows.
cmake_minimum_required(VERSION 3.24)
project(rvktest C CXX)

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
# The ICDs' shims, rvkvkprobe.exe's and rvkinspect.exe's are their own
# targets below.
list(FILTER RVKTEST_SOURCES EXCLUDE REGEX "/um/(icd|vulkan|opengl|opencl|d3d|probe|inspect)/")
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
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_descriptor_template_default.cpp
  ${RVKTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp)

# The tree is built once per guest architecture (AV_GUEST_ARCH, from the
# toolchain): x64, and x86 for the drivers a 32-bit process loads.
if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_DLLTOOL_MACHINE i386)
  # What the compiler calls on x86 that a C runtime would define - the stack
  # probe and 64-bit arithmetic - comes from ntdll's own exports, as memcpy
  # does; the x64 probe is assembly of its own.
  set(AV_RUNTIME_HELPERS ntdll_x86)
  set(AV_STACK_PROBE "")
  list(REMOVE_ITEM RVKTEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
  # Every Vulkan non-dispatchable handle is one uint64_t on x86; this makes
  # each a type of its own, ahead of any Khronos header.
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/../icd/vulkan/non_dispatchable_handle.hpp>)
else()
  set(AV_DLLTOOL_MACHINE i386:x86-64)
  set(AV_RUNTIME_HELPERS "")
  set(AV_STACK_PROBE ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
endif()

# Import libraries naming exactly what the programs call; the SDK's own are
# not in the kit. vulkan-1 is the Khronos loader, which only the probe calls.
set(RVKTEST_IMPORTS "")
foreach(module gdi32 kernel32 ntdll ${AV_RUNTIME_HELPERS} vulkan-1)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
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

# rvkclprobe.exe, feature 052's proof through the Khronos loader (T030). The
# import library is named apart from the loader target's below: both would
# otherwise be OpenCL.lib in this directory, and the probe must build whether
# or not the pinned loader tree is there.
set(RVKCLPROBE_IMPORTS "")
foreach(module OpenCL)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}_import.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND RVKCLPROBE_IMPORTS ${library})
endforeach()
add_custom_target(rvkclprobe_imports DEPENDS ${RVKCLPROBE_IMPORTS})

# rvkvkprobe.exe, feature 045's proof through the Khronos loader.
file(GLOB RVKVKPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/*.cpp)
# And the OpenCL seam from the CL probe's directory - the entry point and the
# CL probe's own program stay there. Feature 053's shared-object steps need
# both APIs in one process: Vulkan is the only one that exports, and OpenCL is
# what opens what it exported (T019).
list(APPEND RVKVKPROBE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/probe/cl/windows_cl_entry.cpp)
add_executable(rvkvkprobe ${RVKVKPROBE_SOURCES})
target_link_options(rvkvkprobe PRIVATE /ENTRY:rvkvkprobe_entry)
# The present steps make a window and read it back (feature 046), so this
# probe links user32 as the GL one does; its import library is made below.
target_link_libraries(rvkvkprobe PRIVATE rvkguest ${RVKTEST_IMPORTS} ${VULKAN_LOADER_IMPORT}
                                         ${RVKCLPROBE_IMPORTS})
add_dependencies(rvkvkprobe rvkclprobe_imports)

# rvkvk.dll, the Vulkan ICD (feature 045): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, the shim both ICDs share in icd/ and its
# own in vulkan/. The generated
# default handle_to_wire of the dispatchable types is not linked: icd_hooks.cpp
# answers the host's own handle for those.
file(GLOB_RECURSE RVKVK_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/vulkan/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp)
list(REMOVE_ITEM RVKVK_SOURCES ${RVKTEST_GENERATED})
list(APPEND RVKVK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/suite/adapter_search.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_icd_entries.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_handle_default.cpp
  ${RVKTEST_GENERATED_DIR}/vulkan/vk_host_image_default.cpp)

# The probe's module-level assembly is Intel syntax that LTO's IR assembler
# reads as AT&T: it stays an ordinary object.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp PROPERTIES COMPILE_OPTIONS -fno-lto)
add_library(rvkvk SHARED ${RVKVK_SOURCES} ${AV_STACK_PROBE})
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
add_executable(rvkinspect ${RVKINSPECT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/icd/process_heap.cpp)
target_link_options(rvkinspect PRIVATE /ENTRY:rvkinspect_entry)
target_link_libraries(rvkinspect PRIVATE rvkguest ${RVKTEST_IMPORTS})

# Import libraries for what only the OpenGL programs call: opengl32 for the
# probe, user32 for both.
set(RVKGLPROBE_IMPORTS "")
foreach(module opengl32 user32)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND RVKGLPROBE_IMPORTS ${library})
endforeach()
add_custom_target(rvkglprobe_imports DEPENDS ${RVKGLPROBE_IMPORTS})
list(GET RVKGLPROBE_IMPORTS 1 USER32_IMPORT)

# rvkgl.dll, the OpenGL ICD (feature 050): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, windows through user32, the shim both
# ICDs share in icd/ and its own in opengl/.
file(GLOB_RECURSE RVKGL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opengl/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opengl/*.cpp)
list(REMOVE_ITEM RVKGL_SOURCES ${RVKTEST_GENERATED})
list(APPEND RVKGL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/suite/adapter_search.cpp
  ${RVKTEST_GENERATED_DIR}/opengl/gl_icd_entries.cpp
  ${RVKTEST_GENERATED_DIR}/opengl/gl_dispatch_client.cpp)
add_library(rvkgl SHARED ${RVKGL_SOURCES} ${AV_STACK_PROBE})
add_dependencies(rvkgl rvktest_imports rvkglprobe_imports)
set_target_properties(rvkgl PROPERTIES PREFIX "")
target_include_directories(rvkgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${RVKTEST_GENERATED_DIR}/opengl
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opengl)
target_link_options(rvkgl PRIVATE /ENTRY:rvkgl_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/opengl/rvkgl.def)
set_property(TARGET rvkgl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opengl/rvkgl.def)
target_link_libraries(rvkgl PRIVATE ${RVKTEST_IMPORTS} ${USER32_IMPORT})

# rvkcl.dll, the OpenCL ICD (feature 052): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, the shim both ICDs share in icd/ and its
# own in opencl/. gdi32, kernel32 and ntdll are the whole import list - this ICD
# makes no window, so no user32.
file(GLOB_RECURSE RVKCL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opencl/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opencl/*.cpp)
# The recursive proto glob above sweeps the generated tree, the host's server
# dispatch included; only the OpenCL client belongs in a guest DLL. The window
# surface goes too: it is the one thing in icd/ that calls user32, and an
# OpenCL ICD presents nothing.
list(REMOVE_ITEM RVKCL_SOURCES ${RVKTEST_GENERATED}
  ${CMAKE_CURRENT_SOURCE_DIR}/icd/gdi_window_surface.cpp)
list(APPEND RVKCL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/suite/adapter_search.cpp
  ${RVKTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp)
add_library(rvkcl SHARED ${RVKCL_SOURCES} ${AV_STACK_PROBE})
add_dependencies(rvkcl rvktest_imports)
set_target_properties(rvkcl PROPERTIES PREFIX "")
# compat/ first: it answers the <type_traits> the generated ICD stubs include,
# which a build with no C++ standard library has nothing else to resolve.
target_include_directories(rvkcl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/opencl/compat
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${RVKTEST_GENERATED_DIR}/opencl
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opencl)
target_link_options(rvkcl PRIVATE /ENTRY:rvkcl_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/opencl/rvkcl.def)
set_property(TARGET rvkcl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opencl/rvkcl.def)
target_link_libraries(rvkcl PRIVATE ${RVKTEST_IMPORTS})

# rvkglprobe.exe, feature 050's experiment through the system opengl32.dll.
file(GLOB RVKGLPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/gl/*.cpp)
add_executable(rvkglprobe ${RVKGLPROBE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/probe/windows_image_sink.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/probe/probe_window.cpp)
add_dependencies(rvkglprobe rvkglprobe_imports)
target_link_options(rvkglprobe PRIVATE /ENTRY:rvkglprobe_entry)
target_link_libraries(rvkglprobe PRIVATE rvkguest ${RVKTEST_IMPORTS} ${RVKGLPROBE_IMPORTS})


file(GLOB RVKCLPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/cl/*.cpp)
add_executable(rvkclprobe ${RVKCLPROBE_SOURCES})
add_dependencies(rvkclprobe rvkclprobe_imports)
target_compile_definitions(rvkclprobe PRIVATE CL_TARGET_OPENCL_VERSION=300)
target_link_options(rvkclprobe PRIVATE /ENTRY:rvkclprobe_entry)
target_link_libraries(rvkclprobe PRIVATE rvkguest ${RVKTEST_IMPORTS} ${RVKCLPROBE_IMPORTS})

# The three D3D probes (feature 047): DXVK's d3d9.dll and d3d11.dll and
# vkd3d-proton's d3d12.dll are what they call, which is the whole point of the
# run - the layer beside the probe, on the guest's Vulkan loader, on this ICD.
# Import libraries for what only they call; dxgi is D3D12's adapter
# enumeration.
set(RVKD3DPROBE_IMPORTS "")
foreach(module d3d9 d3d11 d3d12 dxgi)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND RVKD3DPROBE_IMPORTS ${library})
endforeach()
add_custom_target(rvkd3dprobe_imports DEPENDS ${RVKD3DPROBE_IMPORTS})

# One source list for three programs: each takes only what its own entry point
# reaches, and /OPT:REF drops the other two APIs' seams.
file(GLOB RVKD3DPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/d3d/*.cpp)
foreach(probe d3d9 d3d11 d3d12)
  add_executable(rvk${probe}probe ${RVKD3DPROBE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/probe/windows_image_sink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/probe/probe_window.cpp)
  add_dependencies(rvk${probe}probe rvkd3dprobe_imports rvkglprobe_imports)
  target_link_options(rvk${probe}probe PRIVATE /ENTRY:rvk${probe}probe_entry)
  target_link_libraries(rvk${probe}probe PRIVATE rvkguest ${RVKTEST_IMPORTS} ${RVKD3DPROBE_IMPORTS}
    ${USER32_IMPORT})
endforeach()

# A D3D11 sample application run on the guest's desktop: the D3D11 runtime
# and our user-mode driver, not a layer beside it.
add_executable(rvkd3d11sample ${CMAKE_CURRENT_SOURCE_DIR}/sample/rvkd3d11sample.cpp)
add_dependencies(rvkd3d11sample rvkd3dprobe_imports rvkglprobe_imports)
target_link_options(rvkd3d11sample PRIVATE /ENTRY:rvkd3d11sample_entry)
target_link_libraries(rvkd3d11sample PRIVATE ${RVKTEST_IMPORTS} ${RVKD3DPROBE_IMPORTS} ${USER32_IMPORT})

# The D3D11 samples (samples/d3d11): the report's guest side. Relaxed warnings:
# the samples are ports of other code, not this driver's.
file(GLOB RVKD3D11SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d11/*.cpp)
list(FILTER RVKD3D11SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(rvkd3d11samples ${RVKD3D11SAMPLES_SOURCES})
add_dependencies(rvkd3d11samples rvkd3dprobe_imports rvkglprobe_imports)
target_compile_options(rvkd3d11samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(rvkd3d11samples PRIVATE /ENTRY:rvkd3d11samples_entry /STACK:8388608,8388608)
target_link_libraries(rvkd3d11samples PRIVATE ${RVKTEST_IMPORTS} ${RVKD3DPROBE_IMPORTS})

# The OpenCL samples (samples/opencl): the OpenCL report's guest side. It
# imports no OpenCL: every entry point is resolved from the OpenCL.dll pushed
# beside it, as the host half resolves them from libOpenCL.
file(GLOB RVKCLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opencl/*.cpp)
list(FILTER RVKCLSAMPLES_SOURCES EXCLUDE REGEX "(host|loopback)_platform\\.cpp$")
add_executable(rvkclsamples ${RVKCLSAMPLES_SOURCES})
add_dependencies(rvkclsamples rvktest_imports)
target_include_directories(rvkclsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(rvkclsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(rvkclsamples PRIVATE /ENTRY:rvkclsamples_entry /STACK:8388608,8388608)
target_link_libraries(rvkclsamples PRIVATE ${RVKTEST_IMPORTS})

# The OpenGL samples (samples/opengl): tools/opengl_report.py's guest side,
# through the system opengl32.dll and whichever ICD it loads. Relaxed warnings
# as for the D3D11 samples.
file(GLOB RVKGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opengl/*.cpp)
list(FILTER RVKGLSAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(rvkglsamples ${RVKGLSAMPLES_SOURCES})
add_dependencies(rvkglsamples rvkglprobe_imports)
target_compile_options(rvkglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(rvkglsamples PRIVATE /ENTRY:rvkglsamples_entry /STACK:8388608,8388608)
target_link_libraries(rvkglsamples PRIVATE ${RVKTEST_IMPORTS} ${RVKGLPROBE_IMPORTS})

# The OpenCL-OpenGL sharing samples (samples/clgl): cl_khr_gl_sharing between
# the system opengl32.dll's context and the OpenCL.dll pushed beside it, each
# sample checking the values it moves.
file(GLOB RVKCLGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/clgl/*.cpp)
add_executable(rvkclglsamples ${RVKCLGLSAMPLES_SOURCES})
add_dependencies(rvkclglsamples rvktest_imports rvkglprobe_imports)
target_include_directories(rvkclglsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(rvkclglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(rvkclglsamples PRIVATE /ENTRY:rvkclglsamples_entry /STACK:8388608,8388608)
target_link_libraries(rvkclglsamples PRIVATE ${RVKTEST_IMPORTS} ${RVKGLPROBE_IMPORTS})

# The D3D12 samples (samples/d3d12): tools/d3d12_report.py's guest side, on
# the system d3d12.dll, which reaches rvkumd.dll's OpenAdapter12.
file(GLOB RVKD3D12SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d12/*.cpp)
list(FILTER RVKD3D12SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(rvkd3d12samples ${RVKD3D12SAMPLES_SOURCES})
add_dependencies(rvkd3d12samples rvkd3dprobe_imports rvkglprobe_imports)
target_compile_options(rvkd3d12samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(rvkd3d12samples PRIVATE /ENTRY:rvkd3d12samples_entry /STACK:8388608,8388608)
target_link_libraries(rvkd3d12samples PRIVATE ${RVKTEST_IMPORTS} ${RVKD3DPROBE_IMPORTS})

# rvkvk.dll puts a presented frame on its window through user32 and gdi32
# (feature 046), and rvkvkprobe.exe makes that window and reads it back;
# their import library is made with the GL probe's above.
add_dependencies(rvkvk rvkglprobe_imports)
target_link_libraries(rvkvk PRIVATE ${USER32_IMPORT})
add_dependencies(rvkvkprobe rvkglprobe_imports)
target_link_libraries(rvkvkprobe PRIVATE ${USER32_IMPORT})

# OpenCL.dll, the Khronos ICD loader, cross-built from the source
# tools/fetch_opencl_loader.py pins (feature 052, research §2). It is a guest
# *test* artifact: the runner pushes it beside rvkclprobe.exe so the proof runs
# through the real loader's own discovery of OpenCLDriverName, and the driver
# package ships no loader. Built only when the pinned tree is there.
set(OPENCL_LOADER_TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/opencl-loader)
if(EXISTS ${OPENCL_LOADER_TREE}/loader/icd.c)
  # cfgmgr32 is the SetupAPI discovery path; the app-package path reaches
  # kernel32 through GetProcAddress and needs no import library of its own.
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
  add_dependencies(OpenCL rvktest_imports opencl_loader_imports)
  set_target_properties(OpenCL PROPERTIES PREFIX "")
  # compat/ first: it answers the handful of C library names the loader calls,
  # which the guest build has no runtime for.
  target_include_directories(OpenCL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/opencl_loader/compat
    ${OPENCL_LOADER_TREE}/loader
    ${OPENCL_LOADER_TREE}/loader/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
  target_compile_definitions(OpenCL PRIVATE CL_TARGET_OPENCL_VERSION=300 UNICODE _UNICODE)
  # Forced: the loader's sources call malloc and the string functions without
  # including the headers that declare them, which a build with a C runtime
  # gets away with and this one does not.
  target_compile_options(OpenCL PRIVATE /FIwindows.h /FIav_loader_prelude.h /FIstdlib.h /FIstring.h /FIstdio.h)
  target_link_options(OpenCL PRIVATE /NODEFAULTLIB /ENTRY:av_opencl_loader_entry
    /DEF:${OPENCL_LOADER_TREE}/loader/windows/OpenCL.def)
  target_link_libraries(OpenCL PRIVATE ${RVKTEST_IMPORTS} ${OPENCL_LOADER_IMPORTS})
else()
  message(STATUS "OpenCL loader: skipped, no pinned tree; run tools/fetch_opencl_loader.py")
endif()

# rvkumd.dll, the Direct3D user-mode driver (feature 055): the driver's
# decisions in src/icd/d3d over the transport, D3DKMT through gdi32, and the
# shim all three user-mode drivers share in icd/. Unlike the two ICDs it links
# no generated tree - the D3D DDI is the WDK's shape, not a registry's - and
# unlike them it imports no user32: nothing here touches a window.
file(GLOB_RECURSE RVKUMD_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/d3d/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/icd/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d/*.cpp)
list(REMOVE_ITEM RVKUMD_SOURCES ${RVKTEST_GENERATED})
# ExtensionPolicy reports Khronos extensions, which a Direct3D driver has none
# of; GdiWindowSurface is the ICDs' presentation seam.
list(FILTER RVKUMD_SOURCES EXCLUDE REGEX "(extension_policy|gdi_window_surface)\\.cpp$")
list(APPEND RVKUMD_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/d3d/d3d_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/d3d12/d3d12_dispatch_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_kmt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/suite/adapter_search.cpp)
add_library(rvkumd SHARED ${RVKUMD_SOURCES} ${AV_STACK_PROBE})
add_dependencies(rvkumd rvktest_imports)
set_target_properties(rvkumd PROPERTIES PREFIX "")
target_include_directories(rvkumd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
# d3d10umddi.h includes d3dkmddi.h, the kernel DDI header, which the WDK ships
# in shared/ - the kit's wdk-shared. Only this target sees it: it is where the
# UMD DDI's own definitions come from, and nothing else in user mode needs it.
target_compile_options(rvkumd PRIVATE -imsvc/windows-kit/wdk-shared)
target_link_options(rvkumd PRIVATE /ENTRY:rvkumd_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/d3d/rvkumd.def)
set_property(TARGET rvkumd APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/d3d/rvkumd.def)
target_link_libraries(rvkumd PRIVATE ${RVKTEST_IMPORTS})

# The 32-bit drivers are the same files a WoW64 process loads beside the 64-bit
# ones, from one driver store directory: rvkgpu.inf's *Wow values name them.
if(AV_GUEST_ARCH STREQUAL "x86")
  foreach(driver rvkvk rvkgl rvkcl rvkumd)
    set_target_properties(${driver} PROPERTIES OUTPUT_NAME ${driver}32)
  endforeach()
endif()
