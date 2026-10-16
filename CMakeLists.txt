cmake_minimum_required(VERSION 3.24)
project(accel_virt CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "clang is required (constitution: Technology constraints)")
endif()

option(AV_COVERAGE "instrument for llvm-cov"        OFF)
option(AV_SANITIZE "build tests with ASan + UBSan"  ON)
option(AV_FUZZ     "build libFuzzer targets"        OFF)
option(AV_QEMU     "build the patched QEMU with the device and smoke-test it" OFF)
# The QEMU configuration is the one guests run on: with no build type, CMake
# compiles the core and the executor library unoptimized.
if(AV_QEMU AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(
  -Wall -Wextra -Werror
  -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  -Wcast-qual -Wpointer-arith -Wvla -Wnon-virtual-dtor -Woverloaded-virtual
  -Wno-unused-command-line-argument
  # The kernel subset: no exceptions, no RTTI, no C++ standard library. The
  # same flags the driver cross-build uses, so the native tests cannot rely on
  # anything the guest kernel will not have.
  -fno-exceptions -fno-rtti)

# ---- the Windows kit: fetched by pinned hash, never committed ----------------
set(AV_WINDOWS_KIT_DIR ${CMAKE_SOURCE_DIR}/third_party/windows-kit CACHE PATH
    "Windows kit subset written by tools/fetch_windows_kit.sh")
set(AV_WINDOWS_KIT_VERSION
    "Microsoft.Windows.WDK.x64 10.0.26100.6584\nMicrosoft.Windows.SDK.CPP 10.0.26100.9169\nheaders km shared um-closure-4 umddi-4")
foreach(required VERSION overlay.yaml include/km/ntddk.h include/um/Windows.h lib/x64/ntoskrnl.lib)
  if(NOT EXISTS ${AV_WINDOWS_KIT_DIR}/${required})
    message(FATAL_ERROR "Windows kit: ${AV_WINDOWS_KIT_DIR}/${required} is missing; "
                        "run tools/fetch_windows_kit.sh or set AV_WINDOWS_KIT_DIR")
  endif()
endforeach()
file(READ ${AV_WINDOWS_KIT_DIR}/VERSION kit_version)
string(STRIP "${kit_version}" kit_version)
if(NOT kit_version STREQUAL AV_WINDOWS_KIT_VERSION)
  message(FATAL_ERROR "Windows kit: ${AV_WINDOWS_KIT_DIR}/VERSION is\n${kit_version}\n"
                      "want\n${AV_WINDOWS_KIT_VERSION}\nrun tools/fetch_windows_kit.sh")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${AV_WINDOWS_KIT_DIR}/VERSION)

set(AV_SOURCE_DIR ${CMAKE_SOURCE_DIR})
include(cmake/check_fuzz_corpora.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/corpus)

