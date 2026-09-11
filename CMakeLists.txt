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
    "Microsoft.Windows.WDK.x64 10.0.26100.6584\nMicrosoft.Windows.SDK.CPP 10.0.26100.9169")
foreach(required VERSION overlay.yaml include/km/ntddk.h lib/x64/ntoskrnl.lib)
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
file(GLOB AV_PORTABLE_SOURCES CONFIGURE_DEPENDS
  src/core/*.cpp src/core/ddi/*.cpp src/proto/*.cpp src/device/*.cpp)
add_library(av_core STATIC ${AV_PORTABLE_SOURCES})
target_include_directories(av_core PUBLIC src/core src/proto src/device)
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# ---- test harness ----------------------------------------------------------
add_library(av_harness STATIC
  tests/harness/test_registry.cpp
  tests/harness/mock_platform.cpp
  tests/harness/guarded_buffer.cpp
  tests/harness/hostile_sweep.cpp
  tests/harness/address_space_fixture.cpp
  tests/harness/ddi_table_probe.cpp
  tests/harness/fake_dxgkrnl.cpp
  tests/harness/byte_layout.cpp
  tests/harness/fixture_file.cpp
  tests/harness/fake_common_config.cpp
  tests/harness/fake_register_bytes.cpp
  tests/harness/fake_isr_register.cpp
  tests/harness/fake_virtqueue_device.cpp
  tests/harness/fake_resource_list.cpp
  tests/harness/fake_virtio_gpu_function.cpp
  tests/harness/fake_dxgkrnl_adapter.cpp
  tests/harness/fake_dxgkrnl_fdo.cpp
  tests/harness/fake_dxgadapter.cpp
  tests/harness/fake_vidmm.cpp
  tests/harness/fake_handle_table.cpp
  tests/harness/fake_vidsch.cpp
  tests/harness/fake_vidpn.cpp
  tests/harness/vidpn_fixture.cpp
  tests/harness/fake_display_mode_manager.cpp
  tests/harness/mapped_virtio_function.cpp
  tests/harness/fake_gpu_mmu.cpp)
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
  list(APPEND AV_TEST_FLAGS -fprofile-instr-generate -fcoverage-mapping)
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
file(GLOB AV_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/test_*.cpp)
foreach(src ${AV_UNIT_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} tests/harness/test_main.cpp)
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
ExternalProject_Add(rvkgpu
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/km
  BINARY_DIR ${CMAKE_BINARY_DIR}/driver
  CMAKE_ARGS -G Ninja
             -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
             -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR}
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys)
add_test(NAME driver_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_driver_image.py
                 ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys
                 # displib's DxgkInitialize is linked, not just declared
                 --require-import IoGetDeviceObjectPointer)

# ---- gate scripts are tested like any other code ---------------------------
add_test(NAME tools_tests
         COMMAND python3 -B -m unittest discover
                 -s ${CMAKE_SOURCE_DIR}/tests/tools -p "test_*.py")

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
