# Replaces every flag the MSVC platform module chose. Loaded through
# CMAKE_USER_MAKE_RULES_OVERRIDE_CXX by windows_user_toolchain.cmake.
if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

# As windows_kernel_flags.cmake, without /kernel: /X keeps only the kit
# visible, the overlay makes it case-insensitive. /GS- and /Gs: no C runtime
# seeds a cookie or provides __chkstk, so neither may be called. /Gy /Gw: the
# generated clients encode every command of their API and the suite calls a
# handful, so /OPT:REF gets a section per function and variable to drop.
string(JOIN " " CMAKE_CXX_FLAGS_INIT
  /GR- /EHs-c- /Zl /X /Z7 /GS- /Gs1048576 /Gy /Gw
  /D_AMD64_ /DAMD64 /D_WIN64 /DAV_CHECKED=1 /DWIN32_LEAN_AND_MEAN /DNOMINMAX
  /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/km/crt -imsvc/windows-kit/um -imsvc/windows-kit/sdk-shared)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_CXX_FLAGS_${config}_INIT "")
  set(CMAKE_EXE_LINKER_FLAGS_${config}_INIT "")
endforeach()

string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /SUBSYSTEM:CONSOLE /ENTRY:rvktest_entry /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG)
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "")