# ---- portable code: no platform headers, compiled into every target --------
# src/device and src/proto join the same gates the moment they hold a source.
# Includes name a path from src/ ("miniport/display/edid.hpp"), from any file.
file(GLOB_RECURSE AV_PORTABLE_SOURCES CONFIGURE_DEPENDS
  src/core/*.cpp src/miniport/*.cpp src/proto/*.cpp src/device/*.cpp src/executor/*.cpp src/guest/*.cpp)

# What the generators wrote is committed, but it includes the API's own header,
# and that arrives with tools/fetch_khronos_registries.py. A clean checkout
# still builds: the generated sources join the build only once the headers are
# there, and the configure says which state it is in (feature 063).
set(AV_KHRONOS_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/khronos/include)
# Only the wire compiles today. The generator also writes the guest's dispatch
# and ICD entries and the host's server dispatch, and those name types no
# feature has built yet - a transport (043), an ICD connection (045), a backend
# registry (042). They are generated and committed all the same, because the
# gate compares the whole tree; each joins a build when its feature arrives.
# The three command-name tables are strings the registries gave their ids and
# nothing else - no API header, no registry type - so they compile wherever the
# rest of the tree does and the recording tools can name a command on a
# checkout with no registries fetched (feature 065).
set(AV_GENERATED_NAMES
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d/d3d_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d12/d3d12_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d9/d3d9_command_names.cpp)
set(AV_GENERATED_SOURCES
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_server.cpp
  # The OpenGL host decode path, which compiles now that the sync hooks are
  # fields on RealGl rather than free functions with nowhere to keep state
  # (feature 042).
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_dispatch_server.cpp
  # Direct3D's, from the registry this project writes (tools/codegen/registry).
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d/d3d_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d12/d3d12_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d9/d3d9_dispatch_server.cpp)

# The OpenGL tree writes its own gl_types.h from the registry and includes it by
# name, so it needs its own directory on the include path and no external header.
set(AV_GENERATED_OPENGL_INCLUDE ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl)
if(EXISTS ${AV_KHRONOS_INCLUDE}/vulkan/vulkan_core.h)
  set(AV_GENERATED ON)
  message(STATUS "generated sources: compiled against ${AV_KHRONOS_INCLUDE}")
else()
  set(AV_GENERATED OFF)
  message(STATUS "generated sources: skipped, no Khronos headers; "
                 "run tools/fetch_khronos_registries.py to compile them")
endif()
# Whatever the glob picked up from generated/, the list above is what builds.
# The executor library's C entry points define the symbol QEMU dlopens. They
# belong to that shared object and to nothing else: linked into av_core or
# av_device they would export av_executor_ops from a library nobody loads by
# that name (feature 042).
set(AV_EXECUTOR_ENTRY ${CMAKE_SOURCE_DIR}/src/executor/shim/executor_ops.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_EXECUTOR_ENTRY})
# avrecord's own entry point, for the same reason: a main() in a library is a
# second main wherever that library is linked (feature 065).
set(AV_RECORD_ENTRY ${CMAKE_SOURCE_DIR}/src/device/shim/recording_tool.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_RECORD_ENTRY})
# av-renderer's entry point, likewise (docs/renderer.md).
set(AV_RENDERER_ENTRY ${CMAKE_SOURCE_DIR}/src/device/shim/renderer_main.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_RENDERER_ENTRY})

file(GLOB_RECURSE AV_GENERATED_ALL CONFIGURE_DEPENDS src/proto/generated/*.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_GENERATED_ALL})
list(APPEND AV_PORTABLE_SOURCES ${AV_GENERATED_NAMES})
# The guest's encoders join the core too, and nothing of the device's: the av
# scenarios in src/guest send real Vulkan and OpenCL streams (044), and the
# device core for QEMU below never encodes one.
set(AV_GENERATED_CLIENT_SOURCES
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatch_client.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_handle_identity.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_client.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d/d3d_dispatch_client.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d12/d3d12_dispatch_client.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d9/d3d9_dispatch_client.cpp)
if(AV_GENERATED)
  list(APPEND AV_PORTABLE_SOURCES ${AV_GENERATED_SOURCES} ${AV_GENERATED_CLIENT_SOURCES})
endif()
add_library(av_core STATIC ${AV_PORTABLE_SOURCES})
target_include_directories(av_core PUBLIC src)
# The executor library is a shared object built from these same objects, and it
# must carry whatever sanitizers the configuration asked for - so av_core is
# position-independent rather than the .so linking the unsanitized av_device.
set_target_properties(av_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# The executor's backends run commands on the host's own Vulkan, the way the
# QEMU device already does (src/device/qemu/virtio-gpu-av-host.c). A host with
# the loader but no GPU still links and enumerates nothing.
find_library(AV_VULKAN_LIBRARY NAMES vulkan)
if(AV_VULKAN_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_VULKAN_LIBRARY})
endif()
# OpenCL the same way. Its ICD loader exports the whole 3.0 surface, which is
# what the generated backend forwards into; a host with no OpenCL device still
# loads it and enumerates no platform.
find_library(AV_OPENCL_LIBRARY NAMES OpenCL)
if(AV_OPENCL_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_OPENCL_LIBRARY})
endif()
# EGL, which is how an OpenGL context is made without a window. Everything past
# EGL 1.5 is resolved through eglGetProcAddress, so only the loader is linked.
find_library(AV_EGL_LIBRARY NAMES EGL)
if(AV_EGL_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_EGL_LIBRARY})
endif()
if(AV_GENERATED)
  target_include_directories(av_core PUBLIC ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
endif()
# The Direct3D API as DXVK's native build declares it, for the executor's
# Direct3D backend: third_party/dxvk is a submodule, so its headers are there
# on any checkout, whether or not DXVK itself was built.
set(AV_DXVK_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/dxvk/include/native/directx
                    ${CMAKE_SOURCE_DIR}/third_party/dxvk/include/native/windows)
target_include_directories(av_core SYSTEM PUBLIC ${AV_DXVK_INCLUDE})
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# The D3D11 samples' host side (samples/d3d11): DXVK native, the guest's
# shader bytecode, the same frames the guest renders - the report's baseline
# (tools/d3d11_report.py).
file(GLOB AV_D3D11_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/d3d11/*.cpp)
list(FILTER AV_D3D11_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
add_executable(avd3d11samples_host ${AV_D3D11_SAMPLES_SOURCES})
target_include_directories(avd3d11samples_host PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(avd3d11samples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avd3d11samples_host PRIVATE ${CMAKE_DL_LIBS})

# The OpenCL samples' host side (samples/opencl): the host's own OpenCL, loaded
# at run time as the guest loads OpenCL.dll - the report's baseline
# (tools/opencl_report.py).
file(GLOB AV_OPENCL_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/opencl/*.cpp)
list(FILTER AV_OPENCL_SAMPLES_SOURCES EXCLUDE REGEX "(guest|loopback)_platform\\.cpp$")
add_executable(avclsamples_host ${AV_OPENCL_SAMPLES_SOURCES})
target_include_directories(avclsamples_host SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/khronos/include)
target_compile_options(avclsamples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avclsamples_host PRIVATE ${CMAKE_DL_LIBS})

find_package(Threads REQUIRED)
# The OpenGL samples' host side (samples/opengl): the host's own driver through
# EGL, the same GLSL the guest compiles - the report's baseline
# (tools/opengl_report.py).
if(AV_EGL_LIBRARY)
  file(GLOB AV_OPENGL_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/opengl/*.cpp)
  list(FILTER AV_OPENGL_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
  add_executable(avglsamples_host ${AV_OPENGL_SAMPLES_SOURCES})
  target_compile_options(avglsamples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
  target_link_libraries(avglsamples_host PRIVATE ${AV_EGL_LIBRARY} Threads::Threads)
endif()

# The D3D12 samples' host side (samples/d3d12): vkd3d-proton native, the
# guest's DXIL, the same frames the guest renders (tools/d3d12_report.py).
file(GLOB AV_D3D12_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/d3d12/*.cpp)
list(FILTER AV_D3D12_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
add_executable(avd3d12samples_host ${AV_D3D12_SAMPLES_SOURCES})
target_include_directories(avd3d12samples_host PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(avd3d12samples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avd3d12samples_host PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)


# ---- the device core for QEMU: a PIC static library behind the C ABI -------
# No sanitizer or profile flags and hidden visibility: the archive exports only
# av_device.h, and tools/build_qemu.sh links it into the virtio-gpu-av module
# through the generated av_device.pc.
# TextBuffer, ByteRange, SettingText (under EnvSetting) and Fnv1aDigest (a guest
# page list's fingerprint) are the core/support classes with a body the device
# core uses, and DriverLog and LogLine the core/log ones; the trace table is
# core/trace's, every function opens a scope on it, and every device lock marks
# it (LockMarks).
file(GLOB_RECURSE AV_DEVICE_SOURCES CONFIGURE_DEPENDS src/device/*.cpp src/proto/*.cpp src/core/trace/*.cpp)
list(APPEND AV_DEVICE_SOURCES src/core/platform/lock_marks.cpp src/core/platform/thread_lock_hold.cpp)
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_RECORD_ENTRY})
list(APPEND AV_DEVICE_SOURCES src/core/support/text_buffer.cpp src/core/support/byte_range.cpp
  src/core/support/setting_text.cpp src/core/support/fnv1a_digest.cpp
  src/core/log/driver_log.cpp src/core/log/log_line.cpp)
# The renderer's own process code: it is linked into av-renderer, never into
# the freestanding module QEMU loads.
list(FILTER AV_DEVICE_SOURCES EXCLUDE REGEX "/src/device/(shim/(posix_renderer|renderer_main)|renderer/)[^/]*\\.cpp$")
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_GENERATED_ALL})
list(APPEND AV_DEVICE_SOURCES ${AV_GENERATED_NAMES})
if(AV_GENERATED)
  list(APPEND AV_DEVICE_SOURCES ${AV_GENERATED_SOURCES})
endif()
add_library(av_device STATIC ${AV_DEVICE_SOURCES})
target_include_directories(av_device PRIVATE src)
if(AV_GENERATED)
  target_include_directories(av_device PRIVATE ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
endif()
# Unwind tables, which -ffreestanding with -fno-exceptions leaves out: the crash
# file walks the faulting thread's stack through them (docs/trace.md).
target_include_directories(av_device SYSTEM PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(av_device PRIVATE -O2 -ffreestanding -fasynchronous-unwind-tables)
set_target_properties(av_device PROPERTIES
  POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
configure_file(cmake/av_device.pc.in ${CMAKE_BINARY_DIR}/av_device.pc.in @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/pkgconfig/av_device.pc INPUT ${CMAKE_BINARY_DIR}/av_device.pc.in)

# ---- the patched QEMU: its own configuration, since it needs nothing else ----
# ---- the executor library: the other end of src/device/abi/av_executor.h ---
# Built in every configuration, which is why it is defined before AV_QEMU's
# return rather than after it: the conformance suite runs it under ASan and
# UBSan in the default configuration, and the qemu configuration points a real
# device at the same object through its executor= property (feature 042).
add_library(av_executor SHARED ${AV_EXECUTOR_ENTRY})
# The host's own Vulkan, the way the QEMU device already takes it
# (src/device/qemu/virtio-gpu-av-host.c calls vkCreateInstance directly). A
# host with the loader but no GPU still loads this and enumerates nothing.
if(NOT AV_VULKAN_LIBRARY)
  message(FATAL_ERROR "libvulkan not found; the executor library runs commands on it")
endif()
target_link_libraries(av_executor PRIVATE av_core)
target_include_directories(av_executor PRIVATE src)
set_target_properties(av_executor PROPERTIES CXX_VISIBILITY_PRESET hidden
                                              VISIBILITY_INLINES_HIDDEN ON)
# The archive it links carries the whole core with default visibility, and this
# object has exactly one thing to say: the symbol QEMU looks up. Anything else
# reaching the dynamic table is surface nobody asked for.
target_link_options(av_executor PRIVATE -Wl,--exclude-libs,ALL)
# A command crosses a few dozen small functions of the core on its way to the
# host API, most in other translation units: link-time inlining takes a large
# share of the executor's per-command cost. Only here - av_device is an
# archive QEMU's own link reads, and it cannot read LTO objects.
if(AV_QEMU)
  set_target_properties(av_core av_executor PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  # Whatever links the core's LTO objects needs a linker that reads them.
  target_link_options(av_core INTERFACE -fuse-ld=lld)
endif()

# ---- avrecord: a recording printed, and replayed into a library (065) -----
# Built in every configuration, before AV_QEMU's return: it reads what a device
# wrote and drives the same av_executor.h a device drives, so it belongs
# wherever either of those is built.
add_executable(avrecord ${AV_RECORD_ENTRY})
target_link_libraries(avrecord PRIVATE av_core ${CMAKE_DL_LIBS})
target_include_directories(avrecord PRIVATE src)

# ---- av-renderer: the host GPU stack in a process of its own ----------------
# Built wherever avrecord is: it drives the same av_executor.h a device does,
# for a device that runs its contexts outside QEMU (docs/renderer.md).
add_executable(av-renderer ${AV_RENDERER_ENTRY})
target_link_libraries(av-renderer PRIVATE av_core ${CMAKE_DL_LIBS} pthread)
target_include_directories(av-renderer PRIVATE src)

# ---- two libraries that exist to be held to the ABI ------------------------
# Built in every configuration, before AV_QEMU's return: the qemu smoke and
# virtio-gpu suites load them, and so does the conformance suite, which is only
# worth running against more than this project's newest library.
add_library(av_echo_executor SHARED tests/qemu/echo_executor.cpp)
target_include_directories(av_echo_executor PRIVATE src/device/abi)
# A library that loads and answers no table, so the smoke test can prove realize
# fails rather than a capset-less device coming up (feature 042).
add_library(av_null_ops_executor SHARED tests/qemu/null_ops_executor.cpp)
target_include_directories(av_null_ops_executor PRIVATE src/device/abi)

if(AV_QEMU)
  enable_testing()
  add_custom_target(qemu ALL
    COMMAND ${CMAKE_SOURCE_DIR}/tools/build_qemu.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/pkgconfig
    DEPENDS av_device
    USES_TERMINAL
    COMMENT "QEMU with virtio-gpu-av")
  add_test(NAME qemu_smoke
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tests/qemu/qemu_smoke.py ${CMAKE_BINARY_DIR}/qemu
                   $<TARGET_FILE:av_echo_executor> $<TARGET_FILE:av_null_ops_executor>
                   $<TARGET_FILE:av_executor>)
  # The driver's own request bytes, sent to the real device by tests/qemu.
  add_executable(gpu_request_bytes tests/qemu/gpu_request_bytes.cpp)
  target_link_libraries(gpu_request_bytes PRIVATE av_core)
  add_test(NAME qemu_virtio_gpu
           COMMAND python3 -B -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests/qemu -p "test_*.py")
  set_tests_properties(qemu_virtio_gpu PROPERTIES
    ENVIRONMENT "AV_QEMU_BUILD=${CMAKE_BINARY_DIR}/qemu;AV_GPU_REQUEST_BYTES=$<TARGET_FILE:gpu_request_bytes>;AV_EXECUTOR=$<TARGET_FILE:av_echo_executor>;AV_RENDERER=$<TARGET_FILE:av-renderer>;AV_REAL_EXECUTOR=$<TARGET_FILE:av_executor>")
  return()
endif()

# ---- test harness ----------------------------------------------------------
# Harness includes name a path from tests/ ("harness/vidsch/fake_vidsch.hpp").
file(GLOB_RECURSE AV_HARNESS_SOURCES CONFIGURE_DEPENDS tests/harness/*.cpp)
list(FILTER AV_HARNESS_SOURCES EXCLUDE REGEX "/test_main\\.cpp$")
add_library(av_harness STATIC ${AV_HARNESS_SOURCES})
target_include_directories(av_harness PUBLIC tests)
# MockHost runs the device core's workers on real threads.
find_package(Threads REQUIRED)
target_link_libraries(av_harness PUBLIC Threads::Threads)
target_link_libraries(av_harness PUBLIC av_core)
target_compile_definitions(av_harness PUBLIC AV_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

# ---- a GPU that loses a device on request ----------------------------------
# VK_LAYER_AV_device_loss (tests/gpu_fault): a Vulkan layer whose
# vkAvLoseDeviceAV makes a device lost from then on, so the executor's answer
# to a host GPU fault is tested on this machine's GPU. Nothing loads it unless
# asked: the tests that lose a device name its directory in VK_ADD_LAYER_PATH
# (AV_LOSS_LAYER_ENVIRONMENT), and a guest run passes --device-loss-layer.
add_library(av_device_loss_layer SHARED tests/gpu_fault/device_loss_layer.cpp)
set_target_properties(av_device_loss_layer PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/gpu_fault CXX_VISIBILITY_PRESET hidden)
set(AV_LOSS_LAYER_LIBRARY ${CMAKE_BINARY_DIR}/gpu_fault/libav_device_loss_layer.so)
configure_file(tests/gpu_fault/VkLayer_av_device_loss.json.in ${CMAKE_BINARY_DIR}/gpu_fault/VkLayer_av_device_loss.json
               @ONLY)
set(AV_LOSS_LAYER_ENVIRONMENT
  "VK_ADD_LAYER_PATH=${CMAKE_BINARY_DIR}/gpu_fault;VK_INSTANCE_LAYERS=VK_LAYER_AV_device_loss")

# Sanitizers, profile instrumentation and libFuzzer each need every object built
# their way, which is why they are separate build configurations (tools/gate.sh).
if(AV_COVERAGE AND AV_FUZZ)
  message(FATAL_ERROR "AV_COVERAGE and AV_FUZZ are separate configurations; "
                      "use build-coverage/ and build-fuzz/ (tools/gate.sh)")
endif()
set(AV_TEST_FLAGS "")
if(AV_SANITIZE)
  list(APPEND AV_TEST_FLAGS -fsanitize=address,undefined
                            -fno-omit-frame-pointer
                            -fno-sanitize-recover=all)
endif()
if(AV_COVERAGE)
  # Atomic counters: the device core's tests run real worker threads, and a lost
  # increment corrupts the branch counts derived from it.
  list(APPEND AV_TEST_FLAGS -fprofile-instr-generate -fcoverage-mapping -fprofile-update=atomic)
endif()
if(AV_FUZZ)
  list(APPEND AV_TEST_FLAGS -fsanitize=fuzzer-no-link)
endif()

foreach(t av_core av_harness)
  target_compile_options(${t} PRIVATE ${AV_TEST_FLAGS})
  target_link_options(${t} PUBLIC ${AV_TEST_FLAGS})
endforeach()

# ---- the Vulkan ICD's portable core: src/icd over the generated wrappers ------
# Not in av_core: the ICD defines the hooks its generated wrappers call by name
# (icd_hooks.cpp), which only what links the wrappers may hold. The DLL itself
# is src/um's (T029); on Linux this is what the ICD's unit tests and the
# full-stack rig link.
if(AV_GENERATED)
  file(GLOB_RECURSE AV_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/vulkan/*.cpp)
  add_library(av_icd STATIC ${AV_ICD_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_icd_entries.cpp)
  target_link_libraries(av_icd PUBLIC av_core)
  # The generated wrappers include their siblings by the names the generator
  # gives them.
  target_include_directories(av_icd PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan)
  target_compile_options(av_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_icd PUBLIC ${AV_TEST_FLAGS})

  # The generated OpenGL client and ICD entries, built against the hooks the
  # OpenGL ICD declares (src/proto/client/gl_icd_support.hpp, src/icd/opengl/gl_helpers.h),
  # so a change to either side that breaks the other fails the build; the
  # OpenGL ICD links them once it defines the hooks (feature 050).
  add_library(av_gl_generated OBJECT
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_dispatch_client.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_icd_entries.cpp)
  target_link_libraries(av_gl_generated PRIVATE av_core)
  target_include_directories(av_gl_generated PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl
    ${CMAKE_SOURCE_DIR}/src/icd/opengl)
  target_compile_options(av_gl_generated PRIVATE ${AV_TEST_FLAGS})

  # The OpenGL ICD's portable core: src/icd/opengl over the generated client
  # and entries, whose hooks it defines. Its own archive, apart from the Vulkan
  # ICD's, as the two DLLs are apart.
  file(GLOB_RECURSE AV_GL_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/opengl/*.cpp)
  add_library(av_gl_icd STATIC ${AV_GL_ICD_SOURCES} $<TARGET_OBJECTS:av_gl_generated>)
  target_link_libraries(av_gl_icd PUBLIC av_core)
  target_include_directories(av_gl_icd PUBLIC ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl
    ${CMAKE_SOURCE_DIR}/src/icd/opengl)
  target_compile_options(av_gl_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_gl_icd PUBLIC ${AV_TEST_FLAGS})

  # The OpenCL client, built against the ICD's own hooks for the reason the
  # OpenGL pair above gives (feature 052).
  add_library(av_cl_generated OBJECT
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_client.cpp)
  target_link_libraries(av_cl_generated PRIVATE av_core)
  target_include_directories(av_cl_generated PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl
    ${CMAKE_SOURCE_DIR}/src/icd/opencl)
  target_compile_options(av_cl_generated PRIVATE ${AV_TEST_FLAGS})

  # The OpenCL ICD's portable core. Its own archive, for the reason the other
  # two have theirs: each defines strong hooks that replace the generated
  # defaults every other binary relies on.
  file(GLOB_RECURSE AV_CL_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/opencl/*.cpp)
  add_library(av_cl_icd STATIC ${AV_CL_ICD_SOURCES} $<TARGET_OBJECTS:av_cl_generated>)
  target_link_libraries(av_cl_icd PUBLIC av_core)
  target_include_directories(av_cl_icd PUBLIC ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl
    ${CMAKE_SOURCE_DIR}/src/icd/opencl)
  target_compile_options(av_cl_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_cl_icd PUBLIC ${AV_TEST_FLAGS})
endif()

# The Direct3D user-mode driver's portable core (feature 055): src/icd/d3d over
# the pieces all three user-mode drivers share. Its own archive, apart from the
# two ICDs', as the three DLLs are apart - and unlike them it needs no
# generated tree, because the D3D DDI is the WDK's shape, not a registry's.
# This archive names what it takes rather than globbing.
# The vendor management libraries' decisions (atiadlxx.dll, nvapi64.dll;
# docs/vendor-libraries.md): portable, over the Kmt and a thread lock.
file(GLOB_RECURSE AV_VENDOR_SOURCES CONFIGURE_DEPENDS src/icd/vendor/*.cpp)
add_library(av_vendor STATIC ${AV_VENDOR_SOURCES} ${CMAKE_SOURCE_DIR}/src/icd/common/adapter_facts.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/adapter_memory.cpp ${CMAKE_SOURCE_DIR}/src/icd/common/adapter_string.cpp)
target_link_libraries(av_vendor PUBLIC av_core)
target_compile_options(av_vendor PRIVATE ${AV_TEST_FLAGS})
target_link_options(av_vendor PUBLIC ${AV_TEST_FLAGS})

file(GLOB_RECURSE AV_D3D_UMD_SOURCES CONFIGURE_DEPENDS src/icd/d3d/*.cpp)
add_library(av_d3d_umd STATIC ${AV_D3D_UMD_SOURCES}
  ${CMAKE_SOURCE_DIR}/src/icd/common/arena_spans.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/host_channel.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/locked_transport.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/pieces.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/range_table.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/row_pieces.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/shared_bytes.cpp
  ${CMAKE_SOURCE_DIR}/src/icd/common/transport_timeline.cpp)
target_link_libraries(av_d3d_umd PUBLIC av_core)
target_compile_options(av_d3d_umd PRIVATE ${AV_TEST_FLAGS})
target_link_options(av_d3d_umd PUBLIC ${AV_TEST_FLAGS})

# ---- unit tests: one binary per module, so a crash isolates -----------------
enable_testing()
if(AV_COVERAGE)
  # A profile left by an earlier run would keep a deleted test's branches covered.
  add_test(NAME coverage_clean
           COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/coverage)
  set_tests_properties(coverage_clean PROPERTIES FIXTURES_SETUP coverage_clean)
endif()
# tests/loopback: the L4 suite, guest core and device core in one process.
file(GLOB_RECURSE AV_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/test_*.cpp tests/loopback/test_*.cpp)
foreach(src ${AV_UNIT_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} tests/harness/framework/test_main.cpp)
  target_link_libraries(${name} PRIVATE av_harness)
  target_compile_options(${name} PRIVATE ${AV_TEST_FLAGS})
  add_test(NAME ${name} COMMAND ${name})
  # MESA_SHADER_CACHE_DISABLE: a warm Mesa shader cache has the driver spawn a
  # thread that allocates out of anonymous memory, which LeakSanitizer reports
  # as `<unknown module>` and no `leak:` suppression can name. Whether it
  # happens depends on what the machine compiled before, so the same test
  # passes or fails on one box depending on its cache - and the cache buys
  # these tests nothing, since none of them measures compile time.
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/${name}.profraw;MESA_SHADER_CACHE_DISABLE=true")
  # Two tests name the generated clients' headers by the names the generator
  # includes them by; the clients themselves are in av_core.
  # The full-stack rig's av run links the executor library's own entry point:
  # its device core runs the table av_executor_ops() answers, in process, on
  # this machine's GPU (feature 044, T028).
  # DXVK's driver threads allocate from modules they unload before exit, which
  # LeakSanitizer reports as `<unknown module>` and no suppression can name.
  if(name STREQUAL "test_d3d_backend_device" OR name STREQUAL "test_d3d_tir_device" OR
     name STREQUAL "test_d3d12_session_device" OR
     name STREQUAL "test_d3d12_session_features_device" OR name STREQUAL "test_d3d12_session_tiles_device" OR
     name STREQUAL "test_d3d_gl_interop_device" OR name STREQUAL "test_d3d_cl_interop_device" OR
     name STREQUAL "test_frame_write_device" OR name STREQUAL "test_buffer_slice_device" OR
     name STREQUAL "test_d3d_external_export_device" OR
     name STREQUAL "test_d3d9_session_device" OR
     name STREQUAL "test_d3d_vulkan_interop_device" OR name STREQUAL "test_d3d_device_loss_device" OR
     name STREQUAL "test_fullstack_device_loss")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT
      "ASAN_OPTIONS=detect_leaks=0;AV_D3D_PATH=${CMAKE_SOURCE_DIR}/build-d3d/lib;DXVK_LOG_PATH=${CMAKE_BINARY_DIR}")
  endif()
  if(name STREQUAL "test_fullstack_av" OR name STREQUAL "test_fullstack_device_loss")
    target_sources(${name} PRIVATE ${AV_EXECUTOR_ENTRY})
  endif()
  # The tests that lose a host device on purpose load the layer that does it
  # (tests/gpu_fault), and only they do.
  if(name STREQUAL "test_d3d_device_loss_device" OR name STREQUAL "test_fullstack_device_loss")
    add_dependencies(${name} av_device_loss_layer)
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT ${AV_LOSS_LAYER_ENVIRONMENT})
  endif()
  # The host makes the shader the guest's driver writes for a stream output
  # with no shader: the writer is the guest's, compiled here alone.
  if(name STREQUAL "test_d3d_backend_device")
    target_sources(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/icd/d3d/passthrough_geometry_shader.cpp)
  endif()
  # The renderer's process code, for real (docs/renderer.md): its host loads
  # the echo executor, and the process test spawns the built av-renderer.
  if(name STREQUAL "test_posix_renderer_host" OR name STREQUAL "test_renderer_process")
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_echo_executor av-renderer)
    target_compile_definitions(${name} PRIVATE AV_ECHO_EXECUTOR="$<TARGET_FILE:av_echo_executor>"
                                               AV_RENDERER_BINARY="$<TARGET_FILE:av-renderer>")
  endif()
  # The ICD's tests link its core, which no other test may: its hooks replace
  # the generated defaults every other binary relies on. The probe's loader
  # seam is tested through the ICD's own entry point, so it links it too.
  if(src MATCHES "/tests/unit/icd/opencl/" OR name STREQUAL "test_fullstack_opencl_icd")
      target_link_libraries(${name} PRIVATE av_cl_icd)
    elseif(src MATCHES "/tests/unit/icd/opengl/" OR name MATCHES "^test_(gl_seams|fake_opengl32|fullstack_opengl_icd)$")
    target_link_libraries(${name} PRIVATE av_gl_icd)
  elseif(src MATCHES "/tests/unit/icd/d3d/")
    target_link_libraries(${name} PRIVATE av_d3d_umd)
  elseif(src MATCHES "/tests/unit/icd/vendor/")
    target_link_libraries(${name} PRIVATE av_vendor)
  elseif(src MATCHES "/tests/unit/icd/" OR name STREQUAL "test_loader_vulkan")
    target_link_libraries(${name} PRIVATE av_icd)
  endif()
  # The probe through the ICD on this machine's GPU (feature 045, T038): the
  # ICD's hooks and the executor's cannot share a binary, so the executor is
  # the library file QEMU loads, its symbols its own.
  if(name STREQUAL "test_fullstack_opengl_icd")
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    # The probe's renders, as bitmaps a person can open (docs/testing.md).
    target_compile_definitions(${name} PRIVATE AV_IMAGE_DIR="${CMAKE_BINARY_DIR}/images")
  endif()
  # The OpenCL ICD through the same stack (feature 052, T031): the ICD's hooks
  # and the executor's cannot share a binary, so the executor is the library
  # file QEMU loads, its symbols its own. The event watcher's callbacks are
  # delivered on a thread the test steps, so no thread library is needed
  # beyond the one the deadlock scenarios' single setter thread uses.
  if(name STREQUAL "test_fullstack_opencl_icd")
    find_package(Threads REQUIRED)
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    # The executor library opens every host API when it loads, Vulkan among
    # them, so this test carries the Vulkan loader whether or not it asks
    # anything of it - and a loader that unloads a driver reports its leaks
    # from a module no suppression can name. Its three siblings pin this for
    # the same reason.
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
  # The descriptor template proof through the same stack (feature 047): the
  # guest sizes a blob from the template it remembers and the device refuses a
  # length its own template does not account for, which only both halves
  # together can show.
  if(name STREQUAL "test_fullstack_templates")
    target_link_libraries(${name} PRIVATE av_icd ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
  # Two devices in one process sharing memory and a semaphore through the Win32
  # external objects (feature 047, US3): only the whole stack shows that the
  # translation to the host's fd mechanism works, because the sharing happens
  # between two host devices.
  if(name STREQUAL "test_fullstack_external_objects")
    target_link_libraries(${name} PRIVATE av_icd ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
  if(name STREQUAL "test_fullstack_vulkan_icd")
    target_link_libraries(${name} PRIVATE av_icd ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    # The probe's renders, as bitmaps a person can open (docs/testing.md).
    target_compile_definitions(${name} PRIVATE AV_IMAGE_DIR="${CMAKE_BINARY_DIR}/images")
    # The Vulkan loader unloads a driver when its last instance goes, and a
    # leak in it is then reported from an unknown module no suppression can
    # name.
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
  if(name STREQUAL "test_generated_clients" OR name STREQUAL "test_loopback_transport")
    target_include_directories(${name} PRIVATE
      ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan
      ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl)
  endif()
  if(AV_COVERAGE)
    set_tests_properties(${name} PROPERTIES
      FIXTURES_REQUIRED coverage_clean FIXTURES_SETUP coverage_profiles)
  endif()
endforeach()

# The OpenGL samples through the OpenGL ICD, the driver and the device core on
# this machine's GPU, with no guest (tests/loopback/gl_samples_loopback.cpp):
# what tools/opengl_report.py --loopback sets beside the host's frames. A tool,
# not a test - the ICD's own tests judge the ICD.
if(AV_GENERATED AND AV_EGL_LIBRARY)
  file(GLOB AV_GL_LOOPBACK_SOURCES CONFIGURE_DEPENDS samples/opengl/*.cpp)
  list(FILTER AV_GL_LOOPBACK_SOURCES EXCLUDE REGEX "(host|guest)_platform\\.cpp$")
  set_source_files_properties(${AV_GL_LOOPBACK_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
  add_executable(avglsamples_loopback tests/loopback/gl_samples_loopback.cpp ${AV_GL_LOOPBACK_SOURCES})
  target_include_directories(avglsamples_loopback PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(avglsamples_loopback PRIVATE av_harness av_gl_icd ${CMAKE_DL_LIBS})
  target_compile_options(avglsamples_loopback PRIVATE ${AV_TEST_FLAGS})
  add_dependencies(avglsamples_loopback av_executor)
  target_compile_definitions(avglsamples_loopback PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>"
    AV_PROC_TABLE="${CMAKE_SOURCE_DIR}/tools/codegen/profiles/opengl_proc_table.txt")
endif()
if(AV_COVERAGE)
  add_test(NAME coverage_gate
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_coverage.py ${CMAKE_BINARY_DIR})
  set_tests_properties(coverage_gate PROPERTIES FIXTURES_REQUIRED coverage_profiles)
endif()

# The OpenCL samples through the guest's whole path in one process: avcl's core
# over the L4 rig and the executor library on this machine's GPU. Not a test -
# nothing here judges it but tools/opencl_report.py --loopback, which compares
# its outputs with avclsamples_host's the way it compares the VM's.
if(AV_GENERATED)
  file(GLOB AV_OPENCL_LOOPBACK_SOURCES CONFIGURE_DEPENDS samples/opencl/*.cpp)
  list(FILTER AV_OPENCL_LOOPBACK_SOURCES EXCLUDE REGEX "(guest|host)_platform\\.cpp$")
  add_executable(avclsamples_loopback ${AV_OPENCL_LOOPBACK_SOURCES})
  target_compile_options(avclsamples_loopback PRIVATE -O2 -w)
  target_link_libraries(avclsamples_loopback PRIVATE av_harness av_cl_icd ${CMAKE_DL_LIBS})
  add_dependencies(avclsamples_loopback av_executor)
  target_compile_definitions(avclsamples_loopback PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
endif()

# ---- the driver: the same core, compiled for the Windows kernel ------------
include(ExternalProject)
# A toolchain's flags reach a build tree only through its first configure: CMake
# caches the *_INIT variables. Each external project therefore configures again,
# from an empty cache, whenever its toolchain or flags file changes; otherwise a
# changed flag silently never reaches an existing tree.
ExternalProject_Add(avgpu
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/km
  BINARY_DIR ${CMAKE_BINARY_DIR}/driver
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/driver/avgpu.sys)
ExternalProject_Add_StepDependencies(avgpu configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_kernel_flags.cmake)
add_test(NAME driver_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_driver_image.py
                 ${CMAKE_BINARY_DIR}/driver/avgpu.sys
                 # displib's DxgkInitialize is linked, not just declared
                 --require-import IoGetDeviceObjectPointer)

# ---- the in-guest suite: src/guest and src/um, for Windows user mode ---------
ExternalProject_Add(avtest
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/um
  BINARY_DIR ${CMAKE_BINARY_DIR}/guest
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/guest/avtest.exe ${CMAKE_BINARY_DIR}/guest/avvk.dll
                   ${CMAKE_BINARY_DIR}/guest/avvkprobe.exe ${CMAKE_BINARY_DIR}/guest/avinspect.exe
                   ${CMAKE_BINARY_DIR}/guest/avgl.dll ${CMAKE_BINARY_DIR}/guest/avglprobe.exe
                   ${CMAKE_BINARY_DIR}/guest/avcl.dll ${CMAKE_BINARY_DIR}/guest/avclprobe.exe
                   ${CMAKE_BINARY_DIR}/guest/avumd.dll ${CMAKE_BINARY_DIR}/guest/avclsamples.exe
                   ${CMAKE_BINARY_DIR}/guest/atiadlxx.dll ${CMAKE_BINARY_DIR}/guest/nvapi64.dll)
ExternalProject_Add_StepDependencies(avtest configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags_c.cmake)
add_test(NAME guest_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avtest.exe
                 --require-import D3DKMTSubmitCommand)

# ---- the same tree for 32-bit processes: the x86 drivers a WoW64 process
# loads through avgpu.inf's *Wow values, and its programs to prove them with.
# The drivers are named avvk32.dll and so on: both sets install into one
# driver store directory. ----------------------------------------------------
ExternalProject_Add(avtest32
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/um
  BINARY_DIR ${CMAKE_BINARY_DIR}/guest32
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR} -DAV_GUEST_ARCH=x86
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/guest32/avvk32.dll ${CMAKE_BINARY_DIR}/guest32/avgl32.dll
                   ${CMAKE_BINARY_DIR}/guest32/avcl32.dll ${CMAKE_BINARY_DIR}/guest32/avumd32.dll
                   ${CMAKE_BINARY_DIR}/guest32/avtest.exe
                   ${CMAKE_BINARY_DIR}/guest32/atiadlxy.dll ${CMAKE_BINARY_DIR}/guest32/nvapi.dll)
ExternalProject_Add_StepDependencies(avtest32 configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags_c.cmake)

# ---- the driver package: every Windows driver, both architectures ----------
# `drivers` builds the kernel driver and the user-mode drivers for x64 and x86.
# The drivers component installs them as the guest's C:\av holds them, so the
# tree copies there whole and provision.ps1 runs as it is:
#
#   <prefix>/provision.ps1
#   <prefix>/adapters.txt              this host's GPU names (cmake/driver_package.cmake)
#   <prefix>/package/avgpu.sys, avgpu.inf, the x64 and x86 user-mode drivers,
#                    their Vulkan manifests and package.sha256
#
# `cmake --install build` installs it, into build/av-drivers unless
# CMAKE_INSTALL_PREFIX or --prefix says otherwise; it installs what is built, so
# build `drivers` first. The drivers are the only thing this project installs.
add_custom_target(drivers DEPENDS avgpu avtest avtest32)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/av-drivers CACHE PATH "Where the driver package is installed" FORCE)
endif()
install(FILES ${CMAKE_SOURCE_DIR}/tools/guest/provision.ps1 DESTINATION . COMPONENT drivers)
install(FILES
          ${CMAKE_BINARY_DIR}/driver/avgpu.sys ${CMAKE_SOURCE_DIR}/src/km/avgpu.inf
          ${CMAKE_BINARY_DIR}/guest/avumd.dll ${CMAKE_BINARY_DIR}/guest/avvk.dll
          ${CMAKE_SOURCE_DIR}/src/um/vulkan/avvk.json ${CMAKE_BINARY_DIR}/guest/avgl.dll
          ${CMAKE_BINARY_DIR}/guest/avcl.dll
          ${CMAKE_BINARY_DIR}/guest32/avumd32.dll ${CMAKE_BINARY_DIR}/guest32/avvk32.dll
          ${CMAKE_SOURCE_DIR}/src/um/vulkan/avvk32.json ${CMAKE_BINARY_DIR}/guest32/avgl32.dll
          ${CMAKE_BINARY_DIR}/guest32/avcl32.dll
          ${CMAKE_BINARY_DIR}/guest/atiadlxx.dll ${CMAKE_BINARY_DIR}/guest/nvapi64.dll
          ${CMAKE_BINARY_DIR}/guest32/atiadlxy.dll ${CMAKE_BINARY_DIR}/guest32/nvapi.dll
        DESTINATION package COMPONENT drivers)
install(CODE "set(AV_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")" COMPONENT drivers)
install(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/driver_package.cmake COMPONENT drivers)

# The probe binds to the Khronos loader and nothing else beyond the three
# (feature 045).
# It links OpenCL beside Vulkan from feature 053: the shared-object steps are
# the one place in the guest where two APIs meet over one allocation, and only
# a process with both can walk them (T019).
add_test(NAME avvkprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avvkprobe.exe
                 --allow-import vulkan-1.dll --allow-import user32.dll --allow-import OpenCL.dll
                 --require-import vkGetInstanceProcAddr --require-import clCreateBufferWithProperties)

# The trace reader needs nothing past the three either (docs/trace.md).
add_test(NAME avinspect_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avinspect.exe
                 --require-import D3DKMTEscape --require-import OpenFileMappingW)

# The Vulkan ICD exports exactly what the loader calls (feature 045).
add_test(NAME avvk_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avvk.dll --dll --only-exports --allow-import user32.dll
                 --require-export vk_icdNegotiateLoaderICDInterfaceVersion
                 --require-export vk_icdGetInstanceProcAddr)

# The OpenGL ICD exports exactly the Drv* entries opengl32 looks up (feature
# 050, contracts/icd_interface.md); user32 is what its window calls will need.
add_test(NAME avgl_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avgl.dll --dll --only-exports --allow-import user32.dll
                 --require-export DrvValidateVersion --require-export DrvSetCallbackProcs
                 --require-export DrvDescribePixelFormat --require-export DrvSetPixelFormat
                 --require-export DrvCreateContext --require-export DrvCreateLayerContext
                 --require-export DrvDeleteContext --require-export DrvSetContext
                 --require-export DrvReleaseContext --require-export DrvCopyContext
                 --require-export DrvShareLists --require-export DrvGetProcAddress
                 --require-export DrvDescribeLayerPlane --require-export DrvSwapLayerBuffers
                 --require-export DrvSwapBuffers)

# The Direct3D user-mode driver exports exactly the three names the runtimes
# resolve (feature 055, contracts/umd_interface.md; OpenAdapter for d3d9.dll,
# feature 064). No user32: nothing in it
# touches a window, and the DXGI seam presents through the KMD, not GDI.
add_test(NAME avumd_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avumd.dll --dll --only-exports
                 --require-export OpenAdapter10_2 --require-export OpenAdapter12
                 --require-export OpenAdapter --require-export AvAskSharedRows
                 --require-export AvWithdrawSharedRows)

# The OpenCL ICD exports exactly the two names the Khronos loader may resolve
# (feature 052, research §1); it presents nothing, so it wants no user32.
add_test(NAME avcl_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avcl.dll --dll --only-exports
                 --require-export clGetExtensionFunctionAddress
                 --require-export clIcdGetPlatformIDsKHR)

# The OpenCL probe binds to the Khronos loader beside it and nothing else
# (feature 052, T030); it opens no window, so it wants no user32.
add_test(NAME avclprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avclprobe.exe
                 --allow-import OpenCL.dll --require-import clGetPlatformIDs)

# The OpenCL samples import nothing past the three: OpenCL.dll is loaded, not
# linked, so the same program runs whichever loader sits beside it.
add_test(NAME avclsamples_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avclsamples.exe --require-import LoadLibraryA)

# The Khronos loader, cross-built beside the probe (feature 052, T005). It is
# somebody else's source, but it is linked by this build against a kit closure
# with no C runtime, and that is this repository's to get right: the check's
# relocated-pointer rule is what catches a C library call the link turned into
# an __imp_ pointer to nothing, which crashes on the first call rather than at
# the link. The guest build is a project of its own, so the condition is the
# one src/um/CMakeLists.txt builds the loader under, not its target.
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/opencl-loader/loader/icd.c)
  add_test(NAME opencl_loader_image
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                   ${CMAKE_BINARY_DIR}/guest/OpenCL.dll --dll
                   --allow-import cfgmgr32.dll --allow-import advapi32.dll --allow-import ole32.dll
                   --require-export clGetPlatformIDs --require-import CM_Get_Device_ID_ListW)
endif()

# The 32-bit drivers, held to what the 64-bit ones are: x86 images, each
# exporting exactly its loader's names, undecorated although every one of them
# is __stdcall on x86.
add_test(NAME avvk32_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest32/avvk32.dll --machine i386 --dll --only-exports
                 --allow-import user32.dll
                 --require-export vk_icdNegotiateLoaderICDInterfaceVersion
                 --require-export vk_icdGetInstanceProcAddr)
add_test(NAME avgl32_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest32/avgl32.dll --machine i386 --dll --only-exports
                 --allow-import user32.dll
                 --require-export DrvValidateVersion --require-export DrvSetCallbackProcs
                 --require-export DrvDescribePixelFormat --require-export DrvSetPixelFormat
                 --require-export DrvCreateContext --require-export DrvCreateLayerContext
                 --require-export DrvDeleteContext --require-export DrvSetContext
                 --require-export DrvReleaseContext --require-export DrvCopyContext
                 --require-export DrvShareLists --require-export DrvGetProcAddress
                 --require-export DrvDescribeLayerPlane --require-export DrvSwapLayerBuffers
                 --require-export DrvSwapBuffers)
add_test(NAME avumd32_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest32/avumd32.dll --machine i386 --dll --only-exports
                 --require-export OpenAdapter10_2 --require-export OpenAdapter12
                 --require-export OpenAdapter --require-export AvAskSharedRows
                 --require-export AvWithdrawSharedRows)
# The vendor libraries export exactly what their .def files name, which is
# what an application resolves by name (docs/vendor-libraries.md): NVAPI's one
# lookup, and ADL's ADL_ and ADL2_ calls. Neither presents anything, so neither
# wants user32; the 32-bit pair's names are undecorated, as AMD's and NVIDIA's.
file(STRINGS ${CMAKE_SOURCE_DIR}/src/um/vendor/adl.def adl_exports REGEX "^ADL")
set(adl_required_exports)
foreach(name IN LISTS adl_exports)
  list(APPEND adl_required_exports --require-export ${name})
endforeach()
foreach(vendor_image IN ITEMS "guest/atiadlxx.dll|amd64|adl" "guest/nvapi64.dll|amd64|nvapi"
                              "guest32/atiadlxy.dll|i386|adl" "guest32/nvapi.dll|i386|nvapi")
  string(REPLACE "|" ";" vendor_image "${vendor_image}")
  list(GET vendor_image 0 vendor_path)
  list(GET vendor_image 1 vendor_machine)
  list(GET vendor_image 2 vendor_api)
  get_filename_component(vendor_name ${vendor_path} NAME_WE)
  if(vendor_api STREQUAL "adl")
    set(vendor_exports ${adl_required_exports})
  else()
    set(vendor_exports --require-export nvapi_QueryInterface)
  endif()
  add_test(NAME ${vendor_name}_image
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                   ${CMAKE_BINARY_DIR}/${vendor_path} --machine ${vendor_machine} --dll --only-exports
                   ${vendor_exports})
endforeach()
add_test(NAME avcl32_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest32/avcl32.dll --machine i386 --dll --only-exports
                 --require-export clGetExtensionFunctionAddress
                 --require-export clIcdGetPlatformIDsKHR)
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/opencl-loader/loader/icd.c)
  add_test(NAME opencl_loader32_image
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                   ${CMAKE_BINARY_DIR}/guest32/OpenCL.dll --machine i386 --dll
                   --allow-import cfgmgr32.dll --allow-import advapi32.dll --allow-import ole32.dll
                   --require-export clGetPlatformIDs --require-import CM_Get_Device_ID_ListW)
endif()

# The OpenGL probe calls the system opengl32.dll and makes a window (050).
add_test(NAME avglprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avglprobe.exe
                 --allow-import opengl32.dll --allow-import user32.dll
                 --require-import wglCreateContext --require-import D3DKMTOpenAdapterFromHdc)

# The three D3D probes bind to the layer beside them and nothing else (feature
# 047, T028): DXVK's d3d9.dll and d3d11.dll, vkd3d-proton's d3d12.dll, and for
# D3D12 the dxgi.dll its adapter enumeration goes through. A probe that
# imported anything more would be proving something other than the layer.
add_test(NAME avd3d9probe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avd3d9probe.exe
                 --allow-import d3d9.dll --allow-import user32.dll
                 --require-import Direct3DCreate9 --require-import Direct3DCreate9Ex)
add_test(NAME avd3d11probe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avd3d11probe.exe
                 --allow-import d3d11.dll --allow-import user32.dll
                 --require-import D3D11CreateDeviceAndSwapChain)
add_test(NAME avd3d12probe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avd3d12probe.exe
                 --allow-import d3d12.dll --allow-import dxgi.dll --allow-import user32.dll
                 --require-import D3D12CreateDevice --require-import CreateDXGIFactory1)

# ---- gate scripts are tested like any other code ---------------------------
# One ctest per module, labelled `tools`, so `ctest -j` runs them side by side
# and `ctest -LE tools` is the fast loop. The guest runner's module drives a
# scripted QEMU per test and is most of the time, so it runs in shards. They
# are Python and the same in every configuration, so only the default one
# registers them.
if(NOT AV_COVERAGE AND NOT AV_FUZZ)
  set(AV_TOOLS_SHARDED test_run_guest_suite)
  # 24 on a 32-core machine: the run is bound by its slowest shard, not by how
  # many run at once, and at 12 one shard took as long as the whole rest of
  # the suite. run_test_shard.py fails a shard that selects nothing, so this
  # cannot be raised past the module's test count without saying so.
  set(AV_TOOLS_SHARDS 24)
  file(GLOB AV_TOOLS_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/tools/test_*.py)
  foreach(module_file ${AV_TOOLS_TESTS})
    get_filename_component(module ${module_file} NAME_WE)
    if(module IN_LIST AV_TOOLS_SHARDED)
      math(EXPR last "${AV_TOOLS_SHARDS} - 1")
      foreach(shard RANGE ${last})
        add_test(NAME tools_${module}_${shard}
                 COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/run_test_shard.py
                         ${CMAKE_SOURCE_DIR}/tests/tools ${module} ${shard} ${AV_TOOLS_SHARDS})
        set_tests_properties(tools_${module}_${shard} PROPERTIES LABELS tools)
      endforeach()
    else()
      add_test(NAME tools_${module}
               COMMAND python3 -B -m unittest ${module}
               WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/tools)
      set_tests_properties(tools_${module} PROPERTIES LABELS tools)
    endif()
  endforeach()
endif()

# ---- the committed generated sources are what the generators write --------
add_test(NAME generated_sources
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_generated.py)
# The D3D user-mode driver's table thunks are what the WDK's own headers say
# each slot's signature is; a hand edit, or a header that moved, fails here.
find_program(AV_CLANG_CL clang-cl)
if(AV_CLANG_CL AND EXISTS ${AV_WINDOWS_KIT_DIR}/include/um/d3d12umddi.h)
  add_test(NAME generated_d3d11_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d11_ddi.py --check)
  add_test(NAME generated_d3d12_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d12_ddi.py --check)
  add_test(NAME generated_d3d9_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d9_ddi.py --check)
endif()

# ---- the committed SPIR-V is what its GLSL compiles to (045) ----------------
add_test(NAME check_spirv
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_spirv.py --check)

# ---- the documented id space and the generator's id files agree (FR-007) ----
add_test(NAME protocol_ids
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_protocol_ids.py)

# ---- constitution VI for the Vulkan ICD: every reported extension is tested (045)
add_test(NAME extension_policy
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_extension_policy.py ${CMAKE_SOURCE_DIR})

# ---- what a pinned translation layer requires is what the ICD reports (047) -
# Skipped, with a line saying so, while a layer's pins are not fetched: the
# comparison needs the release's own source (tools/fetch_layers.py).
add_test(NAME layer_requirements
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_layer_requirements.py ${CMAKE_SOURCE_DIR})

# Every extension the ICD reports is exercised by a group in the CTS subset
# (feature 048, FR-005): constitution VI says nothing is reported that is not
# implemented, and this says nothing is reported that is not measured.
add_test(NAME cts_subset
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_cts_subset.py ${CMAKE_SOURCE_DIR})

# ---- constitution III and IV.5 --------------------------------------------
add_test(NAME design_rules
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_design.py ${CMAKE_SOURCE_DIR})
add_test(NAME symbol_tests
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_symbol_tests.py ${CMAKE_SOURCE_DIR})
# Every function opens with its trace scope (docs/trace.md).
add_test(NAME scope_marks
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/scope_marks.py --check ${CMAKE_SOURCE_DIR})

# ---- any executor library, held to the ABI this repository owns (FR-001) ----
# Not under AV_QEMU: the suite has to run sanitized, and that configuration
# returns before the sanitizer flags are set. It needs no QEMU either - an
# executor library is an ordinary shared object and this calls its ABI.
add_executable(executor_conformance
  tests/conformance/executor/executor_conformance.cpp
  tests/conformance/executor/executor_transport.cpp
  tests/conformance/executor/main.cpp)
target_include_directories(executor_conformance PRIVATE tests src
                           ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
target_link_libraries(executor_conformance PRIVATE av_core)
target_link_libraries(executor_conformance PRIVATE ${CMAKE_DL_LIBS})
# The same flags the tests carry: the library it loads is built with them, and
# a sanitized object cannot be opened by a runner that is not.
target_compile_options(executor_conformance PRIVATE ${AV_TEST_FLAGS})
target_link_options(executor_conformance PRIVATE ${AV_TEST_FLAGS})
# AV_CONFORMANCE_GPU is a host GPU's Vulkan device UUID, 32 hex characters, as
# `tools/host/list_gpus.py` prints it. With one the suite opens real executors
# and runs each API's simplest command; without one it checks what needs no GPU
# and says what it skipped, so a machine with no GPU still runs it honestly.
set(AV_CONFORMANCE_ARGS "")
if(DEFINED ENV{AV_CONFORMANCE_GPU})
  set(AV_CONFORMANCE_ARGS --gpu $ENV{AV_CONFORMANCE_GPU})
  message(STATUS "executor conformance: real executors on GPU $ENV{AV_CONFORMANCE_GPU}")
else()
  message(STATUS "executor conformance: no AV_CONFORMANCE_GPU; "
                 "what needs an executor will be reported as skipped")
endif()
add_test(NAME executor_conformance
         COMMAND executor_conformance ${AV_CONFORMANCE_ARGS}
                 $<TARGET_FILE:av_executor> $<TARGET_FILE:av_echo_executor>)

# ---- principle IX: a clean clone of this repository is all it takes ----------
add_test(NAME self_contained
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_self_contained.py ${CMAKE_SOURCE_DIR})

# ---- fuzz targets: a smoke run per target is a gate (constitution: Done #4) --
if(AV_FUZZ)
  if(NOT DEFINED ENV{AV_FUZZ_SECONDS})
    set(ENV{AV_FUZZ_SECONDS} 60)
  endif()
  file(GLOB AV_FUZZ_SOURCES CONFIGURE_DEPENDS tests/fuzz/fuzz_*.cpp)
  # The generated decoder's target includes the API's own header, which arrives
  # with the registry fetch; without it there is nothing to fuzz.
  if(NOT AV_GENERATED)
    list(REMOVE_ITEM AV_FUZZ_SOURCES ${CMAKE_SOURCE_DIR}/tests/fuzz/fuzz_generated_decode.cpp)
  endif()
  foreach(src ${AV_FUZZ_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE av_harness)
    # The ICD's target links its core, as the ICD's unit tests do.
    if(name STREQUAL "fuzz_physical_device_view" OR name STREQUAL "fuzz_extension_policy" OR
       name STREQUAL "fuzz_memory_type_table" OR name STREQUAL "fuzz_swapchain_create")
      target_link_libraries(${name} PRIVATE av_icd)
    endif()
    if(name STREQUAL "fuzz_context_attributes" OR name STREQUAL "fuzz_pixel_format_query")
      target_link_libraries(${name} PRIVATE av_gl_icd)
    endif()
    if(name STREQUAL "fuzz_cl_property_list" OR name STREQUAL "fuzz_cl_info_query" OR
       name STREQUAL "fuzz_cl_host_reply")
      target_link_libraries(${name} PRIVATE av_cl_icd)
    endif()
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    # New units land in the build tree; the committed seed corpus is read only.
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/${name})
    add_test(NAME ${name}_smoke
             COMMAND ${name} ${CMAKE_BINARY_DIR}/corpus/${name}
                     ${CMAKE_SOURCE_DIR}/tests/corpus/${name}
                     -max_total_time=$ENV{AV_FUZZ_SECONDS} -print_final_stats=1
                     -artifact_prefix=${CMAKE_BINARY_DIR}/corpus/${name}-)
    # The fuzz configuration's own work; its unit tests are the default one's.
    set_tests_properties(${name}_smoke PROPERTIES LABELS fuzz)
    # A target that reaches a real driver loads it with the host's own loader,
    # and what that brings in is not this repository's to fix: Mesa's shader
    # cache leaves allocations nothing can attribute, and the LLVM inside radv
    # frees an aligned operator new with free() from a static initialiser.
    # Neither is reachable from the bytes being fuzzed.
    set_tests_properties(${name}_smoke PROPERTIES
      ENVIRONMENT "MESA_SHADER_CACHE_DISABLE=true;ASAN_OPTIONS=alloc_dealloc_mismatch=0")
  endforeach()
endif()
