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
# The kit's headers pick their architecture from these, as the MSVC runtime
# the toolchain does not link would have defined them.
if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_DEFINES /D_X86_)
else()
  set(AV_GUEST_ARCH_DEFINES /D_AMD64_ /DAMD64 /D_WIN64)
endif()

string(JOIN " " CMAKE_CXX_FLAGS_INIT
  /GR- /EHs-c- /Zl /X /Z7 /GS- /Gy /Gw /O2 -flto=thin
  ${AV_GUEST_ARCH_DEFINES} /DAV_CHECKED=1 /DWIN32_LEAN_AND_MEAN /DNOMINMAX
  /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/km/crt -imsvc/windows-kit/um -imsvc/windows-kit/sdk-shared)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_CXX_FLAGS_${config}_INIT "")
  set(CMAKE_EXE_LINKER_FLAGS_${config}_INIT "")
  set(CMAKE_SHARED_LINKER_FLAGS_${config}_INIT "")
endforeach()

# No C runtime, so each target names its own entry: avtest.exe and
# avvkprobe.exe are two programs, and a guest ICD is a DLL built the same way,
# its exports through a .def (feature 045).
# -stdcall-fixup on x86: src/um/imports names each function undecorated, as
# its DLL exports it, and llvm-dlltool makes the import library from that; the
# compiler references a __stdcall import as _Name@N, which the fixup resolves
# to the undecorated symbol and imports by the undecorated name.
if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_LINK -stdcall-fixup)
else()
  set(AV_GUEST_ARCH_LINK "")
endif()
# /Brepro and /PDBALTPATH:%_PDB% make the image depend on its source alone:
# no link time, a PDB GUID from the content, and the PDB named without the
# build directory. The package's package.sha256 is then the same for the
# same source, and LSW offers a guest a driver update only when one changed.
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /SUBSYSTEM:CONSOLE /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%
  ${AV_GUEST_ARCH_LINK})
string(JOIN " " CMAKE_SHARED_LINKER_FLAGS_INIT
  /DLL /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%
  ${AV_GUEST_ARCH_LINK})
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "")
