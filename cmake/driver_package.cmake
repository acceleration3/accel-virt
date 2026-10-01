# Run by `cmake --install` for the drivers component, after the files are in
# place: the two files provision.ps1 reads beside the package (README,
# "Installing"). AV_SOURCE_DIR is set by the install rule before this runs.
#
#   package/package.sha256  each package file's SHA-256 in sha256sum's form,
#                           which the sign stage refuses a package without
#                           matching, and the install stage checks the driver
#                           store's copies against
#   adapters.txt            what this host's GPUs are called, which the sign
#                           stage names each GPU's INF model with; left out,
#                           with a warning, on a host whose vulkaninfo fails,
#                           and the models take the INF's static name
set(root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
set(manifest "")
file(GLOB package_files RELATIVE "${root}/package" "${root}/package/*")
list(SORT package_files)
foreach(name IN LISTS package_files)
  if(name STREQUAL "package.sha256")
    continue()
  endif()
  file(SHA256 "${root}/package/${name}" digest)
  string(APPEND manifest "${digest}  ${name}\n")
endforeach()
file(WRITE "${root}/package/package.sha256" "${manifest}")
message(STATUS "Installing: ${root}/package/package.sha256")

execute_process(COMMAND python3 -B "${AV_SOURCE_DIR}/tools/host/list_gpus.py" --adapter-names
                OUTPUT_VARIABLE names ERROR_VARIABLE refused RESULT_VARIABLE answered)
if(answered EQUAL 0)
  file(WRITE "${root}/adapters.txt" "${names}")
  message(STATUS "Installing: ${root}/adapters.txt")
else()
  file(REMOVE "${root}/adapters.txt")
  message(WARNING "adapters.txt left out: tools/host/list_gpus.py --adapter-names failed: ${refused}")
endif()
