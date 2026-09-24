# Replaces every flag the MSVC platform module chose. Loaded through
# CMAKE_USER_MAKE_RULES_OVERRIDE_CXX by windows_user_toolchain.cmake.
if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

# As windows_kernel_flags.cmake, without /kernel: /X keeps only the kit
# visible, the overlay makes it case-insensitive. /GS-: no C runtime seeds a
# cookie. Stack probes stay on at the default page: src/um/stack_probe.cpp
# provides the __chkstk they call, which every target links. /Gy /Gw: the
# generated clients encode every command of their API and the suite calls a
# handful, so /OPT:REF gets a section per function and variable to drop.
string(JOIN " " CMAKE_CXX_FLAGS_INIT
  /GR- /EHs-c- /Zl /X /Z7 /GS- /Gy /Gw /O2
  /D_AMD64_ /DAMD64 /D_WIN64 /DAV_CHECKED=1 /DWIN32_LEAN_AND_MEAN /DNOMINMAX
  /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/km/crt -imsvc/windows-kit/um -imsvc/windows-kit/sdk-shared)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_CXX_FLAGS_${config}_INIT "")
  set(CMAKE_EXE_LINKER_FLAGS_${config}_INIT "")
  set(CMAKE_SHARED_LINKER_FLAGS_${config}_INIT "")
endforeach()

# No C runtime, so each target names its own entry: rvktest.exe and
# rvkvkprobe.exe are two programs, and a guest ICD is a DLL built the same way,
# its exports through a .def (feature 045).
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /SUBSYSTEM:CONSOLE /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG)
string(JOIN " " CMAKE_SHARED_LINKER_FLAGS_INIT
  /DLL /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG)
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "")
