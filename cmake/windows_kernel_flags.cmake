# Replaces every flag the MSVC platform module chose for user mode. Loaded
# through CMAKE_USER_MAKE_RULES_OVERRIDE_CXX by windows_kernel_toolchain.cmake.
if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

# /X drops every system include directory, so only the kit is visible. The
# include directories are virtual paths inside the case-insensitive overlay,
# and use the joined -imsvc<dir> form because CMake collapses repeated option
# words ("-imsvc a -imsvc b" becomes "-imsvc a b"). Under clang-cl -Wall means
# -Weverything, hence /W4 plus the host build's explicit list.
string(JOIN " " CMAKE_CXX_FLAGS_INIT
  /kernel /GR- /EHs-c- /Zl /X /Z7 /O2
  /D_AMD64_ /DAMD64 /D_WIN64 /DAV_CHECKED=1
  /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/km/crt -imsvc/windows-kit/km
  -imsvc/windows-kit/wdk-shared -imsvc/windows-kit/sdk-shared)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_CXX_FLAGS_${config}_INIT "")
  set(CMAKE_EXE_LINKER_FLAGS_${config}_INIT "")
endforeach()

# CMake links every executable with /subsystem:console after these flags,
# which overrides /SUBSYSTEM:NATIVE. GsDriverEntry (BufferOverflowK.lib) seeds
# the /GS cookie, then calls DriverEntry. lld-link marks every executable
# terminal-server aware even with /DRIVER, and ntoskrnl's
# MiObtainSectionForDriver refuses a native image carrying that bit with
# STATUS_CONFLICTING_ADDRESSES before DriverEntry runs: Code 39 in the guest.
set(CMAKE_CXX_CREATE_CONSOLE_EXE "")
# /Brepro and /PDBALTPATH:%_PDB% make avgpu.sys depend on its source alone:
# no link time, a PDB GUID from the content, and the PDB named without the
# build directory. The package's package.sha256 is then the same for the
# same source, and LSW offers a guest a driver update only when one changed.
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /DRIVER /SUBSYSTEM:NATIVE /ENTRY:GsDriverEntry /NODEFAULTLIB /MANIFEST:NO
  /TSAWARE:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%)

set(AV_WINDOWS_KIT_LIBRARIES ntoskrnl.lib hal.lib BufferOverflowK.lib displib.lib)
list(TRANSFORM AV_WINDOWS_KIT_LIBRARIES PREPEND "\"${AV_WINDOWS_KIT_DIR}/lib/x64/")
list(TRANSFORM AV_WINDOWS_KIT_LIBRARIES APPEND "\"")
list(JOIN AV_WINDOWS_KIT_LIBRARIES " " CMAKE_CXX_STANDARD_LIBRARIES_INIT)
