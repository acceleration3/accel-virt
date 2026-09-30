# The C half of windows_user_flags.cmake, loaded through
# CMAKE_USER_MAKE_RULES_OVERRIDE_C. Only one thing in this build is C: the
# Khronos OpenCL ICD loader, cross-built as a guest test artifact (feature 052).
# The flags are the C++ ones without what only C++ has, and without /W4 /WX:
# this is somebody else's source, pinned by hash, and warnings in it are not
# this repository's to fix.
if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_DEFINES /D_X86_)
else()
  set(AV_GUEST_ARCH_DEFINES /D_AMD64_ /DAMD64 /D_WIN64)
endif()

string(JOIN " " CMAKE_C_FLAGS_INIT
  /Zl /X /Z7 /GS- /Gy /Gw
  ${AV_GUEST_ARCH_DEFINES} /DWIN32_LEAN_AND_MEAN /DNOMINMAX
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  # km/crt last, and as a system path: windows.h and the intrinsics headers
  # reach for excpt.h and malloc.h from it, while the handful of C library
  # names the loader calls come from src/um/opencl_loader/compat, which is a
  # -I path and so is searched first.
  -imsvc/windows-kit/um -imsvc/windows-kit/sdk-shared -imsvc/windows-kit/km/crt)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_C_FLAGS_${config}_INIT "")
endforeach()
set(CMAKE_C_STANDARD_LIBRARIES_INIT "")

# The linker flags again, identical to windows_user_flags.cmake's. CMake loads
# one override per language and each language's platform module then appends to
# the shared linker-flag variables, so whichever language is enabled last has
# the last word: both overrides have to say the same thing or a C++ target
# silently loses /NODEFAULTLIB and asks for msvcprtd.lib.
# -stdcall-fixup on x86: src/um/imports names each function undecorated, as
# its DLL exports it, and llvm-dlltool makes the import library from that; the
# compiler references a __stdcall import as _Name@N, which the fixup resolves
# to the undecorated symbol and imports by the undecorated name.
if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_LINK -stdcall-fixup)
else()
  set(AV_GUEST_ARCH_LINK "")
endif()
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /SUBSYSTEM:CONSOLE /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG ${AV_GUEST_ARCH_LINK})
string(JOIN " " CMAKE_SHARED_LINKER_FLAGS_INIT
  /DLL /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG ${AV_GUEST_ARCH_LINK})
