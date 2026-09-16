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
    "Microsoft.Windows.WDK.x64 10.0.26100.6584\nMicrosoft.Windows.SDK.CPP 10.0.26100.9169\nheaders km shared um-closure-1")
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
# Includes name a path from src/ ("core/display/edid.hpp"), from any file.
file(GLOB_RECURSE AV_PORTABLE_SOURCES CONFIGURE_DEPENDS
  src/core/*.cpp src/proto/*.cpp src/device/*.cpp src/guest/*.cpp)

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
set(AV_GENERATED_SOURCES
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_handle_default.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatchable_handle_default.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_host_image_default.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_handle_lifetime_default.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_command_names.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_command_names.cpp
  # The OpenGL host decode path, which compiles now that the sync hooks are
  # fields on RealGl rather than free functions with nowhere to keep state
  # (feature 042).
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_dispatch_server.cpp)

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
# rvk_device they would export rvk_executor_ops from a library nobody loads by
# that name (feature 042).
set(AV_EXECUTOR_ENTRY ${CMAKE_SOURCE_DIR}/src/device/shim/executor_ops.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_EXECUTOR_ENTRY})

file(GLOB_RECURSE AV_GENERATED_ALL CONFIGURE_DEPENDS src/proto/generated/*.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_GENERATED_ALL})
# The guest's encoders join the core too, and nothing of the device's: the rvk
# scenarios in src/guest send real Vulkan and OpenCL streams (044), and the
# device core for QEMU below never encodes one.
set(AV_GENERATED_CLIENT_SOURCES
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatch_client.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_client.cpp)
if(AV_GENERATED)
  list(APPEND AV_PORTABLE_SOURCES ${AV_GENERATED_SOURCES} ${AV_GENERATED_CLIENT_SOURCES})
endif()
add_library(av_core STATIC ${AV_PORTABLE_SOURCES})
target_include_directories(av_core PUBLIC src)
# The executor library is a shared object built from these same objects, and it
# must carry whatever sanitizers the configuration asked for - so av_core is
# position-independent rather than the .so linking the unsanitized rvk_device.
set_target_properties(av_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# The executor's backends run commands on the host's own Vulkan, the way the
# QEMU device already does (src/device/qemu/virtio-gpu-rvk-host.c). A host with
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
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# ---- the device core for QEMU: a PIC static library behind the C ABI -------
# No sanitizer or profile flags and hidden visibility: the archive exports only
# rvk_device.h, and tools/build_qemu.sh links it into the virtio-gpu-rvk module
# through the generated rvk_device.pc.
# TextBuffer is the one core/support class with a body the device core uses;
# the trace table is core/trace's, and every function opens a scope on it.
file(GLOB_RECURSE AV_DEVICE_SOURCES CONFIGURE_DEPENDS src/device/*.cpp src/proto/*.cpp src/core/trace/*.cpp)
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_EXECUTOR_ENTRY})
list(APPEND AV_DEVICE_SOURCES src/core/support/text_buffer.cpp)
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_GENERATED_ALL})
if(AV_GENERATED)
  list(APPEND AV_DEVICE_SOURCES ${AV_GENERATED_SOURCES})
endif()
add_library(rvk_device STATIC ${AV_DEVICE_SOURCES})
target_include_directories(rvk_device PRIVATE src)
if(AV_GENERATED)
  target_include_directories(rvk_device PRIVATE ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
endif()
target_compile_options(rvk_device PRIVATE -O2 -ffreestanding)
set_target_properties(rvk_device PROPERTIES
  POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
configure_file(cmake/rvk_device.pc.in ${CMAKE_BINARY_DIR}/rvk_device.pc.in @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/pkgconfig/rvk_device.pc INPUT ${CMAKE_BINARY_DIR}/rvk_device.pc.in)

# ---- the patched QEMU: its own configuration, since it needs nothing else ----
# ---- the executor library: the other end of src/device/abi/rvk_executor.h ---
# Built in every configuration, which is why it is defined before AV_QEMU's
# return rather than after it: the conformance suite runs it under ASan and
# UBSan in the default configuration, and the qemu configuration points a real
# device at the same object through its executor= property (feature 042).
add_library(rvk_executor SHARED ${AV_EXECUTOR_ENTRY})
# The host's own Vulkan, the way the QEMU device already takes it
# (src/device/qemu/virtio-gpu-rvk-host.c calls vkCreateInstance directly). A
# host with the loader but no GPU still loads this and enumerates nothing.
if(NOT AV_VULKAN_LIBRARY)
  message(FATAL_ERROR "libvulkan not found; the executor library runs commands on it")
endif()
target_link_libraries(rvk_executor PRIVATE av_core)
target_include_directories(rvk_executor PRIVATE src)
set_target_properties(rvk_executor PROPERTIES CXX_VISIBILITY_PRESET hidden
                                              VISIBILITY_INLINES_HIDDEN ON)
# The archive it links carries the whole core with default visibility, and this
# object has exactly one thing to say: the symbol QEMU looks up. Anything else
# reaching the dynamic table is surface nobody asked for.
target_link_options(rvk_executor PRIVATE -Wl,--exclude-libs,ALL)

# ---- two libraries that exist to be held to the ABI ------------------------
# Built in every configuration, before AV_QEMU's return: the qemu smoke and
# virtio-gpu suites load them, and so does the conformance suite, which is only
# worth running against more than this project's newest library.
add_library(rvk_echo_executor SHARED tests/qemu/echo_executor.cpp)
target_include_directories(rvk_echo_executor PRIVATE src/device/abi)
# A library that loads and answers no table, so the smoke test can prove realize
# fails rather than a capset-less device coming up (feature 042).
add_library(rvk_null_ops_executor SHARED tests/qemu/null_ops_executor.cpp)
target_include_directories(rvk_null_ops_executor PRIVATE src/device/abi)

if(AV_QEMU)
  enable_testing()
  add_custom_target(qemu ALL
    COMMAND ${CMAKE_SOURCE_DIR}/tools/build_qemu.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/pkgconfig
    DEPENDS rvk_device
    USES_TERMINAL
    COMMENT "QEMU with virtio-gpu-rvk")
  add_test(NAME qemu_smoke
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tests/qemu/qemu_smoke.py ${CMAKE_BINARY_DIR}/qemu
                   $<TARGET_FILE:rvk_echo_executor> $<TARGET_FILE:rvk_null_ops_executor>
                   $<TARGET_FILE:rvk_executor>)
  # The driver's own request bytes, sent to the real device by tests/qemu.
  add_executable(gpu_request_bytes tests/qemu/gpu_request_bytes.cpp)
  target_link_libraries(gpu_request_bytes PRIVATE av_core)
  add_test(NAME qemu_virtio_gpu
           COMMAND python3 -B -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests/qemu -p "test_*.py")
  set_tests_properties(qemu_virtio_gpu PROPERTIES
    ENVIRONMENT "AV_QEMU_BUILD=${CMAKE_BINARY_DIR}/qemu;AV_GPU_REQUEST_BYTES=$<TARGET_FILE:gpu_request_bytes>;AV_EXECUTOR=$<TARGET_FILE:rvk_echo_executor>;AV_REAL_EXECUTOR=$<TARGET_FILE:rvk_executor>")
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
# Not in av_core: the ICD defines the strong handle_to_wire hooks that unwrap a
# dispatchable handle, and the executor library, which links av_core, defines
# its own (feature 045). Only what links this sees the ICD's. The DLL itself is
# src/um's (T029); on Linux this is what the ICD's unit tests and the full-stack
# rig link.
if(AV_GENERATED)
  file(GLOB_RECURSE AV_ICD_SOURCES CONFIGURE_DEPENDS src/icd/*.cpp)
  # The client dispatch and the weak handle defaults are av_core's too, and are
  # compiled again here on purpose. A link reads archives in order: with them in
  # this archive, every handle_to_wire the wrappers reach is resolved here - the
  # ICD's five strong ones and the identity for the rest - before av_core is
  # read, so the executor's own strong definitions in vulkan_backend.cpp are
  # never pulled into an ICD link to collide with them.
  add_library(av_icd STATIC ${AV_ICD_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_icd_entries.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_dispatch_client.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_handle_default.cpp)
  target_link_libraries(av_icd PUBLIC av_core)
  # The generated wrappers include their siblings by the names the generator
  # gives them.
  target_include_directories(av_icd PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan)
  target_compile_options(av_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_icd PUBLIC ${AV_TEST_FLAGS})
endif()

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
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/${name}.profraw")
  # Two tests name the generated clients' headers by the names the generator
  # includes them by; the clients themselves are in av_core.
  # The full-stack rig's rvk run links the executor library's own entry point:
  # its device core runs the table rvk_executor_ops() answers, in process, on
  # this machine's GPU (feature 044, T028).
  if(name STREQUAL "test_fullstack_rvk")
    target_sources(${name} PRIVATE ${AV_EXECUTOR_ENTRY})
  endif()
  # The ICD's tests link its core, which no other test may: its hooks replace
  # the generated defaults every other binary relies on. The probe's loader
  # seam is tested through the ICD's own entry point, so it links it too.
  if(src MATCHES "/tests/unit/icd/" OR name STREQUAL "test_loader_vulkan")
    target_link_libraries(${name} PRIVATE av_icd)
  endif()
  # The probe through the ICD on this machine's GPU (feature 045, T038): the
  # ICD's hooks and the executor's cannot share a binary, so the executor is
  # the library file QEMU loads, its symbols its own.
  if(name STREQUAL "test_fullstack_vulkan_icd")
    target_link_libraries(${name} PRIVATE av_icd ${CMAKE_DL_LIBS})
    add_dependencies(${name} rvk_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:rvk_executor>")
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
if(AV_COVERAGE)
  add_test(NAME coverage_gate
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_coverage.py ${CMAKE_BINARY_DIR})
  set_tests_properties(coverage_gate PROPERTIES FIXTURES_REQUIRED coverage_profiles)
endif()

# ---- the driver: the same core, compiled for the Windows kernel ------------
include(ExternalProject)
# A toolchain's flags reach a build tree only through its first configure: CMake
# caches the *_INIT variables. Each external project therefore configures again,
# from an empty cache, whenever its toolchain or flags file changes; otherwise a
# changed flag silently never reaches an existing tree.
ExternalProject_Add(rvkgpu
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/km
  BINARY_DIR ${CMAKE_BINARY_DIR}/driver
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys)
ExternalProject_Add_StepDependencies(rvkgpu configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_kernel_flags.cmake)
add_test(NAME driver_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_driver_image.py
                 ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys
                 # displib's DxgkInitialize is linked, not just declared
                 --require-import IoGetDeviceObjectPointer)

# ---- the in-guest suite: src/guest and src/um, for Windows user mode ---------
ExternalProject_Add(rvktest
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/um
  BINARY_DIR ${CMAKE_BINARY_DIR}/guest
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/guest/rvktest.exe ${CMAKE_BINARY_DIR}/guest/rvkvk.dll
                   ${CMAKE_BINARY_DIR}/guest/rvkvkprobe.exe ${CMAKE_BINARY_DIR}/guest/rvkinspect.exe)
ExternalProject_Add_StepDependencies(rvktest configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags.cmake)
add_test(NAME guest_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/rvktest.exe
                 --require-import D3DKMTSubmitCommand)

# The probe binds to the Khronos loader and nothing else beyond the three
# (feature 045).
add_test(NAME rvkvkprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/rvkvkprobe.exe
                 --allow-import vulkan-1.dll --require-import vkGetInstanceProcAddr)

# The trace reader needs nothing past the three either (docs/trace.md).
add_test(NAME rvkinspect_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/rvkinspect.exe
                 --require-import D3DKMTEscape --require-import OpenFileMappingW)

# The Vulkan ICD exports exactly what the loader calls (feature 045).
add_test(NAME rvkvk_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/rvkvk.dll --dll --only-exports
                 --require-export vk_icdNegotiateLoaderICDInterfaceVersion
                 --require-export vk_icdGetInstanceProcAddr)

# ---- gate scripts are tested like any other code ---------------------------
add_test(NAME tools_tests
         COMMAND python3 -B -m unittest discover
                 -s ${CMAKE_SOURCE_DIR}/tests/tools -p "test_*.py")

# ---- the committed generated sources are what the generators write --------
add_test(NAME generated_sources
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_generated.py)

# ---- the committed SPIR-V is what its GLSL compiles to (045) ----------------
add_test(NAME check_spirv
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_spirv.py --check)

# ---- the documented id space and the generator's id files agree (FR-007) ----
add_test(NAME protocol_ids
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_protocol_ids.py)

# ---- constitution VI for the Vulkan ICD: every reported extension is tested (045)
add_test(NAME extension_policy
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_extension_policy.py ${CMAKE_SOURCE_DIR})

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
                 $<TARGET_FILE:rvk_executor> $<TARGET_FILE:rvk_echo_executor>)

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
       name STREQUAL "fuzz_memory_type_table")
      target_link_libraries(${name} PRIVATE av_icd)
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
  endforeach()
endif()
