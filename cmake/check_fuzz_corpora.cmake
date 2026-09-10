# Constitution IV.3: every fuzz target has a committed, non-empty seed corpus.
# Included by CMakeLists.txt in every configuration, and runnable on its own:
#   cmake -DAV_SOURCE_DIR=<repository root> -P cmake/check_fuzz_corpora.cmake
file(GLOB fuzz_targets ${AV_SOURCE_DIR}/tests/fuzz/fuzz_*.cpp)
set(without_corpus "")
foreach(target_source ${fuzz_targets})
  get_filename_component(target ${target_source} NAME_WE)
  file(GLOB seeds ${AV_SOURCE_DIR}/tests/corpus/${target}/*)
  if(NOT seeds)
    list(APPEND without_corpus ${target})
  endif()
endforeach()
if(without_corpus)
  list(JOIN without_corpus ", " names)
  message(FATAL_ERROR "fuzz targets with no seeds in tests/corpus/<target>/: ${names}")
endif()
