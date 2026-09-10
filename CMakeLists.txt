cmake_minimum_required(VERSION 3.24)
project(accel_virt C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "clang is required (constitution: Technology constraints)")
endif()

option(AV_COVERAGE "instrument for llvm-cov"        OFF)
option(AV_SANITIZE "build tests with ASan + UBSan"  ON)
option(AV_FUZZ     "build libFuzzer targets"        OFF)

add_compile_options(
  -Wall -Wextra -Werror
  -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -Wcast-qual -Wpointer-arith -Wvla
  -Wno-unused-command-line-argument)

# ---- portable core: no platform headers, compiled into every target --------
add_library(av_core STATIC src/core/gpuva.c)
target_include_directories(av_core PUBLIC src/core)
target_compile_definitions(av_core PUBLIC AV_CHECKED=1)

# ---- test harness ----------------------------------------------------------
add_library(av_harness STATIC
  tests/harness/av_test.c
  tests/harness/platform_mock.c
  tests/harness/av_hostile.c)
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
file(GLOB AV_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/test_*.c)
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
add_test(NAME symbol_test_coverage
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/check_symbol_tests.py
                 ${CMAKE_SOURCE_DIR})

# ---- fuzz targets ----------------------------------------------------------
if(AV_FUZZ)
  file(GLOB AV_FUZZ_SRCS CONFIGURE_DEPENDS tests/fuzz/fuzz_*.c)
  foreach(src ${AV_FUZZ_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src}
      tests/harness/platform_mock.c src/core/gpuva.c)
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
