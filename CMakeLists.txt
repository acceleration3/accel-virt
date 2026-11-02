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
option(AV_CHECKED "verify invariants around every mutation" ON)
option(AV_QEMU     "build the patched QEMU with the device and smoke-test it" OFF)
if(AV_QEMU AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(
  -Wall -Wextra -Werror
  -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  -Wcast-qual -Wpointer-arith -Wvla -Wnon-virtual-dtor -Woverloaded-virtual
  -Wno-unused-command-line-argument
  -fno-exceptions -fno-rtti)

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

file(GLOB_RECURSE AV_PORTABLE_SOURCES CONFIGURE_DEPENDS
  src/core/*.cpp src/miniport/*.cpp src/proto/*.cpp src/device/*.cpp src/executor/*.cpp src/guest/*.cpp)

set(AV_KHRONOS_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/khronos/include)
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
  ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d/d3d_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d12/d3d12_dispatch_server.cpp
  ${CMAKE_SOURCE_DIR}/src/proto/generated/d3d9/d3d9_dispatch_server.cpp)

set(AV_GENERATED_OPENGL_INCLUDE ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl)
if(EXISTS ${AV_KHRONOS_INCLUDE}/vulkan/vulkan_core.h)
  set(AV_GENERATED ON)
  message(STATUS "generated sources: compiled against ${AV_KHRONOS_INCLUDE}")
else()
  set(AV_GENERATED OFF)
  message(STATUS "generated sources: skipped, no Khronos headers; "
                 "run tools/fetch_khronos_registries.py to compile them")
endif()
set(AV_EXECUTOR_ENTRY ${CMAKE_SOURCE_DIR}/src/executor/shim/executor_ops.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_EXECUTOR_ENTRY})
set(AV_RECORD_ENTRY ${CMAKE_SOURCE_DIR}/src/device/shim/recording_tool.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_RECORD_ENTRY})
set(AV_RENDERER_ENTRY ${CMAKE_SOURCE_DIR}/src/device/shim/renderer_main.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_RENDERER_ENTRY})

file(GLOB_RECURSE AV_GENERATED_ALL CONFIGURE_DEPENDS src/proto/generated/*.cpp)
list(REMOVE_ITEM AV_PORTABLE_SOURCES ${AV_GENERATED_ALL})
list(APPEND AV_PORTABLE_SOURCES ${AV_GENERATED_NAMES})
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
set_target_properties(av_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_library(AV_VULKAN_LIBRARY NAMES vulkan)
if(AV_VULKAN_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_VULKAN_LIBRARY})
endif()
find_library(AV_OPENCL_LIBRARY NAMES OpenCL)
if(AV_OPENCL_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_OPENCL_LIBRARY})
endif()
find_library(AV_EGL_LIBRARY NAMES EGL)
if(AV_EGL_LIBRARY)
  target_link_libraries(av_core PUBLIC ${AV_EGL_LIBRARY})
endif()
if(AV_GENERATED)
  target_include_directories(av_core PUBLIC ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
endif()
set(AV_DXVK_INCLUDE ${CMAKE_SOURCE_DIR}/third_party/dxvk/include/native/directx
                    ${CMAKE_SOURCE_DIR}/third_party/dxvk/include/native/windows)
target_include_directories(av_core SYSTEM PUBLIC ${AV_DXVK_INCLUDE})
if(AV_CHECKED)
  target_compile_definitions(av_core PUBLIC AV_CHECKED=1)
endif()

file(GLOB AV_D3D11_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/d3d11/*.cpp)
list(FILTER AV_D3D11_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
add_executable(avd3d11samples_host ${AV_D3D11_SAMPLES_SOURCES})
target_include_directories(avd3d11samples_host PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(avd3d11samples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avd3d11samples_host PRIVATE ${CMAKE_DL_LIBS})

file(GLOB AV_OPENCL_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/opencl/*.cpp)
list(FILTER AV_OPENCL_SAMPLES_SOURCES EXCLUDE REGEX "(guest|loopback)_platform\\.cpp$")
add_executable(avclsamples_host ${AV_OPENCL_SAMPLES_SOURCES})
target_include_directories(avclsamples_host SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/khronos/include)
target_compile_options(avclsamples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avclsamples_host PRIVATE ${CMAKE_DL_LIBS})

find_package(Threads REQUIRED)
if(AV_EGL_LIBRARY)
  file(GLOB AV_OPENGL_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/opengl/*.cpp)
  list(FILTER AV_OPENGL_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
  add_executable(avglsamples_host ${AV_OPENGL_SAMPLES_SOURCES})
  target_compile_options(avglsamples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
  target_link_libraries(avglsamples_host PRIVATE ${AV_EGL_LIBRARY} Threads::Threads)
endif()

file(GLOB AV_D3D12_SAMPLES_SOURCES CONFIGURE_DEPENDS samples/d3d12/*.cpp)
list(FILTER AV_D3D12_SAMPLES_SOURCES EXCLUDE REGEX "guest_platform\\.cpp$")
add_executable(avd3d12samples_host ${AV_D3D12_SAMPLES_SOURCES})
target_include_directories(avd3d12samples_host PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(avd3d12samples_host PRIVATE -O2 -w -fno-exceptions -fno-rtti)
target_link_libraries(avd3d12samples_host PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)


file(GLOB_RECURSE AV_DEVICE_SOURCES CONFIGURE_DEPENDS src/device/*.cpp src/proto/*.cpp src/core/trace/*.cpp)
list(APPEND AV_DEVICE_SOURCES src/core/platform/lock_marks.cpp src/core/platform/thread_lock_hold.cpp)
list(REMOVE_ITEM AV_DEVICE_SOURCES ${AV_RECORD_ENTRY})
list(APPEND AV_DEVICE_SOURCES src/core/support/text_buffer.cpp src/core/support/byte_range.cpp
  src/core/support/bgra_rows.cpp
  src/core/support/setting_text.cpp src/core/support/fnv1a_digest.cpp
  src/core/log/driver_log.cpp src/core/log/log_line.cpp)
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
target_include_directories(av_device SYSTEM PRIVATE ${AV_DXVK_INCLUDE})
target_compile_options(av_device PRIVATE -O2 -ffreestanding -fasynchronous-unwind-tables)
set_target_properties(av_device PROPERTIES
  POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
configure_file(cmake/av_device.pc.in ${CMAKE_BINARY_DIR}/av_device.pc.in @ONLY)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/pkgconfig/av_device.pc INPUT ${CMAKE_BINARY_DIR}/av_device.pc.in)

add_library(av_executor SHARED ${AV_EXECUTOR_ENTRY})
if(NOT AV_VULKAN_LIBRARY)
  message(FATAL_ERROR "libvulkan not found; the executor library runs commands on it")
endif()
target_link_libraries(av_executor PRIVATE av_core)
target_include_directories(av_executor PRIVATE src)
set_target_properties(av_executor PROPERTIES CXX_VISIBILITY_PRESET hidden
                                              VISIBILITY_INLINES_HIDDEN ON)
target_link_options(av_executor PRIVATE -Wl,--exclude-libs,ALL)
if(AV_QEMU)
  set_target_properties(av_core av_executor PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  target_link_options(av_core INTERFACE -fuse-ld=lld)
endif()

add_executable(avrecord ${AV_RECORD_ENTRY})
target_link_libraries(avrecord PRIVATE av_core ${CMAKE_DL_LIBS})
target_include_directories(avrecord PRIVATE src)

add_executable(av-renderer ${AV_RENDERER_ENTRY})
target_link_libraries(av-renderer PRIVATE av_core ${CMAKE_DL_LIBS} pthread)
target_include_directories(av-renderer PRIVATE src)

add_library(av_echo_executor SHARED tests/qemu/echo_executor.cpp)
target_include_directories(av_echo_executor PRIVATE src/device/abi)
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
  add_executable(gpu_request_bytes tests/qemu/gpu_request_bytes.cpp)
  target_link_libraries(gpu_request_bytes PRIVATE av_core)
  add_test(NAME qemu_virtio_gpu
           COMMAND python3 -B -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests/qemu -p "test_*.py")
  set_tests_properties(qemu_virtio_gpu PROPERTIES
    ENVIRONMENT "AV_QEMU_BUILD=${CMAKE_BINARY_DIR}/qemu;AV_GPU_REQUEST_BYTES=$<TARGET_FILE:gpu_request_bytes>;AV_EXECUTOR=$<TARGET_FILE:av_echo_executor>;AV_RENDERER=$<TARGET_FILE:av-renderer>;AV_REAL_EXECUTOR=$<TARGET_FILE:av_executor>")
  return()
endif()

file(GLOB_RECURSE AV_HARNESS_SOURCES CONFIGURE_DEPENDS tests/harness/*.cpp)
list(FILTER AV_HARNESS_SOURCES EXCLUDE REGEX "/test_main\\.cpp$")
if(AV_LOCAL_MINIPORT_ONLY)
  list(FILTER AV_HARNESS_SOURCES EXCLUDE REGEX "/harness/(executor|icd)/|/harness/transport/mock_transport\\.cpp$|/harness/guest/scripted_host\\.cpp$")
endif()
add_library(av_harness STATIC ${AV_HARNESS_SOURCES})
target_include_directories(av_harness PUBLIC tests)
find_package(Threads REQUIRED)
target_link_libraries(av_harness PUBLIC Threads::Threads)
target_link_libraries(av_harness PUBLIC av_core)
target_compile_definitions(av_harness PUBLIC AV_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

add_library(av_device_loss_layer SHARED tests/gpu_fault/device_loss_layer.cpp)
set_target_properties(av_device_loss_layer PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/gpu_fault CXX_VISIBILITY_PRESET hidden)
set(AV_LOSS_LAYER_LIBRARY ${CMAKE_BINARY_DIR}/gpu_fault/libav_device_loss_layer.so)
configure_file(tests/gpu_fault/VkLayer_av_device_loss.json.in ${CMAKE_BINARY_DIR}/gpu_fault/VkLayer_av_device_loss.json
               @ONLY)
set(AV_LOSS_LAYER_ENVIRONMENT
  "VK_ADD_LAYER_PATH=${CMAKE_BINARY_DIR}/gpu_fault;VK_INSTANCE_LAYERS=VK_LAYER_AV_device_loss")

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
  list(APPEND AV_TEST_FLAGS -fprofile-instr-generate -fcoverage-mapping -fprofile-update=atomic)
endif()
if(AV_FUZZ)
  list(APPEND AV_TEST_FLAGS -fsanitize=fuzzer-no-link)
endif()

foreach(t av_core av_harness)
  target_compile_options(${t} PRIVATE ${AV_TEST_FLAGS})
  target_link_options(${t} PUBLIC ${AV_TEST_FLAGS})
endforeach()

if(AV_GENERATED)
  file(GLOB_RECURSE AV_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/vulkan/*.cpp)
  add_library(av_icd STATIC ${AV_ICD_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan/vk_icd_entries.cpp)
  target_link_libraries(av_icd PUBLIC av_core)
  target_include_directories(av_icd PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/vulkan)
  target_compile_options(av_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_icd PUBLIC ${AV_TEST_FLAGS})

  add_library(av_gl_generated OBJECT
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_dispatch_client.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl/gl_icd_entries.cpp)
  target_link_libraries(av_gl_generated PRIVATE av_core)
  target_include_directories(av_gl_generated PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl
    ${CMAKE_SOURCE_DIR}/src/icd/opengl)
  target_compile_options(av_gl_generated PRIVATE ${AV_TEST_FLAGS})

  file(GLOB_RECURSE AV_GL_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/opengl/*.cpp)
  add_library(av_gl_icd STATIC ${AV_GL_ICD_SOURCES} $<TARGET_OBJECTS:av_gl_generated>)
  target_link_libraries(av_gl_icd PUBLIC av_core)
  target_include_directories(av_gl_icd PUBLIC ${CMAKE_SOURCE_DIR}/src/proto/generated/opengl
    ${CMAKE_SOURCE_DIR}/src/icd/opengl)
  target_compile_options(av_gl_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_gl_icd PUBLIC ${AV_TEST_FLAGS})

  add_library(av_cl_generated OBJECT
    ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl/cl_dispatch_client.cpp)
  target_link_libraries(av_cl_generated PRIVATE av_core)
  target_include_directories(av_cl_generated PRIVATE ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl
    ${CMAKE_SOURCE_DIR}/src/icd/opencl)
  target_compile_options(av_cl_generated PRIVATE ${AV_TEST_FLAGS})

  file(GLOB_RECURSE AV_CL_ICD_SOURCES CONFIGURE_DEPENDS src/icd/common/*.cpp src/icd/opencl/*.cpp)
  add_library(av_cl_icd STATIC ${AV_CL_ICD_SOURCES} $<TARGET_OBJECTS:av_cl_generated>)
  target_link_libraries(av_cl_icd PUBLIC av_core)
  target_include_directories(av_cl_icd PUBLIC ${CMAKE_SOURCE_DIR}/src/proto/generated/opencl
    ${CMAKE_SOURCE_DIR}/src/icd/opencl)
  target_compile_options(av_cl_icd PRIVATE ${AV_TEST_FLAGS})
  target_link_options(av_cl_icd PUBLIC ${AV_TEST_FLAGS})
endif()

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

enable_testing()
if(AV_COVERAGE)
  add_test(NAME coverage_clean
           COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/coverage)
  set_tests_properties(coverage_clean PROPERTIES FIXTURES_SETUP coverage_clean)
endif()
file(GLOB_RECURSE AV_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/test_*.cpp tests/loopback/test_*.cpp)
foreach(src ${AV_UNIT_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} tests/harness/framework/test_main.cpp)
  target_link_libraries(${name} PRIVATE av_harness)
  target_compile_options(${name} PRIVATE ${AV_TEST_FLAGS})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/${name}.profraw;MESA_SHADER_CACHE_DISABLE=true")
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
  if(name STREQUAL "test_d3d_device_loss_device" OR name STREQUAL "test_fullstack_device_loss")
    add_dependencies(${name} av_device_loss_layer)
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT ${AV_LOSS_LAYER_ENVIRONMENT})
  endif()
  if(name STREQUAL "test_d3d_backend_device")
    target_sources(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/icd/d3d/passthrough_geometry_shader.cpp)
  endif()
  if(name STREQUAL "test_posix_renderer_host" OR name STREQUAL "test_renderer_process")
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_echo_executor av-renderer)
    target_compile_definitions(${name} PRIVATE AV_ECHO_EXECUTOR="$<TARGET_FILE:av_echo_executor>"
                                               AV_RENDERER_BINARY="$<TARGET_FILE:av-renderer>")
  endif()
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
  if(name STREQUAL "test_fullstack_opengl_icd")
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    target_compile_definitions(${name} PRIVATE AV_IMAGE_DIR="${CMAKE_BINARY_DIR}/images")
  endif()
  if(name STREQUAL "test_fullstack_opencl_icd")
    find_package(Threads REQUIRED)
    target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
  if(name STREQUAL "test_fullstack_templates")
    target_link_libraries(${name} PRIVATE av_icd ${CMAKE_DL_LIBS})
    add_dependencies(${name} av_executor)
    target_compile_definitions(${name} PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1)
  endif()
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
    target_compile_definitions(${name} PRIVATE AV_IMAGE_DIR="${CMAKE_BINARY_DIR}/images")
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

if(AV_GENERATED)
  file(GLOB AV_OPENCL_LOOPBACK_SOURCES CONFIGURE_DEPENDS samples/opencl/*.cpp)
  list(FILTER AV_OPENCL_LOOPBACK_SOURCES EXCLUDE REGEX "(guest|host)_platform\\.cpp$")
  add_executable(avclsamples_loopback ${AV_OPENCL_LOOPBACK_SOURCES})
  target_compile_options(avclsamples_loopback PRIVATE -O2 -w)
  target_link_libraries(avclsamples_loopback PRIVATE av_harness av_cl_icd ${CMAKE_DL_LIBS})
  add_dependencies(avclsamples_loopback av_executor)
  target_compile_definitions(avclsamples_loopback PRIVATE AV_EXECUTOR_LIBRARY="$<TARGET_FILE:av_executor>")
endif()

include(ExternalProject)
ExternalProject_Add(avgpu
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/km
  BINARY_DIR ${CMAKE_BINARY_DIR}/driver
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_kernel_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR} -DAV_CHECKED=${AV_CHECKED}
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
                 --require-import IoGetDeviceObjectPointer)

ExternalProject_Add(avtest
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/um
  BINARY_DIR ${CMAKE_BINARY_DIR}/guest
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR} -DAV_CHECKED=${AV_CHECKED}
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

ExternalProject_Add(avtest32
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/um
  BINARY_DIR ${CMAKE_BINARY_DIR}/guest32
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E rm -f <BINARY_DIR>/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -G Ninja -S <SOURCE_DIR> -B <BINARY_DIR>
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/windows_user_toolchain.cmake
          -DAV_WINDOWS_KIT_DIR=${AV_WINDOWS_KIT_DIR} -DAV_GUEST_ARCH=x86 -DAV_CHECKED=${AV_CHECKED}
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

add_test(NAME avvkprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avvkprobe.exe
                 --allow-import vulkan-1.dll --allow-import user32.dll --allow-import OpenCL.dll
                 --require-import vkGetInstanceProcAddr --require-import clCreateBufferWithProperties)

add_test(NAME avinspect_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avinspect.exe
                 --require-import D3DKMTEscape --require-import OpenFileMappingW)

add_test(NAME avvk_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avvk.dll --dll --only-exports --allow-import user32.dll
                 --require-export vk_icdNegotiateLoaderICDInterfaceVersion
                 --require-export vk_icdGetInstanceProcAddr)

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

add_test(NAME avumd_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avumd.dll --dll --only-exports
                 --require-export OpenAdapter10_2 --require-export OpenAdapter12
                 --require-export OpenAdapter --require-export AvAskSharedRows
                 --require-export AvWithdrawSharedRows)

add_test(NAME avcl_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avcl.dll --dll --only-exports
                 --require-export clGetExtensionFunctionAddress
                 --require-export clIcdGetPlatformIDsKHR)

add_test(NAME avclprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avclprobe.exe
                 --allow-import OpenCL.dll --require-import clGetPlatformIDs)

add_test(NAME avclsamples_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avclsamples.exe --require-import LoadLibraryA)

if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/opencl-loader/loader/icd.c)
  add_test(NAME opencl_loader_image
           COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                   ${CMAKE_BINARY_DIR}/guest/OpenCL.dll --dll
                   --allow-import cfgmgr32.dll --allow-import advapi32.dll --allow-import ole32.dll
                   --require-export clGetPlatformIDs --require-import CM_Get_Device_ID_ListW)
endif()

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

add_test(NAME avglprobe_image
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_guest_image.py
                 ${CMAKE_BINARY_DIR}/guest/avglprobe.exe
                 --allow-import opengl32.dll --allow-import user32.dll
                 --require-import wglCreateContext --require-import D3DKMTOpenAdapterFromHdc)

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

if(NOT AV_COVERAGE AND NOT AV_FUZZ)
  set(AV_TOOLS_SHARDED test_run_guest_suite)
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

add_test(NAME generated_sources
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_generated.py)
find_program(AV_CLANG_CL clang-cl)
if(AV_CLANG_CL AND EXISTS ${AV_WINDOWS_KIT_DIR}/include/um/d3d12umddi.h)
  add_test(NAME generated_d3d11_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d11_ddi.py --check)
  add_test(NAME generated_d3d12_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d12_ddi.py --check)
  add_test(NAME generated_d3d9_ddi COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_d3d9_ddi.py --check)
endif()

add_test(NAME check_spirv
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/gen_spirv.py --check)

add_test(NAME extension_policy
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_extension_policy.py ${CMAKE_SOURCE_DIR})

add_test(NAME layer_requirements
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_layer_requirements.py ${CMAKE_SOURCE_DIR})

add_test(NAME cts_subset
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_cts_subset.py ${CMAKE_SOURCE_DIR})

add_test(NAME design_rules
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_design.py ${CMAKE_SOURCE_DIR})
add_test(NAME symbol_tests
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_symbol_tests.py ${CMAKE_SOURCE_DIR})
add_test(NAME scope_marks
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/scope_marks.py --check ${CMAKE_SOURCE_DIR})

add_executable(executor_conformance
  tests/conformance/executor/executor_conformance.cpp
  tests/conformance/executor/executor_transport.cpp
  tests/conformance/executor/main.cpp)
target_include_directories(executor_conformance PRIVATE tests src
                           ${AV_KHRONOS_INCLUDE} ${AV_GENERATED_OPENGL_INCLUDE})
target_link_libraries(executor_conformance PRIVATE av_core)
target_link_libraries(executor_conformance PRIVATE ${CMAKE_DL_LIBS})
target_compile_options(executor_conformance PRIVATE ${AV_TEST_FLAGS})
target_link_options(executor_conformance PRIVATE ${AV_TEST_FLAGS})
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

add_test(NAME self_contained
         COMMAND python3 -B ${CMAKE_SOURCE_DIR}/tools/check_self_contained.py ${CMAKE_SOURCE_DIR})

if(AV_FUZZ)
  if(NOT DEFINED ENV{AV_FUZZ_SECONDS})
    set(ENV{AV_FUZZ_SECONDS} 60)
  endif()
  file(GLOB AV_FUZZ_SOURCES CONFIGURE_DEPENDS tests/fuzz/fuzz_*.cpp)
  if(NOT AV_GENERATED)
    list(REMOVE_ITEM AV_FUZZ_SOURCES ${CMAKE_SOURCE_DIR}/tests/fuzz/fuzz_generated_decode.cpp)
  endif()
  foreach(src ${AV_FUZZ_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE av_harness)
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
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/${name})
    add_test(NAME ${name}_smoke
             COMMAND ${name} ${CMAKE_BINARY_DIR}/corpus/${name}
                     ${CMAKE_SOURCE_DIR}/tests/corpus/${name}
                     -max_total_time=$ENV{AV_FUZZ_SECONDS} -print_final_stats=1
                     -artifact_prefix=${CMAKE_BINARY_DIR}/corpus/${name}-)
    set_tests_properties(${name}_smoke PROPERTIES LABELS fuzz)
    set_tests_properties(${name}_smoke PROPERTIES
      ENVIRONMENT "MESA_SHADER_CACHE_DISABLE=true;ASAN_OPTIONS=alloc_dealloc_mismatch=0")
  endforeach()
endif()
