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
  ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_command_names.cpp)
if(EXISTS ${AV_KHRONOS_INCLUDE}/vulkan/vulkan_core.h)
  set(AV_GENERATED ON)
  message(STATUS "generated sources: compiled against ${AV_KHRONOS_INCLUDE}")
else()
  set(AV_GENERATED OFF)
  message(STATUS "generated sources: skipped, no Khronos headers; "
                 "run tools/fetch_khronos_registries.py to compile them")
endif()
# Whatever the glob picked up from generated/, the list above is what builds.
file(GLOB_RECURSE AV_GENERATED_ALL CONFIGURE_DEPENDS src/proto/generated/*.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_GENERATED_ALL})
if(AV_GENERATED)
  list(APPEND AV_PORTABLE_SOURCES ${AV_GENERATED_SOURCES})
endif()
add_library(av_core STATIC ${AV_PORTABLE_SOURCES})
target_include_directories(av_core PUBLIC src)
if(AV_GENERATED)
  target_include_directories(av_core PUBLIC ${AV_KHRONOS_INCLUDE})
endif()
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# ---- the device core for QEMU: a PIC static library behind the C ABI -------
# No sanitizer or profile flags and hidden visibility: the archive exports only
# rvk_device.h, and tools/build_qemu.sh links it into the virtio-gpu-rvk module
# through the generated rvk_device.pc.
# TextBuffer is the one core/support class with a body the device core uses.
file(GLOB_RECURSE AV_DEVICE_SOURCES CONFIGURE_DEPENDS src/device/*.cpp src/proto/*.cpp)
list(APPEND AV_DEVICE_SOURCES src/core/support/text_buffer.cpp)
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_GENERATED_ALL})
if(AV_GENERATED)
  list(APPEND AV_DEVICE_SOURCES ${AV_GENERATED_SOURCES})
endif()
add_library(rvk_device STATIC ${AV_DEVICE_SOURCES})
target_include_directories(rvk_device PRIVATE src)
if(AV_GENERATED)
  target_include_directories(rvk_device PRIVATE ${AV_KHRONOS_INCLUDE})
endif()
target_compile_options(rvk_device PRIVATE -O2 -ffreestanding)
set_target_properties(rvk_device PROPERTIES
  POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
configure_file(cmake/rvk_device.pc.in ${CMAKE_BINARY_DIR}/rvk_device.pc.in @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/pkgconfig/rvk_device.pc INPUT ${CMAKE_BINARY_DIR}/rvk_device.pc.in)

# ---- the patched QEMU: its own configuration, since it needs nothing else ----
if(AV_QEMU)
  enable_testing()
  add_custom_target(qemu ALL
    COMMAND ${CMAKE_SOURCE_DIR}/tools/build_qemu.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/pkgconfig
    DEPENDS rvk_device
    USES_TERMINAL
    COMMENT "QEMU with virtio-gpu-rvk")
  # The executor the qtest suites name: the device's execution path without render-server.
  add_library(rvk_echo_executor SHARED tests/qemu/echo_executor.cpp)
  target_include_directories(rvk_echo_executor PRIVATE src/device/abi)
  add_test(NAME qemu_smoke
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tests/qemu/qemu_smoke.py ${CMAKE_BINARY_DIR}/qemu
                   $<TARGET_FILE:rvk_echo_executor>)
  # The driver's own request bytes, sent to the real device by tests/qemu.
  add_executable(gpu_request_bytes tests/qemu/gpu_request_bytes.cpp)
  target_link_libraries(gpu_request_bytes PRIVATE av_core)
  add_test(NAME qemu_virtio_gpu
           COMMAND python3 -B -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests/qemu -p "test_*.py")
  set_tests_properties(qemu_virtio_gpu PROPERTIES
    ENVIRONMENT "AV_QEMU_BUILD=${CMAKE_BINARY_DIR}/qemu;AV_GPU_REQUEST_BYTES=$<TARGET_FILE:gpu_request_bytes>;AV_EXECUTOR=$<TARGET_FILE:rvk_echo_executor>")
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
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/guest/rvktest.exe)
ExternalProject_Add_StepDependencies(rvktest configure
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
  ${CMAKE_SOURCE_DIR}/cmake/windows_user_flags.cmake)
add_test(NAME guest_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/rvktest.exe
                 --require-import D3DKMTSubmitCommand)

# ---- gate scripts are tested like any other code ---------------------------
add_test(NAME tools_tests
         COMMAND python3 -B -m unittest discover
                 -s ${CMAKE_SOURCE_DIR}/tests/tools -p "test_*.py")

# ---- the committed generated sources are what the generators write --------
add_test(NAME generated_sources
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_generated.py)

# ---- constitution III and IV.5 --------------------------------------------
add_test(NAME design_rules
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_design.py ${CMAKE_SOURCE_DIR})
add_test(NAME symbol_tests
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_symbol_tests.py ${CMAKE_SOURCE_DIR})

# ---- principle IX: a clean clone of this repository is all it takes ----------
add_test(NAME self_contained
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_self_contained.py ${CMAKE_SOURCE_DIR})

# ---- fuzz targets: a smoke run per target is a gate (constitution: Done #4) --
if(AV_FUZZ)
  if(NOT DEFINED ENV{AV_FUZZ_SECONDS})
    set(ENV{AV_FUZZ_SECONDS} 60)
  endif()
  file(GLOB AV_FUZZ_SOURCES CONFIGURE_DEPENDS tests/fuzz/fuzz_*.cpp)
  foreach(src ${AV_FUZZ_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE av_harness)
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
