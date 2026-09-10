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

# ---- portable core: no platform headers, compiled into every target --------
add_library(av_core STATIC
  src/core/alignment.cpp
  src/core/byte_range.cpp
  src/core/gpu_virtual_address_space.cpp)
target_include_directories(av_core PUBLIC src/core)
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# ---- test harness ----------------------------------------------------------
add_library(av_harness STATIC
  tests/harness/test_registry.cpp
  tests/harness/mock_platform.cpp
  tests/harness/guarded_buffer.cpp
  tests/harness/hostile_sweep.cpp
  tests/harness/address_space_fixture.cpp)
target_link_libraries(av_harness PUBLIC av_core)

set(AV_TEST_FLAGS "")
if(AV_SANITIZE)
  list(APPEND AV_TEST_FLAGS -fsanitize=address,undefined
                            -fno-omit-frame-pointer
                            -fno-sanitize-recover=all)
endif()
if(AV_COVERAGE)
  list(APPEND AV_TEST_FLAGS -fprofile-instr-generate -fcoverage-mapping)
endif()

foreach(t av_core av_harness)
  target_compile_options(${t} PRIVATE ${AV_TEST_FLAGS})
  target_link_options(${t} PUBLIC ${AV_TEST_FLAGS})
endforeach()

# ---- unit tests: one binary per module, so a crash isolates -----------------
enable_testing()
file(GLOB AV_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/test_*.cpp)
foreach(src ${AV_UNIT_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE av_harness)
  target_compile_options(${name} PRIVATE ${AV_TEST_FLAGS})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/${name}.profraw")
endforeach()

# ---- the rule with teeth: no exported core symbol without a test ------------
add_test(NAME design_rules
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/check_design.py
                 ${CMAKE_SOURCE_DIR})

# ---- fuzz targets ----------------------------------------------------------
if(AV_FUZZ)
  file(GLOB AV_FUZZ_SRCS CONFIGURE_DEPENDS tests/fuzz/fuzz_*.cpp)
  foreach(src ${AV_FUZZ_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src}
      tests/harness/mock_platform.cpp
      tests/harness/test_registry.cpp
      src/core/alignment.cpp
      src/core/byte_range.cpp
      src/core/gpu_virtual_address_space.cpp)
    target_include_directories(${name} PRIVATE src/core)
    target_compile_definitions(${name} PRIVATE AV_CHECKED=1)
    target_compile_options(${name} PRIVATE
      -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(${name} PRIVATE
      -fsanitize=fuzzer,address,undefined)
    # 60-second smoke run is a gate, not a report (constitution: Done #4)
    add_test(NAME ${name}_smoke COMMAND ${name}
             ${CMAKE_SOURCE_DIR}/tests/corpus/${name}
             -max_total_time=60 -print_final_stats=1)
  endforeach()
endif()
