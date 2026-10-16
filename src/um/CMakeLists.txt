# avtest.exe, the in-guest D3DKMT suite. Configured by the top-level build as
# an external project with cmake/windows_user_toolchain.cmake. The suite is
# src/guest; this directory translates it to Windows.
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
# The ICDs' shims, avvkprobe.exe's and avinspect.exe's are their own
# targets below.
list(FILTER AVTEST_SOURCES EXCLUDE REGEX "/um/(common|vulkan|opengl|opencl|d3d|probe|inspect|sample)/")
# Of the generated tree, only the guest's encoders: the av scenarios send real
# Vulkan and OpenCL streams (044), and the host-loss scenario Direct3D 11's.
# The server dispatch is the host's, and the OpenGL client does not link
# before its ICD helpers exist (050).
file(GLOB_RECURSE AVTEST_GENERATED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated/*.cpp)
list(REMOVE_ITEM AVTEST_SOURCES ${AVTEST_GENERATED})
set(AVTEST_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/generated)
list(APPEND AVTEST_SOURCES
  ${AVTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_handle_default.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_dispatchable_handle_default.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_host_image_default.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_descriptor_template_default.cpp
  ${AVTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/d3d/d3d_dispatch_client.cpp
  # Where every program's report grows into (GuestReport).
  ${CMAKE_CURRENT_SOURCE_DIR}/common/process_heap.cpp)

# The tree is built once per guest architecture (AV_GUEST_ARCH, from the
# toolchain): x64, and x86 for the drivers a 32-bit process loads.
if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_DLLTOOL_MACHINE i386)
  # What the compiler calls on x86 that a C runtime would define - the stack
  # probe and 64-bit arithmetic - comes from ntdll's own exports, as memcpy
  # does; the x64 probe is assembly of its own.
  set(AV_RUNTIME_HELPERS ntdll_x86)
  set(AV_STACK_PROBE "")
  list(REMOVE_ITEM AVTEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
  # Every Vulkan non-dispatchable handle is one uint64_t on x86; this makes
  # each a type of its own, ahead of any Khronos header.
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/../icd/vulkan/non_dispatchable_handle.hpp>)
else()
  set(AV_DLLTOOL_MACHINE i386:x86-64)
  set(AV_RUNTIME_HELPERS "")
  set(AV_STACK_PROBE ${CMAKE_CURRENT_SOURCE_DIR}/stack_probe.cpp)
endif()

# What the MSVC ABI references with no C runtime: the programs take it from
# avguest, which globs it; each DLL names it. Native code, not bitcode: the
# references to it are made by LTO's own code generation, after which the
# linker can no longer pull a bitcode member out of an archive.
set(AV_CRT_STUBS ${CMAKE_CURRENT_SOURCE_DIR}/crt_stubs.cpp)
set_source_files_properties(${AV_CRT_STUBS} PROPERTIES COMPILE_OPTIONS -fno-lto)

# Import libraries naming exactly what the programs call; the SDK's own are
# not in the kit. vulkan-1 is the Khronos loader, which only the probe calls.
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

# The suite and the probe share everything but their entry points, as one
# archive the linker takes only what each program calls from.
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

# avclprobe.exe, feature 052's proof through the Khronos loader (T030). The
# import library is named apart from the loader target's below: both would
# otherwise be OpenCL.lib in this directory, and the probe must build whether
# or not the pinned loader tree is there.
set(AVCLPROBE_IMPORTS "")
foreach(module OpenCL)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}_import.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVCLPROBE_IMPORTS ${library})
endforeach()
add_custom_target(avclprobe_imports DEPENDS ${AVCLPROBE_IMPORTS})

# avvkprobe.exe, feature 045's proof through the Khronos loader.
file(GLOB AVVKPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/probe/vk/*.cpp)
# And the OpenCL seam from the CL probe's directory - the entry point and the
# CL probe's own program stay there. Feature 053's shared-object steps need
# both APIs in one process: Vulkan is the only one that exports, and OpenCL is
# what opens what it exported (T019).
list(APPEND AVVKPROBE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/probe/cl/windows_cl_entry.cpp)
add_executable(avvkprobe ${AVVKPROBE_SOURCES})
target_link_options(avvkprobe PRIVATE /ENTRY:avvkprobe_entry)
# The present steps make a window and read it back (feature 046), so this
# probe links user32 as the GL one does; its import library is made below.
target_link_libraries(avvkprobe PRIVATE avguest ${AVTEST_IMPORTS} ${VULKAN_LOADER_IMPORT}
                                         ${AVCLPROBE_IMPORTS})
add_dependencies(avvkprobe avclprobe_imports)

# avvk.dll, the Vulkan ICD (feature 045): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, the shim both ICDs share in common/ and its
# own in vulkan/. The generated
# default handle_to_wire of the dispatchable types is not linked: icd_hooks.cpp
# answers the host's own handle for those.
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
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/*.cpp)
list(REMOVE_ITEM AVVK_SOURCES ${AVTEST_GENERATED})
list(APPEND AVVK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_icd_entries.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_dispatch_client.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_handle_default.cpp
  ${AVTEST_GENERATED_DIR}/vulkan/vk_host_image_default.cpp)

# The probe's module-level assembly is Intel syntax that LTO's IR assembler
# reads as AT&T: it stays an ordinary object.
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

# avinspect.exe, the guest's reader of trace tables and state dumps
# (docs/trace.md): InspectProgram over the Windows host.
file(GLOB AVINSPECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inspect/*.cpp)
add_executable(avinspect ${AVINSPECT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/common/process_heap.cpp)
target_link_options(avinspect PRIVATE /ENTRY:avinspect_entry)
target_link_libraries(avinspect PRIVATE avguest ${AVTEST_IMPORTS})

# Import libraries for what only the OpenGL programs call: opengl32 for the
# probe, user32 for both.
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

# avgl.dll, the OpenGL ICD (feature 050): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, windows through user32, the shim both
# ICDs share in common/ and its own in opengl/.
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
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opengl/*.cpp)
list(REMOVE_ITEM AVGL_SOURCES ${AVTEST_GENERATED})
list(APPEND AVGL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/opengl/gl_icd_entries.cpp
  ${AVTEST_GENERATED_DIR}/opengl/gl_dispatch_client.cpp)
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

# avcl.dll, the OpenCL ICD (feature 052): the ICD's decisions in src/icd over
# the transport, D3DKMT through gdi32, the shim both ICDs share in common/ and its
# own in opencl/. gdi32, kernel32 and ntdll are the whole import list - this ICD
# makes no window, so no user32.
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
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/opencl/*.cpp)
# The recursive proto glob above sweeps the generated tree, the host's server
# dispatch included; only the OpenCL client belongs in a guest DLL. The window
# surface goes too: it is the one thing in common/ that calls user32, and an
# OpenCL ICD presents nothing.
list(REMOVE_ITEM AVCL_SOURCES ${AVTEST_GENERATED}
  ${CMAKE_CURRENT_SOURCE_DIR}/common/gdi_window_surface.cpp)
list(APPEND AVCL_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/kmt/gdi_kmt.cpp
  ${AVTEST_GENERATED_DIR}/opencl/cl_dispatch_client.cpp)
add_library(avcl SHARED ${AVCL_SOURCES} ${AV_STACK_PROBE} ${AV_CRT_STUBS})
add_dependencies(avcl avtest_imports)
set_target_properties(avcl PROPERTIES PREFIX "")
# compat/ first: it answers the <type_traits> the generated ICD stubs include,
# which a build with no C++ standard library has nothing else to resolve.
target_include_directories(avcl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/opencl/compat
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include
  ${AVTEST_GENERATED_DIR}/opencl
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/opencl)
target_link_options(avcl PRIVATE /ENTRY:avcl_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/opencl/avcl.def)
set_property(TARGET avcl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/opencl/avcl.def)
target_link_libraries(avcl PRIVATE ${AVTEST_IMPORTS})

# atiadlxx.dll and nvapi64.dll, the GPU vendors' management libraries as they
# describe avgpu's adapters (docs/vendor-libraries.md): what an application
# that trusts only a GPU its vendor's library vouches for asks. D3DKMT through
# gdi32, and no trace service, log or crash filter in the processes that load
# them (vendor/vendor_driver.hpp). Both installed into System32 and SysWOW64
# by avgpu.inf, under the names applications load them by.
file(GLOB AV_VENDOR_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/vendor/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp)
list(APPEND AV_VENDOR_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/thread_lock_hold.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/adapter_string.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/escape/umd_adapter_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/wire/wire_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/wire/wire_writer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/srw_lock.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/run_once.cpp
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

# avglprobe.exe, feature 050's experiment through the system opengl32.dll.
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

# The three D3D probes (feature 047): DXVK's d3d9.dll and d3d11.dll and
# vkd3d-proton's d3d12.dll are what they call, which is the whole point of the
# run - the layer beside the probe, on the guest's Vulkan loader, on this ICD.
# Import libraries for what only they call; dxgi is D3D12's adapter
# enumeration.
set(AVD3DPROBE_IMPORTS "")
foreach(module d3d9 d3d11 d3d12 dxgi)
  set(library ${CMAKE_CURRENT_BINARY_DIR}/${module}.lib)
  add_custom_command(OUTPUT ${library}
    COMMAND llvm-dlltool -m ${AV_DLLTOOL_MACHINE} -d ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def -l ${library}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/imports/${module}.def)
  list(APPEND AVD3DPROBE_IMPORTS ${library})
endforeach()
add_custom_target(avd3dprobe_imports DEPENDS ${AVD3DPROBE_IMPORTS})

# One source list for three programs: each takes only what its own entry point
# reaches, and /OPT:REF drops the other two APIs' seams.
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

# avoverlayprobe.exe: the overlay proof on the guest's own d3d11.dll and
# dxgi.dll - the runtime DWM composes with - so it runs from a directory with
# no Direct3D layer beside it.
file(GLOB AVOVERLAYPROBE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/probe/overlay/*.cpp)
add_executable(avoverlayprobe ${AVOVERLAYPROBE_SOURCES})
add_dependencies(avoverlayprobe avd3dprobe_imports avglprobe_imports)
target_link_options(avoverlayprobe PRIVATE /ENTRY:avoverlayprobe_entry)
target_link_libraries(avoverlayprobe PRIVATE avguest ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS} ${USER32_IMPORT})

# A D3D11 sample application run on the guest's desktop: the D3D11 runtime
# and our user-mode driver, not a layer beside it.
add_executable(avd3d11sample ${CMAKE_CURRENT_SOURCE_DIR}/sample/avd3d11sample.cpp)
add_dependencies(avd3d11sample avd3dprobe_imports avglprobe_imports)
target_link_options(avd3d11sample PRIVATE /ENTRY:avd3d11sample_entry)
target_link_libraries(avd3d11sample PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS} ${USER32_IMPORT})

# The D3D11 samples (samples/d3d11): the report's guest side. Relaxed warnings:
# the samples are ports of other code, not this driver's.
file(GLOB AVD3D11SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d11/*.cpp)
list(FILTER AVD3D11SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avd3d11samples ${AVD3D11SAMPLES_SOURCES})
add_dependencies(avd3d11samples avd3dprobe_imports avglprobe_imports)
target_compile_options(avd3d11samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avd3d11samples PRIVATE /ENTRY:avd3d11samples_entry /STACK:8388608,8388608)
target_link_libraries(avd3d11samples PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS})

# The OpenCL samples (samples/opencl): the OpenCL report's guest side. It
# imports no OpenCL: every entry point is resolved from the OpenCL.dll pushed
# beside it, as the host half resolves them from libOpenCL.
file(GLOB AVCLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opencl/*.cpp)
list(FILTER AVCLSAMPLES_SOURCES EXCLUDE REGEX "(host|loopback)_platform\\.cpp$")
add_executable(avclsamples ${AVCLSAMPLES_SOURCES})
add_dependencies(avclsamples avtest_imports)
target_include_directories(avclsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(avclsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avclsamples PRIVATE /ENTRY:avclsamples_entry /STACK:8388608,8388608)
target_link_libraries(avclsamples PRIVATE ${AVTEST_IMPORTS})

# The OpenGL samples (samples/opengl): tools/opengl_report.py's guest side,
# through the system opengl32.dll and whichever ICD it loads. Relaxed warnings
# as for the D3D11 samples.
file(GLOB AVGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/opengl/*.cpp)
list(FILTER AVGLSAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avglsamples ${AVGLSAMPLES_SOURCES})
add_dependencies(avglsamples avglprobe_imports)
target_compile_options(avglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avglsamples PRIVATE /ENTRY:avglsamples_entry /STACK:8388608,8388608)
target_link_libraries(avglsamples PRIVATE ${AVTEST_IMPORTS} ${AVGLPROBE_IMPORTS})

# The OpenCL-OpenGL sharing samples (samples/clgl): cl_khr_gl_sharing between
# the system opengl32.dll's context and the OpenCL.dll pushed beside it, each
# sample checking the values it moves.
file(GLOB AVCLGLSAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/clgl/*.cpp)
add_executable(avclglsamples ${AVCLGLSAMPLES_SOURCES})
add_dependencies(avclglsamples avtest_imports avglprobe_imports)
target_include_directories(avclglsamples SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/khronos/include)
target_compile_options(avclglsamples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avclglsamples PRIVATE /ENTRY:avclglsamples_entry /STACK:8388608,8388608)
target_link_libraries(avclglsamples PRIVATE ${AVTEST_IMPORTS} ${AVGLPROBE_IMPORTS})

# The D3D12 samples (samples/d3d12): tools/d3d12_report.py's guest side, on
# the system d3d12.dll, which reaches avumd.dll's OpenAdapter12.
file(GLOB AVD3D12SAMPLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/d3d12/*.cpp)
list(FILTER AVD3D12SAMPLES_SOURCES EXCLUDE REGEX "host_platform\\.cpp$")
add_executable(avd3d12samples ${AVD3D12SAMPLES_SOURCES})
add_dependencies(avd3d12samples avd3dprobe_imports avglprobe_imports)
target_compile_options(avd3d12samples PRIVATE /WX- /W3 -Wno-conversion -Wno-sign-conversion -Wno-old-style-cast
                       -Wno-shadow -Wno-unused-parameter /Gs1000000)
target_link_options(avd3d12samples PRIVATE /ENTRY:avd3d12samples_entry /STACK:8388608,8388608)
target_link_libraries(avd3d12samples PRIVATE ${AVTEST_IMPORTS} ${AVD3DPROBE_IMPORTS})

# avvk.dll puts a presented frame on its window through user32 and gdi32
# (feature 046), and avvkprobe.exe makes that window and reads it back;
# their import library is made with the GL probe's above.
add_dependencies(avvk avglprobe_imports)
target_link_libraries(avvk PRIVATE ${USER32_IMPORT})
add_dependencies(avvkprobe avglprobe_imports)
target_link_libraries(avvkprobe PRIVATE ${USER32_IMPORT})

# OpenCL.dll, the Khronos ICD loader, cross-built from the source
# tools/fetch_opencl_loader.py pins (feature 052, research §2). It is a guest
# *test* artifact: the runner pushes it beside avclprobe.exe so the proof runs
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
  add_dependencies(OpenCL avtest_imports opencl_loader_imports)
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
  target_link_libraries(OpenCL PRIVATE ${AVTEST_IMPORTS} ${OPENCL_LOADER_IMPORTS})
else()
  message(STATUS "OpenCL loader: skipped, no pinned tree; run tools/fetch_opencl_loader.py")
endif()

# avumd.dll, the Direct3D user-mode driver (feature 055): the driver's
# decisions in src/icd/d3d over the transport, D3DKMT through gdi32, and the
# shim all three user-mode drivers share in common/. Unlike the two ICDs it links
# no generated tree - the D3D DDI is the WDK's shape, not a registry's - and
# unlike them it imports no user32: nothing here touches a window.
file(GLOB_RECURSE AVUMD_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../icd/d3d/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/kmt/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../guest/transport/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../proto/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/log/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/platform/lock_marks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/trace/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d/*.cpp)
list(REMOVE_ITEM AVUMD_SOURCES ${AVTEST_GENERATED})
# ExtensionPolicy reports Khronos extensions, which a Direct3D driver has none
# of; GdiWindowSurface is the ICDs' presentation seam.
list(FILTER AVUMD_SOURCES EXCLUDE REGEX "(extension_policy|gdi_window_surface)\\.cpp$")
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
# d3d10umddi.h includes d3dkmddi.h, the kernel DDI header, which the WDK ships
# in shared/ - the kit's wdk-shared. Only this target sees it: it is where the
# UMD DDI's own definitions come from, and nothing else in user mode needs it.
target_compile_options(avumd PRIVATE -imsvc/windows-kit/wdk-shared)
target_link_options(avumd PRIVATE /ENTRY:avumd_entry /DEF:${CMAKE_CURRENT_SOURCE_DIR}/d3d/avumd.def)
set_property(TARGET avumd APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/d3d/avumd.def)
target_link_libraries(avumd PRIVATE ${AVTEST_IMPORTS})

# The 32-bit drivers are the same files a WoW64 process loads beside the 64-bit
# ones, from one driver store directory: avgpu.inf's *Wow values name them.
if(AV_GUEST_ARCH STREQUAL "x86")
  foreach(driver avvk avgl avcl avumd)
    set_target_properties(${driver} PROPERTIES OUTPUT_NAME ${driver}32)
  endforeach()
  # The vendors' own 32-bit names, which a 32-bit application loads.
  set_target_properties(atiadlxx PROPERTIES OUTPUT_NAME atiadlxy)
  set_target_properties(nvapi64 PROPERTIES OUTPUT_NAME nvapi)
endif()
