cmake_minimum_required(VERSION 3.20)
project(avvksamples CXX C)

set(VKS_TREE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vulkan-samples" CACHE PATH "tools/fetch_vulkan_samples.sh's tree")
set(VKS_UPSTREAM "${VKS_TREE}/Vulkan-Samples")
if(NOT EXISTS "${VKS_UPSTREAM}/base/vulkanexamplebase.cpp")
  message(FATAL_ERROR "no Vulkan samples at ${VKS_UPSTREAM}: run tools/fetch_vulkan_samples.sh")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory("${VKS_TREE}/assimp" assimp EXCLUDE_FROM_ALL)

set(VKS_SAMPLES
  triangle pushconstants gears texture
  geometryshader tessellation displacement instancing mesh pipelines multisampling
  shadowmapping shadowmappingomni deferred parallaxmapping sphericalenvmapping skeletalanimation vulkanscene
  texturearray texturecubemap distancefieldfonts textoverlay debugmarker occlusionquery multithreading
  computeparticles computeshader particlefire raytracing bloom radialblur offscreen)

set(VKS_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VKS_WRAPPERS "")
set(VKS_DECLARATIONS "")
set(VKS_ROWS "")
foreach(SAMPLE ${VKS_SAMPLES})
  set(EXTRA_SOURCES "")
  if(SAMPLE STREQUAL "gears")
    set(EXTRA_SOURCES "#include \"gears/vulkangear.cpp\"")
  endif()
  configure_file(sample.cpp.in "${VKS_GENERATED}/sample_${SAMPLE}.cpp" @ONLY)
  list(APPEND VKS_WRAPPERS "${VKS_GENERATED}/sample_${SAMPLE}.cpp")
  string(APPEND VKS_DECLARATIONS "VulkanExampleBase *vks_make_${SAMPLE}();\nvoid vks_destroy_${SAMPLE}(VulkanExampleBase *);\n")
  string(APPEND VKS_ROWS "\t{\"${SAMPLE}\", vks_make_${SAMPLE}, vks_destroy_${SAMPLE}},\n")
endforeach()
configure_file(registry.cpp.in "${VKS_GENERATED}/registry.cpp" @ONLY)

add_executable(avvksamples
  runner.cpp
  "${VKS_GENERATED}/registry.cpp"
  ${VKS_WRAPPERS}
  "${VKS_UPSTREAM}/base/vulkanexamplebase.cpp"
  "${VKS_UPSTREAM}/base/vulkantools.cpp"
  "${VKS_UPSTREAM}/base/vulkandebug.cpp")
target_include_directories(avvksamples PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${VKS_UPSTREAM}"
  "${VKS_UPSTREAM}/base"
  "${VKS_UPSTREAM}/external"
  "${VKS_UPSTREAM}/external/glm"
  "${VKS_UPSTREAM}/external/gli")
target_compile_definitions(avvksamples PRIVATE VK_EXAMPLE_HEADLESS NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
target_compile_options(avvksamples PRIVATE -O2 -w -fpermissive)
target_link_libraries(avvksamples PRIVATE assimp)

if(WIN32)
  set(VKS_VULKAN_DLL "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vulkan-runtime/vulkan-1.dll" CACHE FILEPATH "")
  target_compile_definitions(avvksamples PRIVATE VK_USE_PLATFORM_WIN32_KHR)
  target_link_options(avvksamples PRIVATE -static -static-libgcc -static-libstdc++ -mconsole -s)
  target_link_libraries(avvksamples PRIVATE "${VKS_VULKAN_DLL}")
  set_target_properties(avvksamples PROPERTIES SUFFIX ".exe")
else()
  find_package(Threads REQUIRED)
  find_library(VKS_XCB xcb REQUIRED)
  target_compile_definitions(avvksamples PRIVATE VK_USE_PLATFORM_XCB_KHR)
  target_link_libraries(avvksamples PRIVATE vulkan ${VKS_XCB} Threads::Threads)
endif()
