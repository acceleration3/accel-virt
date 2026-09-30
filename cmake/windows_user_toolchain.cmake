# Cross-compiles rvktest.exe, the in-guest suite, and the user-mode drivers for
# Windows user mode with clang-cl and lld-link against the kit's um header
# closure. No C runtime: each program has its own entry point and imports only
# what src/um/imports names from gdi32, kernel32 and ntdll.
#
# AV_GUEST_ARCH is the process the tree serves: x64, or x86 for the 32-bit
# drivers a WoW64 process loads through the adapter's *Wow registry values.
set(CMAKE_SYSTEM_NAME Windows)
if(NOT AV_GUEST_ARCH)
  set(AV_GUEST_ARCH x64)
endif()
if(AV_GUEST_ARCH STREQUAL "x86")
  set(CMAKE_SYSTEM_PROCESSOR X86)
  set(av_guest_target i686-pc-windows-msvc)
elseif(AV_GUEST_ARCH STREQUAL "x64")
  set(CMAKE_SYSTEM_PROCESSOR AMD64)
  set(av_guest_target x86_64-pc-windows-msvc)
else()
  message(FATAL_ERROR "AV_GUEST_ARCH is x64 or x86, not ${AV_GUEST_ARCH}")
endif()

set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER_TARGET ${av_guest_target})
# C is here for one thing only: the Khronos OpenCL loader, cross-built as a
# guest test artifact (feature 052).
set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET ${av_guest_target})
# No C runtime is linked, so CMake must not choose one: its default
# MultiThreadedDebugDLL puts /MDd on every compile, which defines _DLL, which
# makes the kit's crtdefs.h declare every C library function
# __declspec(dllimport). A guest binary that then calls one - only the
# vendored OpenCL loader does - gets an __imp_ pointer lld-link resolves to
# nothing, and the call jumps to the image base at run time.
set(CMAKE_MSVC_RUNTIME_LIBRARY "")

set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

# Compiler detection links a test program, and there is no C runtime to link.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AV_WINDOWS_KIT_DIR AV_GUEST_ARCH)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_LIST_DIR}/windows_user_flags.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C ${CMAKE_CURRENT_LIST_DIR}/windows_user_flags_c.cmake)
